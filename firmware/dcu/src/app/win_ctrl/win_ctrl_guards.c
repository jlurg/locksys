/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file win_ctrl_guards.c
 * @brief WinCtrl guards: pure functions of the input snapshot.
 */

#include "app/win_ctrl/win_ctrl_priv.h"

#define WIN_CTRL_STOP_PRIO_COUNT (16u)

/* LS-DCU-SAD-001 section 8.9: StopReason priority, highest first. */
static const Ls_WindowStopReasonType s_stopPriority[WIN_CTRL_STOP_PRIO_COUNT] = {
    LS_WINDOW_STOP_REASON_DRIVER_FAULT, LS_WINDOW_STOP_REASON_OVERCURRENT,
    LS_WINDOW_STOP_REASON_DIR_MISMATCH, LS_WINDOW_STOP_REASON_STALL,
    LS_WINDOW_STOP_REASON_OBSTACLE,     LS_WINDOW_STOP_REASON_UPPER_LIMIT,
    LS_WINDOW_STOP_REASON_LOWER_LIMIT,  LS_WINDOW_STOP_REASON_OVERVOLTAGE,
    LS_WINDOW_STOP_REASON_UNDERVOLTAGE, LS_WINDOW_STOP_REASON_OVERTEMP,
    LS_WINDOW_STOP_REASON_MODE_INHIBIT, LS_WINDOW_STOP_REASON_E2E_ERROR,
    LS_WINDOW_STOP_REASON_CAN_TIMEOUT,  LS_WINDOW_STOP_REASON_HOLD_TIMEOUT,
    LS_WINDOW_STOP_REASON_MAX_RUNTIME,  LS_WINDOW_STOP_REASON_RELEASED,
};

bool WinCtrl_GuardModeReady(Ls_NodeModeType mode)
{
    return (mode == LS_NODE_MODE_NORMAL) || (mode == LS_NODE_MODE_DEGRADED);
}

/* @satisfies SWR-DCU-007 */
bool WinCtrl_GuardNewPress(uint8_t pressId, uint8_t latchedPressId)
{
    return (pressId != 0u) && (pressId != latchedPressId);
}

/* @satisfies SWR-DCU-003 */
Ls_WindowStopReasonType WinCtrl_StopReasonArbitrate(uint32_t causes)
{
    Ls_WindowStopReasonType reason = LS_WINDOW_STOP_REASON_NONE;
    uint32_t i;

    for (i = 0u; (i < WIN_CTRL_STOP_PRIO_COUNT) && (reason == LS_WINDOW_STOP_REASON_NONE); i++)
    {
        if ((causes & ((uint32_t)1u << s_stopPriority[i])) != 0u)
        {
            reason = s_stopPriority[i];
        }
    }
    return reason;
}

/* @satisfies SWR-DCU-012 */
Ls_WindowStateType WinCtrl_MapState(WinCtrl_PhaseType phase)
{
    Ls_WindowStateType state;

    switch (phase)
    {
        case WIN_CTRL_PHASE_IDLE:
        case WIN_CTRL_PHASE_BRAKE:
        case WIN_CTRL_PHASE_DEAD:
            state = LS_WINDOW_STATE_STOPPED;
            break;
        case WIN_CTRL_PHASE_MOVING_UP:
            state = LS_WINDOW_STATE_MOVING_UP;
            break;
        case WIN_CTRL_PHASE_MOVING_DOWN:
            state = LS_WINDOW_STATE_MOVING_DOWN;
            break;
        case WIN_CTRL_PHASE_FAULT:
            state = LS_WINDOW_STATE_FAULT;
            break;
        default:
            state = LS_WINDOW_STATE_UNKNOWN;
            break;
    }
    return state;
}
