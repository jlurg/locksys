/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "unity.h"

#include "app/win_ctrl/win_ctrl_priv.h"

TEST_SOURCE_FILE("src/app/win_ctrl/win_ctrl_guards.c")

#define CAUSE(reason) ((uint32_t)1u << (reason))

void setUp(void)
{
}

void tearDown(void)
{
}

void test_WinCtrl_GuardModeReady_NormalOrDegraded_True(void)
{
    TEST_ASSERT_TRUE(WinCtrl_GuardModeReady(LS_NODE_MODE_NORMAL));
    TEST_ASSERT_TRUE(WinCtrl_GuardModeReady(LS_NODE_MODE_DEGRADED));
}

void test_WinCtrl_GuardModeReady_OtherModes_False(void)
{
    TEST_ASSERT_FALSE(WinCtrl_GuardModeReady(LS_NODE_MODE_UNKNOWN));
    TEST_ASSERT_FALSE(WinCtrl_GuardModeReady(LS_NODE_MODE_INIT));
    TEST_ASSERT_FALSE(WinCtrl_GuardModeReady(LS_NODE_MODE_SAFE));
    TEST_ASSERT_FALSE(WinCtrl_GuardModeReady(LS_NODE_MODE_SERVICE));
}

/* @verifies SWR-DCU-007 */
void test_WinCtrl_GuardNewPress_ZeroOrLatched_False(void)
{
    TEST_ASSERT_FALSE(WinCtrl_GuardNewPress(0u, 7u));
    TEST_ASSERT_FALSE(WinCtrl_GuardNewPress(7u, 7u));
}

/* @verifies SWR-DCU-007 */
void test_WinCtrl_GuardNewPress_OtherPressId_True(void)
{
    TEST_ASSERT_TRUE(WinCtrl_GuardNewPress(8u, 7u));
    TEST_ASSERT_TRUE(WinCtrl_GuardNewPress(1u, 0u));
}

/* @verifies SWR-DCU-003 */
void test_WinCtrl_StopReasonArbitrate_NoCause_None(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STOP_REASON_NONE, WinCtrl_StopReasonArbitrate(0u));
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STOP_REASON_NONE,
                            WinCtrl_StopReasonArbitrate(CAUSE(LS_WINDOW_STOP_REASON_NONE)));
}

/* @verifies SWR-DCU-003 */
void test_WinCtrl_StopReasonArbitrate_EachSingleCause_ReturnsIt(void)
{
    Ls_WindowStopReasonType reason;

    for (reason = LS_WINDOW_STOP_REASON_RELEASED; reason < LS_WINDOW_STOP_REASON_COUNT; reason++)
    {
        TEST_ASSERT_EQUAL_UINT8(reason, WinCtrl_StopReasonArbitrate(CAUSE(reason)));
    }
}

/* @verifies SWR-DCU-003 */
void test_WinCtrl_StopReasonArbitrate_AllCauses_DriverFaultWins(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STOP_REASON_DRIVER_FAULT,
                            WinCtrl_StopReasonArbitrate(0xFFFFFFFFu));
}

/* @verifies SWR-DCU-003 */
void test_WinCtrl_StopReasonArbitrate_PriorityChain_FollowsDesignOrder(void)
{
    static const Ls_WindowStopReasonType order[] = {
        LS_WINDOW_STOP_REASON_DRIVER_FAULT, LS_WINDOW_STOP_REASON_OVERCURRENT,
        LS_WINDOW_STOP_REASON_DIR_MISMATCH, LS_WINDOW_STOP_REASON_STALL,
        LS_WINDOW_STOP_REASON_OBSTACLE,     LS_WINDOW_STOP_REASON_UPPER_LIMIT,
        LS_WINDOW_STOP_REASON_LOWER_LIMIT,  LS_WINDOW_STOP_REASON_OVERVOLTAGE,
        LS_WINDOW_STOP_REASON_UNDERVOLTAGE, LS_WINDOW_STOP_REASON_OVERTEMP,
        LS_WINDOW_STOP_REASON_MODE_INHIBIT, LS_WINDOW_STOP_REASON_E2E_ERROR,
        LS_WINDOW_STOP_REASON_CAN_TIMEOUT,  LS_WINDOW_STOP_REASON_HOLD_TIMEOUT,
        LS_WINDOW_STOP_REASON_MAX_RUNTIME,  LS_WINDOW_STOP_REASON_RELEASED,
    };
    uint32_t causes = 0u;
    uint32_t i;

    for (i = 0u; i < (sizeof(order) / sizeof(order[0])); i++)
    {
        causes |= CAUSE(order[i]);
    }
    for (i = 0u; i < (sizeof(order) / sizeof(order[0])); i++)
    {
        TEST_ASSERT_EQUAL_UINT8(order[i], WinCtrl_StopReasonArbitrate(causes));
        causes &= ~CAUSE(order[i]);
    }
}

/* @verifies SWR-DCU-012 */
void test_WinCtrl_MapState_EachPhase_MapsToWindowState(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STATE_UNKNOWN, WinCtrl_MapState(WIN_CTRL_PHASE_INIT));
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STATE_STOPPED, WinCtrl_MapState(WIN_CTRL_PHASE_IDLE));
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STATE_STOPPED, WinCtrl_MapState(WIN_CTRL_PHASE_BRAKE));
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STATE_STOPPED, WinCtrl_MapState(WIN_CTRL_PHASE_DEAD));
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STATE_MOVING_UP, WinCtrl_MapState(WIN_CTRL_PHASE_MOVING_UP));
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STATE_MOVING_DOWN,
                            WinCtrl_MapState(WIN_CTRL_PHASE_MOVING_DOWN));
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STATE_FAULT, WinCtrl_MapState(WIN_CTRL_PHASE_FAULT));
}

void test_WinCtrl_MapState_UnknownPhase_Unknown(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_WINDOW_STATE_UNKNOWN, WinCtrl_MapState((WinCtrl_PhaseType)0xFFu));
}
