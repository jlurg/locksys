/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/*
 * cgw_com host tests: CGW_WinCmd Req/HoldAge, schedule, E2E protection, reception, RX timeouts
 * and bus-off policy. Frame bytes are copied from interfaces/vectors/e2e_v1.json.
 */

#include <string.h>

#include "cgw_com/cgw_com.h"
#include "fake_ports.h"
#include "ls_can_matrix_gen.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"
#include "unity.h"

TEST_SOURCE_FILE("ls_e2e.c")
TEST_SOURCE_FILE("ls_crc8.c")
TEST_SOURCE_FILE("locksys_cgw.c")

static cgw_com_t com;
static cgw_com_tick_out_t out;

static void expect_frame(uint32_t idx, uint16_t id, const char *hex)
{
    uint8_t expect[CGW_CAN_DLC_MAX];
    size_t n = fake_hex(hex, expect, sizeof(expect));

    TEST_ASSERT_TRUE(idx < fake.can_count);
    TEST_ASSERT_EQUAL_HEX16(id, fake.can[idx].id);
    TEST_ASSERT_EQUAL_UINT8(n, fake.can[idx].dlc);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, fake.can[idx].data, n);
}

static cgw_com_rx_result_t rx_hex(uint16_t id, const char *hex, uint32_t now_ms)
{
    uint8_t data[CGW_CAN_DLC_MAX];
    size_t n = fake_hex(hex, data, sizeof(data));

    return cgw_com_on_rx(&com, id, data, (uint8_t)n, now_ms);
}

static void set_intent(bool active, uint8_t dir, uint8_t press_id, uint32_t t_ka_ms)
{
    cgw_win_intent_t in = {t_ka_ms, dir, press_id, active, false};

    cgw_com_set_intent(&com, &in);
}

void setUp(void)
{
    const cgw_com_version_t ver = {0x0abcdefu, 0u, 1u, 0u, LS_BUILD_TYPE_DEV, false};

    fake_ports_reset();
    cgw_com_init(&com, &ver, 0u);
}

void tearDown(void)
{
}

/* @verifies SWR-CGW-041 */
void test_startup_win_cmd_is_stop_press_0_hold_255(void)
{
    uint32_t wait = cgw_com_tick(&com, 0u, &out);

    TEST_ASSERT_EQUAL_UINT32(1u, fake.can_count);
    expect_frame(0u, LS_CAN_CGW_WIN_CMD_ID, "9b0000ff");
    TEST_ASSERT_EQUAL_UINT32(7u, wait);
    (void)cgw_com_tick(&com, 7u, &out);
    TEST_ASSERT_EQUAL_HEX16(LS_CAN_CGW_NODE_STS_ID, fake.can[1].id);
    (void)cgw_com_tick(&com, 13u, &out);
    TEST_ASSERT_EQUAL_HEX16(LS_CAN_CGW_VERSION_ID, fake.can[2].id);
    (void)cgw_com_tick(&com, 20u, &out);
    TEST_ASSERT_EQUAL_HEX16(LS_CAN_CGW_WIN_CMD_ID, fake.can[3].id);
    TEST_ASSERT_EQUAL_UINT8(1u, fake.can[3].data[1] & 0x0Fu);
}

/* @verifies SWR-CGW-041 */
void test_req_and_hold_age_computed_at_send_time(void)
{
    uint8_t req = 0xFFu;
    uint8_t hold = 0u;

    set_intent(true, LS_WINDOW_REQUEST_UP, 7u, 1000u);
    cgw_com_win_cmd_values(&com, 1050u, &req, &hold);
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_REQUEST_UP, req);
    TEST_ASSERT_EQUAL_UINT8(5u, hold);
    cgw_com_win_cmd_values(&com, 1000u + LS_T_CGW_KA_TO_MS, &req, &hold);
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_REQUEST_UP, req);
    cgw_com_win_cmd_values(&com, 1001u + LS_T_CGW_KA_TO_MS, &req, &hold);
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_REQUEST_STOP, req);
    TEST_ASSERT_EQUAL_UINT8(35u, hold);
    cgw_com_win_cmd_values(&com, 1000u + 3000u, &req, &hold);
    TEST_ASSERT_EQUAL_UINT8(255u, hold);
    com.intent.latched = true;
    cgw_com_win_cmd_values(&com, 1050u, &req, &hold);
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_REQUEST_STOP, req);
    TEST_ASSERT_EQUAL_UINT8(255u, hold);
    set_intent(false, LS_WINDOW_REQUEST_STOP, 7u, 1000u);
    cgw_com_win_cmd_values(&com, 1100u, &req, &hold);
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_REQUEST_STOP, req);
    TEST_ASSERT_EQUAL_UINT8(10u, hold);
}

/* @verifies SWR-CGW-042 */
void test_win_cmd_up_and_stop_match_vectors(void)
{
    com.win_tx.counter = 3u;
    set_intent(true, LS_WINDOW_REQUEST_UP, 7u, 950u);
    com.win_next_ms = 1000u;
    com.node_next_ms = 5000u;
    com.ver_next_ms = 5000u;
    (void)cgw_com_tick(&com, 1000u, &out);
    expect_frame(0u, LS_CAN_CGW_WIN_CMD_ID, "5b130705");
    set_intent(true, LS_WINDOW_REQUEST_STOP, 7u, 1010u);
    (void)cgw_com_tick(&com, 1010u, &out);
    expect_frame(1u, LS_CAN_CGW_WIN_CMD_ID, "a0040700");
}

/* @verifies SWR-CGW-024 */
void test_req_change_sent_after_minimum_gap(void)
{
    com.node_next_ms = 1000u;
    com.ver_next_ms = 1000u;
    (void)cgw_com_tick(&com, 0u, &out);
    fake.trace_level[CGW_TRACE_STOP_PENDING] = true;
    set_intent(true, LS_WINDOW_REQUEST_DOWN, 3u, 2u);
    (void)cgw_com_tick(&com, 2u, &out);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.can_count);
    (void)cgw_com_tick(&com, 5u, &out);
    TEST_ASSERT_EQUAL_UINT32(2u, fake.can_count);
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_REQUEST_DOWN, (fake.can[1].data[1] >> 4) & 0x03u);
    set_intent(false, LS_WINDOW_REQUEST_STOP, 3u, 2u);
    (void)cgw_com_tick(&com, 10u, &out);
    TEST_ASSERT_EQUAL_UINT32(3u, fake.can_count);
    TEST_ASSERT_FALSE(fake.trace_level[CGW_TRACE_STOP_PENDING]);
    TEST_ASSERT_EQUAL_UINT32(3u, fake.trace_pulses[CGW_TRACE_WINCMD_TX]);
}

/* @verifies SWR-CGW-042 */
void test_alive_counter_advances_only_on_enqueue(void)
{
    fake.can_rc = CGW_E_FULL;
    (void)cgw_com_tick(&com, 0u, &out);
    TEST_ASSERT_EQUAL_UINT8(0u, com.win_tx.counter);
    TEST_ASSERT_EQUAL_UINT32(1u, com.stats.tx_skipped);
    fake.can_rc = CGW_OK;
    (void)cgw_com_tick(&com, 20u, &out);
    expect_frame(0u, LS_CAN_CGW_WIN_CMD_ID, "9b0000ff");
}

void test_door_cmd_three_transmissions(void)
{
    com.win_next_ms = 1000u;
    com.node_next_ms = 1000u;
    com.ver_next_ms = 1000u;
    cgw_com_request_door(&com, 0x2Au, LS_DOOR_REQUEST_LOCK, 100u);
    TEST_ASSERT_EQUAL_UINT32(LS_CAN_CGW_DOOR_CMD_MIN_GAP_MS, cgw_com_tick(&com, 100u, &out));
    expect_frame(0u, LS_CAN_CGW_DOOR_CMD_ID, "4e102a00");
    (void)cgw_com_tick(&com, 110u, &out);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.can_count);
    (void)cgw_com_tick(&com, 120u, &out);
    (void)cgw_com_tick(&com, 140u, &out);
    (void)cgw_com_tick(&com, 160u, &out);
    TEST_ASSERT_EQUAL_UINT32(3u, fake.can_count);
    TEST_ASSERT_EQUAL_UINT8(2u, fake.can[2].data[1] & 0x0Fu);
}

/* @verifies SWR-CGW-047 */
void test_node_sts_matches_vector(void)
{
    const cgw_node_info_t info = {
        LS_NODE_MODE_NORMAL, LS_APP_LINK_STATE_AUTHENTICATED, 1u, LS_RESET_REASON_POWER_ON, 0u, 0u};

    com.win_next_ms = 1000u;
    cgw_com_set_node_info(&com, &info);
    (void)cgw_com_tick(&com, 7u, &out);
    expect_frame(0u, LS_CAN_CGW_NODE_STS_ID, "0b20010601000000");
}

/* @verifies SWR-CGW-044 */
void test_rx_delivered_after_n_ok_frames(void)
{
    cgw_com_rx_result_t r = rx_hex(LS_CAN_DCU_WIN_STS_ID, "9a25ff0004070002", 10u);

    TEST_ASSERT_EQUAL(CGW_COM_RX_WIN_STS, r.msg);
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_OK, r.status);
    TEST_ASSERT_FALSE(r.deliver);
    r = rx_hex(LS_CAN_DCU_WIN_STS_ID, "7866ff0700070108", 60u);
    TEST_ASSERT_TRUE(r.deliver);
    r = rx_hex(LS_CAN_DCU_WIN_STS_ID, "7966ff0700070108", 70u);
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_CRC_ERROR, r.status);
    r = rx_hex(0x123u, "00", 80u);
    TEST_ASSERT_EQUAL(CGW_COM_RX_COUNT, r.msg);
    TEST_ASSERT_EQUAL_UINT32(1u, com.stats.unknown_id);
    r = rx_hex(0x590u, "0001000100000000", 90u);
    TEST_ASSERT_TRUE(r.deliver);
    r = rx_hex(0x590u, "00", 95u);
    TEST_ASSERT_FALSE(r.deliver);
}

/* @verifies SWR-CGW-044 */
void test_rx_timeout_reported_once_and_cleared_by_ok_frame(void)
{
    (void)cgw_com_tick(&com, LS_CAN_DCU_WIN_STS_RX_TIMEOUT_MS, &out);
    TEST_ASSERT_EQUAL_UINT8(0u, out.timeouts);
    (void)cgw_com_tick(&com, LS_CAN_DCU_WIN_STS_RX_TIMEOUT_MS + 1u, &out);
    TEST_ASSERT_EQUAL_UINT8(1u << CGW_COM_RX_WIN_STS, out.timeouts & (1u << CGW_COM_RX_WIN_STS));
    (void)cgw_com_tick(&com, 300u, &out);
    TEST_ASSERT_EQUAL_UINT8(0u, out.timeouts & (1u << CGW_COM_RX_WIN_STS));
    (void)rx_hex(LS_CAN_DCU_WIN_STS_ID, "9a25ff0004070002", 310u);
    TEST_ASSERT_FALSE(com.rx[CGW_COM_RX_WIN_STS].timed_out);
}

/* @verifies SWR-CGW-046 */
void test_bus_off_rewrites_slots_and_recovers(void)
{
    uint32_t t_ms;

    set_intent(true, LS_WINDOW_REQUEST_UP, 7u, 0u);
    (void)cgw_com_tick(&com, 0u, &out);
    fake.can_busy = 2u;
    cgw_com_on_bus_state(&com, CGW_BUS_OFF, 10u);
    /* Both slots rewritten to STOP, HoldAge 255, new alive counters 1 and 2. */
    TEST_ASSERT_EQUAL_UINT8(0x01u, fake.can_rewritten[0][1] & 0x3Fu);
    TEST_ASSERT_EQUAL_UINT8(0x02u, fake.can_rewritten[1][1] & 0x3Fu);
    TEST_ASSERT_EQUAL_UINT8(7u, fake.can_rewritten[0][2]);
    TEST_ASSERT_EQUAL_UINT8(255u, fake.can_rewritten[1][3]);
    TEST_ASSERT_EQUAL_UINT8(3u, com.win_tx.counter);
    fake.can_count = 0u;
    for (t_ms = 10u; t_ms <= 10u + (LS_N_BUSOFF_FAST * LS_T_BUSOFF_FAST_MS); t_ms += 10u)
    {
        (void)cgw_com_tick(&com, t_ms, &out);
    }
    TEST_ASSERT_EQUAL_UINT32(0u, fake.can_count);
    TEST_ASSERT_EQUAL_UINT32(LS_N_BUSOFF_FAST, fake.can_recover);
    (void)cgw_com_tick(&com, t_ms + LS_T_BUSOFF_FAST_MS, &out);
    TEST_ASSERT_EQUAL_UINT32(LS_N_BUSOFF_FAST, fake.can_recover);
    (void)cgw_com_tick(&com, 10u + (LS_N_BUSOFF_FAST * LS_T_BUSOFF_FAST_MS) + LS_T_BUSOFF_SLOW_MS,
                       &out);
    TEST_ASSERT_EQUAL_UINT32(LS_N_BUSOFF_FAST + 1u, fake.can_recover);

    fake.can_reclaim = 1u;
    cgw_com_on_bus_state(&com, CGW_BUS_ACTIVE, 2000u);
    (void)cgw_com_tick(&com, 2049u, &out);
    TEST_ASSERT_EQUAL_UINT32(0u, com.stats.reclaimed);
    (void)cgw_com_tick(&com, 2050u, &out);
    TEST_ASSERT_EQUAL_UINT32(1u, com.stats.reclaimed);
    TEST_ASSERT_TRUE(fake.can_count > 0u);
    (void)cgw_com_tick(&com, 2000u + LS_T_BUSOFF_HEAL_MS, &out);
    TEST_ASSERT_TRUE(out.busoff_healed);
    TEST_ASSERT_EQUAL_UINT8(0u, com.bus_attempts);
}

void test_rx_msg_lookup(void)
{
    TEST_ASSERT_EQUAL(CGW_COM_RX_WIN_MOTION, cgw_com_rx_msg_of(LS_CAN_DCU_WIN_MOTION_ID));
    TEST_ASSERT_EQUAL(CGW_COM_RX_NODE_STS, cgw_com_rx_msg_of(LS_CAN_DCU_NODE_STS_ID));
    TEST_ASSERT_EQUAL(CGW_COM_RX_COUNT, cgw_com_rx_msg_of(LS_CAN_CGW_WIN_CMD_ID));
}
