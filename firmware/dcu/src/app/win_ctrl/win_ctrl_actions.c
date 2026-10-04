/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file win_ctrl_actions.c
 * @brief Bodies of the WinCtrl engine actions: they write only the output buffer.
 *
 * The generated action prototypes are bound to these functions when the engine is linked
 * (Lab Host check D9, LS-DCU-SAD-001 assumption DCU-A1).
 */

#include "app/win_ctrl/win_ctrl_priv.h"

static WinCtrl_OutType s_out;

void WinCtrl_ActionsReset(void)
{
    uint32_t slot;

    s_out.bridgeCmd = RTE_BRIDGE_OFF;
    s_out.dutyPermille = 0u;
    s_out.phase = WIN_CTRL_PHASE_INIT;
    s_out.stopReason = LS_WINDOW_STOP_REASON_NONE;
    s_out.result = LS_COMMAND_RESULT_UNSPECIFIED;
    s_out.latchPressId = 0u;
    s_out.trace = 0u;
    s_out.timerStartMask = 0u;
    s_out.timerStopMask = 0u;
    for (slot = 0u; slot < WIN_CTRL_TIMER_COUNT; slot++)
    {
        s_out.timerDurationMs[slot] = 0u;
    }
}

const WinCtrl_OutType *WinCtrl_ActionsOutput(void)
{
    return &s_out;
}

void WinCtrl_ActionsClearTimerRequests(void)
{
    s_out.timerStartMask = 0u;
    s_out.timerStopMask = 0u;
}

void WinCtrl_ActTrace(uint8_t trId)
{
    s_out.trace = trId;
}

void WinCtrl_ActBridge(Rte_BridgeCmdType cmd, uint16_t dutyPermille, WinCtrl_PhaseType phase)
{
    s_out.bridgeCmd = cmd;
    s_out.dutyPermille =
        ((cmd == RTE_BRIDGE_DRIVE_UP) || (cmd == RTE_BRIDGE_DRIVE_DOWN)) ? dutyPermille : 0u;
    s_out.phase = phase;
}

void WinCtrl_ActStop(Ls_WindowStopReasonType reason, Ls_CommandResultType result)
{
    s_out.stopReason = reason;
    s_out.result = result;
}

void WinCtrl_ActLatchPress(uint8_t pressId)
{
    s_out.latchPressId = pressId;
}

void WinCtrl_ActTimerStart(uint8_t slot, uint32_t durationMs)
{
    if (slot < WIN_CTRL_TIMER_COUNT)
    {
        s_out.timerStartMask |= (uint8_t)(1u << slot);
        s_out.timerStopMask &= (uint8_t)~(1u << slot);
        s_out.timerDurationMs[slot] = durationMs;
    }
}

void WinCtrl_ActTimerStop(uint8_t slot)
{
    if (slot < WIN_CTRL_TIMER_COUNT)
    {
        s_out.timerStopMask |= (uint8_t)(1u << slot);
        s_out.timerStartMask &= (uint8_t)~(1u << slot);
    }
}
