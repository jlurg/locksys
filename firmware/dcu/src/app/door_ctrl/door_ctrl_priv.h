/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file door_ctrl_priv.h
 * @brief DoorCtrl internals: events, output buffer, guards and actions.
 *
 * Included only by the files of src/app/door_ctrl/ and by their unit tests.
 */

#ifndef DOOR_CTRL_PRIV_H
#define DOOR_CTRL_PRIV_H

#include "ls_common/ls_evset.h"
#include "rte/rte_door_ctrl.h"

/* Event bits, drained lowest first (LS-DCU-SAD-001 section 8.4). */
#define DOOR_CTRL_EV_SAFE           ((LsEvSet_EventType)0u) /**< EcuMode is SAFE */
#define DOOR_CTRL_EV_PULSE_END      ((LsEvSet_EventType)1u) /**< LockAct reported the pulse end */
#define DOOR_CTRL_EV_TM_PULSE_GUARD ((LsEvSet_EventType)2u) /**< Pulse guard time elapsed */
#define DOOR_CTRL_EV_TM_SETTLE      ((LsEvSet_EventType)3u) /**< Settle time elapsed */
#define DOOR_CTRL_EV_TM_PAUSE       ((LsEvSet_EventType)4u) /**< Retry pause elapsed */
#define DOOR_CTRL_EV_TICK           ((LsEvSet_EventType)5u) /**< Once per cycle */
#define DOOR_CTRL_EV_DOOR_REQ       ((LsEvSet_EventType)6u) /**< New ReqId */

/** @brief Drain budget per cycle. */
#define DOOR_CTRL_DRAIN_BUDGET (7u)

/** @brief Number of model timers. */
#define DOOR_CTRL_TIMER_COUNT (3u)

/** @brief Output buffer written by the actions and applied once per cycle. */
typedef struct
{
    Ls_DoorLockStateType state;  /**< DoorLockState during execution */
    uint8_t lastReqId;           /**< LastReqId */
    Ls_CommandResultType result; /**< LastResult */
    bool stopPulse;              /**< End the running pulse */
    uint8_t trace;               /**< Numeric identifier of the last transition */
} DoorCtrl_OutType;

/**
 * @brief New-request detection: ReqId is not 0 and differs from LastReqId.
 *
 * @param[in] reqId     ReqId of CGW_DoorCmd.
 * @param[in] lastReqId Current LastReqId.
 * @return true for a new request.
 */
bool DoorCtrl_GuardNewRequest(uint8_t reqId, uint8_t lastReqId);

/**
 * @brief Maps the debounced position switch level to DoorLockState.
 *
 * @param[in] switchKnown  The switch has been debounced at least once.
 * @param[in] switchHigh   Debounced level is high.
 * @param[in] lockedLevel  Level that means LOCKED (lock_fb_locked_level): true = high.
 * @return LOCKED, UNLOCKED or UNKNOWN.
 */
Ls_DoorLockStateType DoorCtrl_MapLockState(bool switchKnown, bool switchHigh, bool lockedLevel);

/**
 * @brief Clears the output buffer to the safe values.
 */
void DoorCtrl_ActionsReset(void);

/**
 * @brief Returns the output buffer.
 *
 * @return Output buffer of the current cycle.
 */
const DoorCtrl_OutType *DoorCtrl_ActionsOutput(void);

/**
 * @brief Action aTr: records the numeric transition identifier.
 *
 * @param[in] trId Numeric identifier, 21-39.
 */
void DoorCtrl_ActTrace(uint8_t trId);

/**
 * @brief Action: records the result of a transaction.
 *
 * @param[in] reqId  ReqId of the transaction.
 * @param[in] result LastResult.
 */
void DoorCtrl_ActResult(uint8_t reqId, Ls_CommandResultType result);

#endif /* DOOR_CTRL_PRIV_H */
