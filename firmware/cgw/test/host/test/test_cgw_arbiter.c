/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/* cgw_arbiter host tests: admission, keep-alive supervision, latches, DCU feedback, door. */

#include <string.h>

#include "cgw_arbiter/cgw_arbiter.h"
#include "fake_ports.h"
#include "ls_dtc_gen.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"
#include "unity.h"

static cgw_arbiter_t a;
static cgw_arb_env_t env;
static cgw_arb_out_t out;

static cgw_arb_move_t move(uint32_t press_id, uint8_t dir, uint32_t t_rx_ms)
{
    cgw_arb_move_t mv = {press_id, 0u, t_rx_ms, dir};

    return mv;
}

static void press(uint32_t press_id, uint32_t t_rx_ms)
{
    cgw_arb_move_t mv = move(press_id, LS_WINDOW_DIRECTION_UP, t_rx_ms);

    cgw_arbiter_on_move(&a, &mv, &env, &out);
}

void setUp(void)
{
    static const uint8_t seed[1] = {0x10u};

    fake_ports_reset();
    fake_rng_script(seed, sizeof(seed));
    cgw_arbiter_init(&a);
    cgw_arbiter_session_start(&a);
    (void)memset(&env, 0, sizeof(env));
    env.cgw_mode = LS_NODE_MODE_NORMAL;
    env.dcu_mode = LS_NODE_MODE_NORMAL;
    env.lock_state = LS_DOOR_LOCK_STATE_LOCKED;
    env.dcu_alive = true;
    env.version_ok = true;
    env.win_sts_valid = true;
    env.door_sts_valid = true;
    env.link_ok = true;
}

void tearDown(void)
{
}

/* @verifies SWR-CGW-020 */
void test_first_move_admitted_with_one_ack(void)
{
    cgw_win_intent_t in;

    press(1u, 1000u);
    TEST_ASSERT_TRUE(out.win_ack.valid);
    TEST_ASSERT_EQUAL_UINT32(1u, out.win_ack.ref_id);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_ACCEPTED, out.win_ack.result);
    TEST_ASSERT_TRUE(out.intent_changed);
    cgw_arbiter_intent(&a, &in);
    TEST_ASSERT_TRUE(in.active);
    TEST_ASSERT_FALSE(in.latched);
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_REQUEST_UP, in.dir);
    TEST_ASSERT_EQUAL_UINT8(0x11u, in.can_press_id);
    TEST_ASSERT_EQUAL_UINT32(1000u, in.t_ka_ms);
    /* Keep-alive of the same press: no further acknowledgement. */
    press(1u, 1100u);
    TEST_ASSERT_FALSE(out.win_ack.valid);
    TEST_ASSERT_TRUE(out.intent_changed);
    TEST_ASSERT_EQUAL_UINT32(1100u, a.win.t_ka_ms);
}

/* @verifies SWR-CGW-022 */
void test_keep_alive_timeout_latches_stop(void)
{
    cgw_win_intent_t in;

    press(1u, 1000u);
    cgw_arbiter_tick(&a, 1000u + LS_T_CGW_KA_TO_MS, &env, &out);
    TEST_ASSERT_FALSE(out.win_ack.valid);
    TEST_ASSERT_EQUAL(CGW_ARB_WIN_MOVING, a.win.state);
    cgw_arbiter_tick(&a, 1000u + LS_T_CGW_KA_TO_MS + 1u, &env, &out);
    TEST_ASSERT_TRUE(out.win_ack.valid);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_TIMEOUT, out.win_ack.result);
    TEST_ASSERT_EQUAL_UINT16(LS_NOTICE_WINDOW_STOP_KA_TIMEOUT, out.notice);
    TEST_ASSERT_TRUE(out.stop_decided);
    cgw_arbiter_intent(&a, &in);
    TEST_ASSERT_TRUE(in.latched);
    /* At most one acknowledgement after the first; moves of a latched press are ignored. */
    cgw_arbiter_tick(&a, 3000u, &env, &out);
    TEST_ASSERT_FALSE(out.win_ack.valid);
    press(1u, 3000u);
    TEST_ASSERT_FALSE(out.win_ack.valid);
    TEST_ASSERT_FALSE(out.intent_changed);
    /* Only a newer press_id can move again. */
    press(2u, 3100u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_ACCEPTED, out.win_ack.result);
}

/* @verifies SWR-CGW-021 */
void test_keep_alive_refresh_uses_receipt_time(void)
{
    press(1u, 1000u);
    press(1u, 1000u + LS_T_CGW_KA_TO_MS);
    cgw_arbiter_tick(&a, 1000u + (2u * LS_T_CGW_KA_TO_MS), &env, &out);
    TEST_ASSERT_EQUAL(CGW_ARB_WIN_MOVING, a.win.state);
}

void test_backstop_and_link_quality_latches(void)
{
    press(1u, 0u);
    for (uint32_t t_ms = 100u; t_ms < LS_T_CGW_MAX_RUN_BACKSTOP_MS; t_ms += 100u)
    {
        press(1u, t_ms);
        cgw_arbiter_tick(&a, t_ms, &env, &out);
    }
    press(1u, LS_T_CGW_MAX_RUN_BACKSTOP_MS);
    cgw_arbiter_tick(&a, LS_T_CGW_MAX_RUN_BACKSTOP_MS, &env, &out);
    TEST_ASSERT_EQUAL_UINT16(LS_NOTICE_WINDOW_STOP_BACKSTOP, out.notice);

    press(2u, 9000u);
    env.link_ok = false;
    cgw_arbiter_tick(&a, 9010u, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_LINK_QUALITY, out.win_ack.result);
    TEST_ASSERT_EQUAL_UINT16(LS_NOTICE_WINDOW_STOP_LINK_QUALITY, out.notice);
}

void test_direction_change_latches(void)
{
    cgw_arb_move_t mv = move(1u, LS_WINDOW_DIRECTION_DOWN, 1100u);

    press(1u, 1000u);
    cgw_arbiter_on_move(&a, &mv, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_INVALID, out.win_ack.result);
    TEST_ASSERT_EQUAL_UINT16(LS_NOTICE_WINDOW_STOP_DIR_CHANGE, out.notice);
}

/* @verifies SWR-CGW-020 */
void test_admission_order(void)
{
    cgw_arb_move_t mv;
    uint32_t id = 1u;

    env.cgw_mode = LS_NODE_MODE_INIT;
    press(id++, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_MODE, out.win_ack.result);
    env.cgw_mode = LS_NODE_MODE_NORMAL;
    env.pairing_busy = true;
    press(id++, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_MODE, out.win_ack.result);
    env.pairing_busy = false;
    env.dcu_alive = false;
    press(id++, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_COMM, out.win_ack.result);
    env.dcu_alive = true;
    env.dcu_mode = LS_NODE_MODE_SAFE;
    press(id++, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_MODE, out.win_ack.result);
    env.dcu_mode = LS_NODE_MODE_DEGRADED;
    env.win_inhibit = true;
    press(id++, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_INTERLOCK, out.win_ack.result);
    env.win_inhibit = false;
    env.version_ok = false;
    press(id++, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_VERSION, out.win_ack.result);
    env.version_ok = true;
    env.bus_off = true;
    press(id++, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_COMM, out.win_ack.result);
    env.bus_off = false;
    env.win_sts_valid = false;
    press(id++, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_COMM, out.win_ack.result);
    env.win_sts_valid = true;
    env.link_ok = false;
    press(id++, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_LINK_QUALITY, out.win_ack.result);
    env.link_ok = true;
    mv = move(id++, LS_WINDOW_DIRECTION_STOP, 0u);
    cgw_arbiter_on_move(&a, &mv, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_INVALID, out.win_ack.result);
    mv = move(id++, LS_WINDOW_DIRECTION_UP, 0u);
    mv.hold_ms = LS_T_NEW_PRESS_MAX_MS + 1u;
    cgw_arbiter_on_move(&a, &mv, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_INVALID, out.win_ack.result);
    env.cgw_mode = LS_NODE_MODE_DEGRADED;
    press(id++, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_COMM, out.win_ack.result);
    env.cgw_mode = LS_NODE_MODE_NORMAL;
    press(id, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_ACCEPTED, out.win_ack.result);
    /* A rejected press_id is not acknowledged twice and an older one is ignored. */
    press(id - 1u, 10u);
    TEST_ASSERT_FALSE(out.win_ack.valid);
}

/* @verifies SWR-CGW-026 */
void test_newer_press_releases_old_and_stop_ends_press(void)
{
    uint8_t first;

    press(1u, 0u);
    first = a.win.can_press_id;
    press(2u, 50u);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_ACCEPTED, out.win_ack.result);
    TEST_ASSERT_TRUE(out.stop_decided);
    TEST_ASSERT_NOT_EQUAL(first, a.win.can_press_id);
    cgw_arbiter_on_stop(&a, 1u, &out);
    TEST_ASSERT_FALSE(out.intent_changed);
    cgw_arbiter_on_stop(&a, 2u, &out);
    TEST_ASSERT_TRUE(out.intent_changed);
    TEST_ASSERT_FALSE(out.win_ack.valid);
    TEST_ASSERT_FALSE(cgw_arbiter_press_active(&a));
}

/* @verifies SWR-CGW-025 */
void test_can_press_id_skips_zero_and_echo(void)
{
    cgw_arbiter_on_win_sts(&a, 254u, LS_COMMAND_RESULT_UNSPECIFIED, &out);
    press(1u, 0u);
    TEST_ASSERT_EQUAL_UINT8(255u, a.win.can_press_id);
    cgw_arbiter_on_stop(&a, 1u, &out);
    cgw_arbiter_on_win_sts(&a, 1u, LS_COMMAND_RESULT_OK, &out);
    press(2u, 10u);
    TEST_ASSERT_EQUAL_UINT8(2u, a.win.can_press_id);
}

/* @verifies SWR-CGW-022 */
void test_dcu_rejection_latches_with_its_result(void)
{
    press(1u, 0u);
    cgw_arbiter_on_win_sts(&a, a.win.can_press_id, LS_COMMAND_RESULT_ACCEPTED, &out);
    TEST_ASSERT_FALSE(out.win_ack.valid);
    cgw_arbiter_on_win_sts(&a, (uint8_t)(a.win.can_press_id + 1u),
                           LS_COMMAND_RESULT_FAILED_ACTUATOR, &out);
    TEST_ASSERT_FALSE(out.win_ack.valid);
    cgw_arbiter_on_win_sts(&a, a.win.can_press_id, LS_COMMAND_RESULT_FAILED_ACTUATOR, &out);
    TEST_ASSERT_TRUE(out.win_ack.valid);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_ACTUATOR, out.win_ack.result);
    TEST_ASSERT_EQUAL_UINT16(LS_NOTICE_WINDOW_STOP_DCU, out.notice);
    cgw_arbiter_on_win_sts(&a, a.win.can_press_id, LS_COMMAND_RESULT_FAILED_ACTUATOR, &out);
    TEST_ASSERT_FALSE(out.win_ack.valid);
}

void test_session_latch_has_no_ack(void)
{
    press(1u, 0u);
    (void)memset(&out, 0, sizeof(out));
    cgw_arbiter_latch(&a, CGW_LATCH_SESSION, &out);
    TEST_ASSERT_FALSE(out.win_ack.valid);
    TEST_ASSERT_EQUAL_UINT16(LS_NOTICE_WINDOW_STOP_SESSION, out.notice);
    /* Latching an already latched press adds nothing to the (caller-cleared) actions. */
    (void)memset(&out, 0, sizeof(out));
    cgw_arbiter_latch(&a, CGW_LATCH_COMM, &out);
    TEST_ASSERT_FALSE(out.stop_decided);
}

/* @verifies SWR-CGW-031 */
/* @verifies SWR-CGW-032 */
void test_door_transaction_completes(void)
{
    cgw_arb_door_cmd_t cmd = {7u, 100u, LS_DOOR_ACTION_LOCK};
    cgw_arb_door_sts_t sts = {150u, 0u, LS_COMMAND_RESULT_ACCEPTED, LS_DOOR_LOCK_STATE_LOCKING};

    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_ACCEPTED, out.door_ack.result);
    TEST_ASSERT_TRUE(out.door_can_valid);
    TEST_ASSERT_EQUAL_UINT8(LS_DOOR_REQUEST_LOCK, out.door_can_request);
    sts.last_req_id = out.door_can_req_id;
    cgw_arbiter_on_door_sts(&a, &sts, &out);
    TEST_ASSERT_FALSE(out.door_result_valid);
    sts.last_result = LS_COMMAND_RESULT_OK;
    sts.lock_state = LS_DOOR_LOCK_STATE_LOCKED;
    cgw_arbiter_on_door_sts(&a, &sts, &out);
    TEST_ASSERT_TRUE(out.door_result_valid);
    TEST_ASSERT_EQUAL_UINT32(7u, out.door_request_id);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_OK, out.door_result);
    TEST_ASSERT_EQUAL_UINT8(LS_DOOR_LOCK_STATE_LOCKED, out.door_lock_state);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_OK, a.last_door_result);
}

/* @verifies SWR-CGW-030 */
/* @verifies SWR-CGW-033 */
void test_door_busy_repeat_cache_and_timeout(void)
{
    cgw_arb_door_cmd_t cmd = {7u, 100u, LS_DOOR_ACTION_UNLOCK};
    cgw_arb_door_cmd_t other = {8u, 120u, LS_DOOR_ACTION_LOCK};

    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_ACCEPTED, out.door_ack.result);
    TEST_ASSERT_FALSE(out.door_can_valid);
    cgw_arbiter_on_door(&a, &other, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_BUSY, out.door_ack.result);
    cgw_arbiter_tick(&a, 100u + LS_T_CGW_DOOR_RESULT_TO_MS, &env, &out);
    TEST_ASSERT_TRUE(out.door_result_valid);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_TIMEOUT, out.door_result);
    TEST_ASSERT_EQUAL_UINT8(LS_DOOR_LOCK_STATE_LOCKED, out.door_lock_state);
    cmd.t_rx_ms = 3000u;
    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    TEST_ASSERT_TRUE(out.door_result_valid);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_TIMEOUT, out.door_result);
    /* A new session clears the cache: the same request_id is a new request. */
    cgw_arbiter_session_start(&a);
    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    TEST_ASSERT_TRUE(out.door_can_valid);
}

void test_door_admission_checks(void)
{
    cgw_arb_door_cmd_t cmd = {1u, 0u, 9u};

    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_INVALID, out.door_ack.result);
    cmd.action = LS_DOOR_ACTION_LOCK;
    cmd.request_id = 2u;
    env.door_sts_valid = false;
    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_COMM, out.door_ack.result);
    env.door_sts_valid = true;
    env.door_rate_limited = true;
    cmd.request_id = 3u;
    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_RATE_LIMIT, out.door_ack.result);
    env.door_rate_limited = false;
    env.lock_inhibit = true;
    cmd.request_id = 4u;
    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_INTERLOCK, out.door_ack.result);
    env.lock_inhibit = false;
    env.version_ok = false;
    cmd.request_id = 5u;
    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_VERSION, out.door_ack.result);
    env.version_ok = true;
    env.cgw_mode = LS_NODE_MODE_DEGRADED;
    cmd.request_id = 6u;
    cgw_arbiter_on_door(&a, &cmd, &env, &out);
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_COMM, out.door_ack.result);
    TEST_ASSERT_EQUAL_UINT32(6u, a.last_door_request);
}

void test_stop_reason_mapping(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_UNSPECIFIED,
                            cgw_arbiter_stop_reason_result(LS_WINDOW_STOP_REASON_NONE));
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_OK,
                            cgw_arbiter_stop_reason_result(LS_WINDOW_STOP_REASON_RELEASED));
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_FAILED_ACTUATOR,
                            cgw_arbiter_stop_reason_result(LS_WINDOW_STOP_REASON_DIR_MISMATCH));
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_REJECTED_MODE,
                            cgw_arbiter_stop_reason_result(LS_WINDOW_STOP_REASON_MODE_INHIBIT));
    TEST_ASSERT_EQUAL_UINT8(LS_COMMAND_RESULT_UNSPECIFIED, cgw_arbiter_stop_reason_result(31u));
}
