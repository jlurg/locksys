/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file win_ctrl.c
 * @brief WinCtrl adapter: the only file that calls the WinCtrl engine.
 *
 * Skeleton: with LS_CFG_VS_ENGINE_PRESENT = 0 no engine is linked; events are discarded and
 * the window bridge is held off. The engine path (drain, return-code checks, gate and the
 * fail-safe routine) is delivered at milestone M2 (LS-DCU-SAD-001 sections 8.3 to 8.7).
 */

#include "app/win_ctrl/win_ctrl.h"

#include "app/win_ctrl/win_ctrl_priv.h"
#include "ls_cfg.h"
#include "ls_common/ls_tmr.h"

static const LsEvSet_EventType s_timerEvents[WIN_CTRL_TIMER_COUNT] = {
    WIN_CTRL_EV_TM_BRAKE,
    WIN_CTRL_EV_TM_DEAD,
};

static LsEvSet_Type s_events;
static LsTmr_SlotType s_timerSlots[WIN_CTRL_TIMER_COUNT];
static LsTmr_PoolType s_timers;
static WinCtrl_InType s_in;
static uint8_t s_latchedPressId = 0u;

static void WinCtrl_ReadInputs(WinCtrl_InType *in)
{
    (void)Rte_Read_WinRequest(&in->request);
    (void)Rte_Read_EcuMode(&in->mode);
    in->nowMs = Rte_Call_TimeMs();
}

static void WinCtrl_DeriveEvents(const WinCtrl_InType *in, LsEvSet_Type *events)
{
    if (in->mode.mode == LS_NODE_MODE_SAFE)
    {
        (void)LsEvSet_Post(events, WIN_CTRL_EV_SAFE);
    }
    (void)LsEvSet_Post(events, WIN_CTRL_EV_TICK);
    if ((in->request.frame == RTE_FRAME_VALID) &&
        WinCtrl_GuardNewPress(in->request.pressId, s_latchedPressId))
    {
        if (in->request.req == LS_WINDOW_REQUEST_UP)
        {
            (void)LsEvSet_Post(events, WIN_CTRL_EV_REQ_UP);
        }
        else if (in->request.req == LS_WINDOW_REQUEST_DOWN)
        {
            (void)LsEvSet_Post(events, WIN_CTRL_EV_REQ_DOWN);
        }
        else
        {
            /* STOP and INVALID post no request event. */
        }
    }
}

static void WinCtrl_ApplyTimers(const WinCtrl_OutType *out, uint32_t nowMs)
{
    uint8_t slot;

    for (slot = 0u; slot < WIN_CTRL_TIMER_COUNT; slot++)
    {
        const uint8_t bit = (uint8_t)(1u << slot);
        if ((out->timerStopMask & bit) != 0u)
        {
            (void)LsTmr_Stop(&s_timers, s_timerEvents[slot]);
        }
        if ((out->timerStartMask & bit) != 0u)
        {
            (void)LsTmr_Start(&s_timers, s_timerEvents[slot], nowMs, out->timerDurationMs[slot]);
        }
    }
    WinCtrl_ActionsClearTimerRequests();
}

static void WinCtrl_ApplyOutputs(void)
{
    const WinCtrl_OutType *const out = WinCtrl_ActionsOutput();

    if (out->latchPressId != 0u)
    {
        s_latchedPressId = out->latchPressId;
    }
#if LS_CFG_VS_ENGINE_PRESENT == 1
    (void)Rte_Call_WinBridge_Set(out->bridgeCmd, out->dutyPermille);
#else
    (void)Rte_Call_WinBridge_Set(RTE_BRIDGE_OFF, 0u);
#endif
    WinCtrl_ApplyTimers(out, s_in.nowMs);
}

static void WinCtrl_WriteStatus(void)
{
    const WinCtrl_OutType *const out = WinCtrl_ActionsOutput();
    Rte_WinStatusType status;

    status.state = WinCtrl_MapState(out->phase);
    status.stopReason = out->stopReason;
    status.pressIdEcho = s_latchedPressId;
    status.result = out->result;
    status.faults = 0u;
    Rte_Write_WinStatus(&status);
}

void WinCtrl_Init(void)
{
    (void)LsEvSet_Init(&s_events, WIN_CTRL_DRAIN_BUDGET);
    (void)LsTmr_Init(&s_timers, s_timerSlots, s_timerEvents, WIN_CTRL_TIMER_COUNT);
    WinCtrl_ActionsReset();
    s_latchedPressId = 0u;
}

void WinCtrl_Main10ms(void)
{
    WinCtrl_ReadInputs(&s_in);
    (void)LsTmr_Poll(&s_timers, s_in.nowMs, &s_events);
    WinCtrl_DeriveEvents(&s_in, &s_events);
    /* Without a linked engine the events have no consumer. */
    LsEvSet_Clear(&s_events);
    WinCtrl_ApplyOutputs();
    WinCtrl_WriteStatus();
}
