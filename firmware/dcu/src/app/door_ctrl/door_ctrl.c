/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file door_ctrl.c
 * @brief DoorCtrl adapter: the only file that calls the DoorCtrl engine.
 *
 * Skeleton: with LS_CFG_VS_ENGINE_PRESENT = 0 no engine is linked; no lock pulse is started
 * and the status reports the current result buffer. The engine path is delivered at
 * milestone M2.
 */

#include "app/door_ctrl/door_ctrl.h"

#include "app/door_ctrl/door_ctrl_priv.h"
#include "ls_cfg.h"

static LsEvSet_Type s_events;

void DoorCtrl_Init(void)
{
    (void)LsEvSet_Init(&s_events, DOOR_CTRL_DRAIN_BUDGET);
    DoorCtrl_ActionsReset();
}

void DoorCtrl_Main10ms(void)
{
    const DoorCtrl_OutType *const out = DoorCtrl_ActionsOutput();
    Rte_DoorRequestType request;
    Rte_EcuModeType mode;
    Rte_DoorStatusType status;

    (void)Rte_Read_DoorRequest(&request);
    (void)Rte_Read_EcuMode(&mode);
    if (mode.mode == LS_NODE_MODE_SAFE)
    {
        (void)LsEvSet_Post(&s_events, DOOR_CTRL_EV_SAFE);
    }
    (void)LsEvSet_Post(&s_events, DOOR_CTRL_EV_TICK);
    if (request.newRequest && DoorCtrl_GuardNewRequest(request.reqId, out->lastReqId))
    {
        (void)LsEvSet_Post(&s_events, DOOR_CTRL_EV_DOOR_REQ);
    }
    /* Without a linked engine the events have no consumer. */
    LsEvSet_Clear(&s_events);

    status.state = out->state;
    status.lastReqId = out->lastReqId;
    status.lastResult = out->result;
    status.rateLimited = false;
    Rte_Write_DoorStatus(&status);
}
