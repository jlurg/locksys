/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/*
 * cgw_vstate host tests: StatusUpdate mapping and stale rules (LS-SAIC-001 section 8.8), push
 * policy (section 5.5) and version check. DCU frames from interfaces/vectors/e2e_v1.json.
 */

#include <string.h>

#include "cgw_vstate/cgw_vstate.h"
#include "fake_ports.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"
#include "unity.h"

TEST_SOURCE_FILE("locksys_cgw.c")

static cgw_vstate_t vs;
static cgw_vs_status_ctx_t ctx;
static locksys_app_v1_StatusUpdate su;

static void rx(cgw_vs_msg_t msg, const char *hex, uint32_t now_ms)
{
    uint8_t data[8] = {0u};

    (void)fake_hex(hex, data, sizeof(data));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_vstate_on_rx(&vs, msg, data, now_ms));
}

void setUp(void)
{
    cgw_vstate_init(&vs);
    (void)memset(&ctx, 0, sizeof(ctx));
    ctx.seq = 1u;
    ctx.cgw_mode = LS_NODE_MODE_NORMAL;
}

void tearDown(void)
{
}

/* @verifies SWR-CGW-050 */
void test_stale_defaults(void)
{
    cgw_vstate_build_status(&vs, &ctx, 5000u, &su);
    TEST_ASSERT_EQUAL(locksys_app_v1_DoorLockState_DOOR_LOCK_STATE_UNKNOWN, su.door_lock_state);
    TEST_ASSERT_EQUAL(locksys_app_v1_WindowState_WINDOW_STATE_UNKNOWN, su.window_state);
    TEST_ASSERT_EQUAL_UINT32(255u, su.window_position_pct);
    TEST_ASSERT_EQUAL(locksys_app_v1_TempStatus_TEMP_STATUS_UNKNOWN, su.temp_status);
    TEST_ASSERT_EQUAL(locksys_app_v1_NodeMode_NODE_MODE_UNKNOWN, su.dcu_mode);
    TEST_ASSERT_EQUAL(locksys_app_v1_NodeMode_NODE_MODE_NORMAL, su.cgw_mode);
    TEST_ASSERT_FALSE(su.dcu_alive);
    TEST_ASSERT_TRUE(su.window_inhibited);
    TEST_ASSERT_TRUE(su.door_inhibited);
    TEST_ASSERT_EQUAL_UINT32(5000u, su.status_age_ms);
    TEST_ASSERT_EQUAL_UINT32(1u, su.seq);
}

/* @verifies SWR-CGW-050 */
/* @verifies SWR-CGW-052 */
void test_fresh_mapping_from_dcu_frames(void)
{
    rx(CGW_VS_WIN_STS, "7866ff0700070108", 1000u);
    rx(CGW_VS_WIN_MOTION, "0a12a40664b0f5ff", 1000u);
    rx(CGW_VS_DOOR_STS, "35302a0200000000", 950u);
    rx(CGW_VS_TEMP_STS, "901129092a000000", 900u);
    rx(CGW_VS_NODE_STS, "fb20017801000700", 980u);
    cgw_vstate_build_status(&vs, &ctx, 1010u, &su);
    TEST_ASSERT_EQUAL(locksys_app_v1_WindowState_WINDOW_STATE_BLOCKED, su.window_state);
    TEST_ASSERT_EQUAL(locksys_app_v1_WindowStopReason_WINDOW_STOP_REASON_STALL,
                      su.window_stop_reason);
    TEST_ASSERT_EQUAL_HEX32(0x001u, su.window_fault_flags);
    TEST_ASSERT_EQUAL_INT32(1700, su.window_speed_rpm_x10);
    TEST_ASSERT_EQUAL(locksys_app_v1_EncoderStatus_ENCODER_STATUS_OK, su.window_encoder_status);
    TEST_ASSERT_EQUAL(locksys_app_v1_DoorLockState_DOOR_LOCK_STATE_LOCKING, su.door_lock_state);
    TEST_ASSERT_EQUAL_INT32(2345, su.temperature_cdeg);
    TEST_ASSERT_EQUAL(locksys_app_v1_TempStatus_TEMP_STATUS_VALID, su.temp_status);
    TEST_ASSERT_EQUAL(locksys_app_v1_NodeMode_NODE_MODE_NORMAL, su.dcu_mode);
    TEST_ASSERT_EQUAL_UINT32(120u, su.vbat_dv);
    TEST_ASSERT_TRUE(su.dcu_alive);
    TEST_ASSERT_FALSE(su.window_inhibited);
    TEST_ASSERT_EQUAL_UINT32(60u, su.status_age_ms);
    TEST_ASSERT_TRUE(cgw_vstate_version_ok(&vs));

    cgw_vstate_on_timeout(&vs, CGW_VS_TEMP_STS);
    cgw_vstate_on_timeout(&vs, CGW_VS_WIN_MOTION);
    cgw_vstate_build_status(&vs, &ctx, 4000u, &su);
    TEST_ASSERT_EQUAL(locksys_app_v1_TempStatus_TEMP_STATUS_STALE, su.temp_status);
    TEST_ASSERT_EQUAL_INT32(0, su.temperature_cdeg);
    TEST_ASSERT_EQUAL_INT32(0, su.window_speed_rpm_x10);
    TEST_ASSERT_FALSE(cgw_vstate_is_fresh(&vs, CGW_VS_TEMP_STS));
    TEST_ASSERT_FALSE(cgw_vstate_is_fresh(&vs, CGW_VS_MSG_COUNT));
}

/* @verifies SWR-CGW-048 */
void test_version_mismatch_debounced(void)
{
    rx(CGW_VS_NODE_STS, "0020027801000700", 0u);
    rx(CGW_VS_NODE_STS, "0020027801000700", 100u);
    TEST_ASSERT_TRUE(cgw_vstate_version_ok(&vs));
    rx(CGW_VS_NODE_STS, "0020027801000700", 200u);
    TEST_ASSERT_FALSE(cgw_vstate_version_ok(&vs));
    for (uint32_t i = 0u; i < LS_N_VER_DEBOUNCE; i++)
    {
        rx(CGW_VS_NODE_STS, "fb20017801000700", 300u + i);
    }
    TEST_ASSERT_TRUE(cgw_vstate_version_ok(&vs));
    TEST_ASSERT_EQUAL(CGW_E_ARG, cgw_vstate_on_rx(&vs, CGW_VS_MSG_COUNT, NULL, 0u));
}

/* @verifies SWR-CGW-051 */
void test_push_policy(void)
{
    cgw_vs_push_t push;
    locksys_app_v1_StatusUpdate cand;

    cgw_vstate_push_reset(&push);
    cgw_vstate_build_status(&vs, &ctx, 0u, &su);
    TEST_ASSERT_TRUE(cgw_vstate_push_due(&push, &su, 0u, false));
    cgw_vstate_push_commit(&push, &su, 0u);
    cand = su;
    cand.seq = 2u;
    cand.status_age_ms = 500u;
    cand.vbat_dv = 99u;
    TEST_ASSERT_FALSE(cgw_vstate_push_due(&push, &cand, LS_T_STATUS_PUSH_IDLE_MS - 1u, false));
    TEST_ASSERT_TRUE(cgw_vstate_push_due(&push, &cand, LS_T_STATUS_PUSH_IDLE_MS, false));
    TEST_ASSERT_TRUE(cgw_vstate_push_due(&push, &cand, LS_T_STATUS_PUSH_MOTION_MS, true));
    cand.window_state = locksys_app_v1_WindowState_WINDOW_STATE_MOVING_UP;
    TEST_ASSERT_TRUE(cgw_vstate_push_due(&push, &cand, 10u, false));
    cand = su;
    cand.dcu_dtc_count = 1u;
    TEST_ASSERT_FALSE(cgw_vstate_push_due(&push, &cand, LS_T_STATUS_PUSH_GAP_MS - 1u, false));
    TEST_ASSERT_TRUE(cgw_vstate_push_due(&push, &cand, LS_T_STATUS_PUSH_GAP_MS, false));
    cand = su;
    cand.temperature_cdeg = LS_TEMP_PUSH_DELTA_CDEG - 1;
    TEST_ASSERT_FALSE(cgw_vstate_push_due(&push, &cand, 100u, false));
    cand.temperature_cdeg = -LS_TEMP_PUSH_DELTA_CDEG;
    TEST_ASSERT_TRUE(cgw_vstate_push_due(&push, &cand, 100u, false));
    cand = su;
    push.requested = true;
    TEST_ASSERT_TRUE(cgw_vstate_push_due(&push, &cand, 1u, false));
}
