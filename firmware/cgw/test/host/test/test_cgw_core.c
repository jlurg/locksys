/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/*
 * cgw_core integration tests over the port fakes: start-up, handshake with the vectors of
 * interfaces/vectors/app_session_v1.json, window press supervision, DCU loss, pairing, factory
 * reset and protocol violations.
 */

#include <string.h>

#include "cgw_core/cgw_core.h"
#include "cgw_proto/cgw_proto.h"
#include "fake_ports.h"
#include "ls_dtc_gen.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"
#include "soft_crypto.h"
#include "unity.h"

TEST_SOURCE_FILE("cgw_core_app.c")
TEST_SOURCE_FILE("cgw_session.c")
TEST_SOURCE_FILE("cgw_arbiter.c")
TEST_SOURCE_FILE("cgw_vstate.c")
TEST_SOURCE_FILE("cgw_health.c")
TEST_SOURCE_FILE("cgw_pairing.c")
TEST_SOURCE_FILE("cgw_proto.c")
TEST_SOURCE_FILE("locksys_cgw.c")
TEST_SOURCE_FILE("locksys_app.pb.c")
TEST_SOURCE_FILE("pb_common.c")
TEST_SOURCE_FILE("pb_decode.c")
TEST_SOURCE_FILE("pb_encode.c")
TEST_SOURCE_FILE("ls_e2e.c")
TEST_SOURCE_FILE("ls_crc8.c")

#define CONN ((cgw_conn_t)0x0701u)

static const char *const V_HANDSHAKE_RNG =
    "101112131415161718191a1b1c1d1e1f12345678"; /* server_nonce, then session_id */
static const char *const V_CLIENT_AUTH_FRAME =
    "125312510a0208011210303132333435363738393a3b3c3d3e3f1a10202122232425262728292a2b2c2d2e2f2220"
    "1160faa48fbf464cb8cd77f1d0408080f735e4554fbd8c8397120ae618950f992a05302e312e30";
static const char *const V_AUTH_RESULT_FRAME =
    "080112341a32080112204a1567ac13dc8410db9924ea4a6d68f93a9b73e2ad1ca5d3ccca2dedc5f22dc418f8acd1"
    "910120b817286430de021a1010bffe38f4667e06dccd9446aae43d87";
static const char *const V_PING_FRAME = "08021205220308e8071a10e181c70bd5abaf865929121059a9bda3";

static cgw_core_t core;
static cgw_core_identity_t id;
static cgw_pairing_record_t rec;
static cgw_core_event_t ev;
static uint32_t app_counter;

static void dispatch(cgw_core_ev_type_t type, uint32_t t_ms, uint32_t arg)
{
    (void)memset(&ev, 0, sizeof(ev));
    ev.type = (uint8_t)type;
    ev.t_ms = t_ms;
    ev.conn = CONN;
    ev.arg = arg;
    ev.aux = 50u;
    cgw_core_dispatch(&core, &ev);
}

static void can_rx(cgw_vs_msg_t msg, const char *hex, uint32_t t_ms)
{
    (void)memset(&ev, 0, sizeof(ev));
    ev.type = (uint8_t)CGW_EV_CAN_RX;
    ev.t_ms = t_ms;
    ev.arg = (uint32_t)msg;
    ev.len = (uint16_t)fake_hex(hex, ev.data, 8u);
    cgw_core_dispatch(&core, &ev);
}

static void ws_frame_hex(const char *hex, uint32_t t_ms)
{
    (void)memset(&ev, 0, sizeof(ev));
    ev.type = (uint8_t)CGW_EV_WS_FRAME;
    ev.t_ms = t_ms;
    ev.conn = CONN;
    ev.len = (uint16_t)fake_hex(hex, ev.data, sizeof(ev.data));
    cgw_core_dispatch(&core, &ev);
}

/* Seal an APP -> CGW body with K_sess of the session (direction 0x41). */
static void ws_app_body(const locksys_app_v1_Body *body, uint32_t t_ms)
{
    uint8_t raw[CGW_PROTO_BODY_MAX];
    size_t raw_len = 0u;
    size_t len = 0u;
    cgw_proto_frame_t fr;
    cgw_tag_input_t in;

    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_encode_body(body, raw, sizeof(raw), &raw_len));
    (void)memset(&fr, 0, sizeof(fr));
    app_counter++;
    fr.counter = app_counter;
    (void)memcpy(fr.body, raw, raw_len);
    fr.body_len = (uint16_t)raw_len;
    fr.tag_len = CGW_TAG_LEN;
    in.body = raw;
    in.body_len = raw_len;
    in.counter = app_counter;
    in.dir = CGW_DIR_APP_TO_CGW;
    TEST_ASSERT_EQUAL(
        CGW_OK, cgw_crypto_tag_compute(cgw_session_find(&core.ses, CONN)->k_sess, &in, fr.tag));
    (void)memset(&ev, 0, sizeof(ev));
    ev.type = (uint8_t)CGW_EV_WS_FRAME;
    ev.t_ms = t_ms;
    ev.conn = CONN;
    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_encode_frame(&fr, ev.data, sizeof(ev.data), &len));
    ev.len = (uint16_t)len;
    cgw_core_dispatch(&core, &ev);
}

static locksys_app_v1_Body sent_body(uint32_t idx, uint32_t *counter)
{
    cgw_proto_frame_t fr;
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    TEST_ASSERT_TRUE(idx < fake.link_count);
    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_decode_frame(fake.link[idx].data, fake.link[idx].len, &fr));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_decode_body(fr.body, fr.body_len, &body));
    *counter = fr.counter;
    return body;
}

static bool sent_ack(uint32_t from, uint8_t kind, uint8_t result)
{
    bool found = false;
    uint32_t c;

    for (uint32_t i = from; i < fake.link_count; i++)
    {
        locksys_app_v1_Body b = sent_body(i, &c);

        found = found || ((b.which_msg == locksys_app_v1_Body_command_ack_tag) &&
                          ((uint8_t)b.msg.command_ack.kind == kind) &&
                          ((uint8_t)b.msg.command_ack.result == result));
    }
    return found;
}

static bool sent_notice(uint32_t from, uint32_t code)
{
    bool found = false;
    uint32_t c;

    for (uint32_t i = from; i < fake.link_count; i++)
    {
        locksys_app_v1_Body b = sent_body(i, &c);

        found = found ||
                ((b.which_msg == locksys_app_v1_Body_notice_tag) && (b.msg.notice.code == code));
    }
    return found;
}

static void bring_up(void)
{
    dispatch(CGW_EV_AP_STARTED, 10u, 0u);
    dispatch(CGW_EV_CAN_BUS, 20u, CGW_BUS_ACTIVE);
    can_rx(CGW_VS_NODE_STS, "fb20017801000700", 30u);
    can_rx(CGW_VS_DOOR_STS, "35302a0200000000", 30u);
    can_rx(CGW_VS_WIN_STS, "9a25ff0004070000", 30u);
}

static void handshake(uint32_t t_ms)
{
    uint8_t rnd[20];

    (void)fake_hex(V_HANDSHAKE_RNG, rnd, sizeof(rnd));
    fake_rng_script(rnd, sizeof(rnd));
    dispatch(CGW_EV_TCP_OPEN, t_ms, 0u);
    dispatch(CGW_EV_WS_OPEN, t_ms, 0u);
    ws_frame_hex(V_CLIENT_AUTH_FRAME, t_ms);
    app_counter = 0u;
}

static void pong(uint32_t echo_ms, uint32_t t_ms)
{
    locksys_app_v1_Body b = locksys_app_v1_Body_init_zero;

    b.which_msg = locksys_app_v1_Body_pong_tag;
    b.msg.pong.echo_timestamp_ms = echo_ms;
    ws_app_body(&b, t_ms);
}

static void window_move(uint32_t press_id, uint32_t t_ms)
{
    locksys_app_v1_Body b = locksys_app_v1_Body_init_zero;

    b.which_msg = locksys_app_v1_Body_window_move_tag;
    b.msg.window_move.press_id = press_id;
    b.msg.window_move.direction = locksys_app_v1_WindowDirection_WINDOW_DIRECTION_UP;
    ws_app_body(&b, t_ms);
}

void setUp(void)
{
    fake_ports_reset();
    soft_crypto_reset();
    (void)memset(&id, 0, sizeof(id));
    (void)fake_hex("0102030405060708", id.device_id, sizeof(id.device_id));
    (void)fake_hex("7cdfa100aabb", id.ap_mac, sizeof(id.ap_mac));
    (void)strcpy(id.fw_version, "0.1.0-dev+a1b2c3d");
    (void)strcpy(id.ssid, "LockSys-AABB");
    (void)strcpy(id.passphrase, "ABCDEFGHIJKLMNOPQRST");
    id.reset_src = CGW_RST_POWERON;
    (void)memset(&rec, 0, sizeof(rec));
    (void)fake_hex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", rec.k_pair,
                   sizeof(rec.k_pair));
    (void)fake_hex("303132333435363738393a3b3c3d3e3f", rec.client_id, sizeof(rec.client_id));
    rec.generation = 1u;
    cgw_core_init(&core, &id, &rec, 0u);
}

void tearDown(void)
{
}

/* @verifies SWR-CGW-060 */
void test_startup_init_then_normal(void)
{
    dispatch(CGW_EV_TICK, 10u, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_INIT, cgw_core_mode(&core));
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_INIT, fake.node.mode);
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_POWER_ON, fake.node.reset_reason);
    TEST_ASSERT_EQUAL_UINT8(50u, fake.node.heap_free_pct);
    TEST_ASSERT_TRUE(fake.intent_count > 0u);
    TEST_ASSERT_FALSE(fake.intent.active);
    TEST_ASSERT_EQUAL(CGW_IND_STARTING, fake.indication);
    bring_up();
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_NORMAL, cgw_core_mode(&core));
    TEST_ASSERT_EQUAL(CGW_IND_READY, fake.indication);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.trace_events[CGW_TRACE_EV_AP_START]);
}

/* @verifies SWR-CGW-005 */
/* @verifies SWR-CGW-023 */
void test_handshake_matches_vectors_and_pushes_status(void)
{
    uint8_t expect[CGW_WS_FRAME_MAX];
    size_t n;
    uint32_t counter = 0u;
    locksys_app_v1_Body b;

    bring_up();
    handshake(1000u);
    TEST_ASSERT_EQUAL_UINT32(3u, fake.link_count);
    b = sent_body(0u, &counter);
    TEST_ASSERT_EQUAL(locksys_app_v1_Body_server_hello_tag, b.which_msg);
    TEST_ASSERT_EQUAL_UINT32(0u, counter);
    n = fake_hex(V_AUTH_RESULT_FRAME, expect, sizeof(expect));
    TEST_ASSERT_EQUAL_size_t(n, fake.link[1].len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, fake.link[1].data, n);
    n = fake_hex(V_PING_FRAME, expect, sizeof(expect));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, fake.link[2].data, n);
    TEST_ASSERT_EQUAL_HEX32(CONN, fake.authenticated);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.trace_events[CGW_TRACE_EV_SESSION_AUTH]);
    TEST_ASSERT_EQUAL(CGW_IND_CONNECTED, fake.indication);

    dispatch(CGW_EV_TICK, 1010u, 0u);
    b = sent_body(3u, &counter);
    TEST_ASSERT_EQUAL(locksys_app_v1_Body_status_update_tag, b.which_msg);
    TEST_ASSERT_EQUAL_UINT32(1u, b.msg.status_update.seq);
    TEST_ASSERT_TRUE(b.msg.status_update.dcu_alive);
    TEST_ASSERT_EQUAL(locksys_app_v1_NodeMode_NODE_MODE_NORMAL, b.msg.status_update.cgw_mode);
    TEST_ASSERT_EQUAL(CGW_PRIO_STATUS, fake.link[3].prio);
}

/* @verifies SWR-CGW-020 */
/* @verifies SWR-CGW-022 */
void test_press_keep_alive_timeout_sends_second_ack(void)
{
    uint32_t mark;

    bring_up();
    handshake(1000u);
    pong(1000u, 1020u);
    mark = fake.link_count;
    window_move(1u, 1100u);
    TEST_ASSERT_TRUE(sent_ack(mark, LS_COMMAND_KIND_WINDOW, LS_COMMAND_RESULT_ACCEPTED));
    TEST_ASSERT_TRUE(fake.intent.active);
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_REQUEST_UP, fake.intent.dir);
    TEST_ASSERT_EQUAL_UINT32(1100u, fake.intent.t_ka_ms);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.trace_events[CGW_TRACE_EV_WINDOW_MOVE]);
    window_move(1u, 1200u);
    TEST_ASSERT_EQUAL_UINT32(1200u, fake.intent.t_ka_ms);
    mark = fake.link_count;
    dispatch(CGW_EV_TICK, 1200u + LS_T_CGW_KA_TO_MS + 1u, 0u);
    TEST_ASSERT_TRUE(fake.intent.latched);
    TEST_ASSERT_TRUE(fake.trace_level[CGW_TRACE_STOP_PENDING]);
    TEST_ASSERT_TRUE(sent_ack(mark, LS_COMMAND_KIND_WINDOW, LS_COMMAND_RESULT_FAILED_TIMEOUT));
    TEST_ASSERT_TRUE(sent_notice(mark, LS_NOTICE_WINDOW_STOP_KA_TIMEOUT));
}

/* @verifies SWR-CGW-024 */
void test_window_stop_ends_press(void)
{
    locksys_app_v1_Body b = locksys_app_v1_Body_init_zero;

    bring_up();
    handshake(1000u);
    pong(1000u, 1020u);
    window_move(1u, 1100u);
    b.which_msg = locksys_app_v1_Body_window_stop_tag;
    b.msg.window_stop.press_id = 1u;
    ws_app_body(&b, 1150u);
    TEST_ASSERT_FALSE(fake.intent.active);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.trace_events[CGW_TRACE_EV_WINDOW_STOP]);
    TEST_ASSERT_TRUE(fake.trace_level[CGW_TRACE_STOP_PENDING]);
}

/* @verifies SWR-CGW-049 */
void test_dcu_lost_latches_comm_and_degrades(void)
{
    uint32_t mark;

    bring_up();
    handshake(1000u);
    pong(1000u, 1020u);
    window_move(1u, 1100u);
    mark = fake.link_count;
    dispatch(CGW_EV_RX_TIMEOUT, 1150u, CGW_VS_NODE_STS);
    TEST_ASSERT_TRUE(fake.intent.latched);
    TEST_ASSERT_TRUE(sent_ack(mark, LS_COMMAND_KIND_WINDOW, LS_COMMAND_RESULT_FAILED_COMM));
    TEST_ASSERT_TRUE(sent_notice(mark, LS_NOTICE_WINDOW_STOP_COMM));
    TEST_ASSERT_TRUE(sent_notice(mark, LS_DTC_U1B00_87));
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_DEGRADED, cgw_core_mode(&core));
}

void test_bus_off_and_door_command(void)
{
    locksys_app_v1_Body b = locksys_app_v1_Body_init_zero;
    uint32_t mark;

    bring_up();
    handshake(1000u);
    mark = fake.link_count;
    b.which_msg = locksys_app_v1_Body_door_command_tag;
    b.msg.door_command.request_id = 1u;
    b.msg.door_command.action = locksys_app_v1_DoorAction_DOOR_ACTION_UNLOCK;
    ws_app_body(&b, 1100u);
    TEST_ASSERT_TRUE(sent_ack(mark, LS_COMMAND_KIND_DOOR, LS_COMMAND_RESULT_ACCEPTED));
    TEST_ASSERT_EQUAL_UINT32(1u, fake.door_count);
    TEST_ASSERT_EQUAL_UINT8(LS_DOOR_REQUEST_UNLOCK, fake.door_request);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.trace_events[CGW_TRACE_EV_DOOR_COMMAND]);
    mark = fake.link_count;
    dispatch(CGW_EV_CAN_BUS, 1200u, CGW_BUS_OFF);
    TEST_ASSERT_TRUE(sent_notice(mark, LS_DTC_U1B01_88));
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_DEGRADED, cgw_core_mode(&core));
    dispatch(CGW_EV_BUSOFF_HEALED, 1300u, 0u);
    dispatch(CGW_EV_E2E_ERROR, 1300u, 1u);
    TEST_ASSERT_EQUAL_HEX8(0u, core.health.status[CGW_DTC_BUS_OFF] & CGW_DTC_ST_TEST_FAILED);
    TEST_ASSERT_NOT_EQUAL(0u, core.health.status[CGW_DTC_E2E_CRC] & CGW_DTC_ST_TEST_FAILED);
}

/* @verifies SWR-CGW-016 */
/* @verifies SWR-CGW-018 */
void test_pairing_window_and_commit_with_pending_key(void)
{
    uint8_t rnd[20];
    uint32_t mark;

    cgw_core_init(&core, &id, NULL, 0u);
    bring_up();
    fake_rng_script(rec.k_pair, sizeof(rec.k_pair));
    dispatch(CGW_EV_PAIR_REQ, 500u, 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.qr_count);
    TEST_ASSERT_EQUAL_INT(
        0, strncmp(fake.qr, "locksys://pair?v=1&id=0102030405060708&s=LockSys-AABB", 52));
    TEST_ASSERT_EQUAL(CGW_IND_PAIRING, fake.indication);
    TEST_ASSERT_EQUAL_UINT8(LS_APP_LINK_STATE_PAIRING, fake.node.app_link);
    (void)fake_hex(V_HANDSHAKE_RNG, rnd, sizeof(rnd));
    fake_rng_script(rnd, sizeof(rnd));
    dispatch(CGW_EV_TCP_OPEN, 1000u, 0u);
    dispatch(CGW_EV_WS_OPEN, 1000u, 0u);
    ws_frame_hex(V_CLIENT_AUTH_FRAME, 1000u);
    app_counter = 0u;
    TEST_ASSERT_TRUE(cgw_session_find(&core.ses, CONN)->pending_key);
    mark = fake.link_count;
    pong(1000u, 1010u);
    TEST_ASSERT_TRUE(fake.ks_has_rec);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.ks_rec.generation);
    TEST_ASSERT_TRUE(core.paired);
    TEST_ASSERT_FALSE(cgw_pairing_is_open(&core.pair));
    TEST_ASSERT_TRUE(sent_notice(mark, LS_NOTICE_PAIRING_COMMITTED));
}

void test_unpaired_client_closed_4005(void)
{
    cgw_core_init(&core, &id, NULL, 0u);
    bring_up();
    handshake(1000u);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.close_count);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_PAIRING_REQUIRED, fake.close_code[0]);
}

/* @verifies SWR-CGW-019 */
void test_factory_reset(void)
{
    bring_up();
    dispatch(CGW_EV_FACTORY_RESET, 100u, 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.ks_erase);
    TEST_ASSERT_TRUE(fake.ks_has_pass);
    TEST_ASSERT_EQUAL_size_t(CGW_PASSPHRASE_LEN, strlen(fake.ks_pass));
    TEST_ASSERT_EQUAL_UINT32(1u, fake.restarts);
    TEST_ASSERT_FALSE(core.paired);
}

/* @verifies SWR-CGW-006 */
void test_violations_close_1008_and_latch(void)
{
    static const char *const garbage = "ffffff";
    locksys_app_v1_Body b = locksys_app_v1_Body_init_zero;

    bring_up();
    handshake(1000u);
    pong(1000u, 1020u);
    window_move(1u, 1100u);
    /* A CGW -> APP member sent by the APP is a wrong-direction message. */
    b.which_msg = locksys_app_v1_Body_status_update_tag;
    ws_app_body(&b, 1150u);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_POLICY, fake.close_code[fake.close_count - 1u]);
    TEST_ASSERT_TRUE(fake.intent.latched);
    TEST_ASSERT_EQUAL_UINT32(1u, fake.trace_events[CGW_TRACE_EV_SESSION_CLOSED]);
    dispatch(CGW_EV_WS_CLOSE, 1160u, 0u);
    TEST_ASSERT_NULL(cgw_session_find(&core.ses, CONN));
    /* Garbage before authentication counts as a failure. */
    dispatch(CGW_EV_TCP_OPEN, 1200u, 0u);
    dispatch(CGW_EV_WS_OPEN, 1200u, 0u);
    ws_frame_hex(garbage, 1210u);
    TEST_ASSERT_EQUAL_UINT32(1u, core.ses.stats.auth_failures);
}

void test_station_leave_closes_controller(void)
{
    bring_up();
    handshake(1000u);
    fake.peer_conn = CONN;
    dispatch(CGW_EV_STA_LEAVE, 1100u, 0x0104A8C0u);
    TEST_ASSERT_EQUAL_HEX32(CONN, fake.closed[fake.close_count - 1u]);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_NONE, fake.close_code[fake.close_count - 1u]);
}
