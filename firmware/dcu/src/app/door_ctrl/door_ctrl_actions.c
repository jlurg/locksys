/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file door_ctrl_actions.c
 * @brief Bodies of the DoorCtrl engine actions: they write only the output buffer.
 */

#include "app/door_ctrl/door_ctrl_priv.h"

static DoorCtrl_OutType s_out;

void DoorCtrl_ActionsReset(void)
{
    s_out.state = LS_DOOR_LOCK_STATE_UNKNOWN;
    s_out.lastReqId = 0u;
    s_out.result = LS_COMMAND_RESULT_UNSPECIFIED;
    s_out.stopPulse = false;
    s_out.trace = 0u;
}

const DoorCtrl_OutType *DoorCtrl_ActionsOutput(void)
{
    return &s_out;
}

void DoorCtrl_ActTrace(uint8_t trId)
{
    s_out.trace = trId;
}

void DoorCtrl_ActResult(uint8_t reqId, Ls_CommandResultType result)
{
    s_out.lastReqId = reqId;
    s_out.result = result;
}
