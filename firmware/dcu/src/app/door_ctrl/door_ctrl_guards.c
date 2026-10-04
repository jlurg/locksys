/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file door_ctrl_guards.c
 * @brief DoorCtrl guards: pure functions.
 */

#include "app/door_ctrl/door_ctrl_priv.h"

/* @satisfies SWR-DCU-030 */
bool DoorCtrl_GuardNewRequest(uint8_t reqId, uint8_t lastReqId)
{
    return (reqId != 0u) && (reqId != lastReqId);
}

/* @satisfies SWR-DCU-036 */
Ls_DoorLockStateType DoorCtrl_MapLockState(bool switchKnown, bool switchHigh, bool lockedLevel)
{
    Ls_DoorLockStateType state = LS_DOOR_LOCK_STATE_UNKNOWN;

    if (switchKnown)
    {
        state =
            (switchHigh == lockedLevel) ? LS_DOOR_LOCK_STATE_LOCKED : LS_DOOR_LOCK_STATE_UNLOCKED;
    }
    return state;
}
