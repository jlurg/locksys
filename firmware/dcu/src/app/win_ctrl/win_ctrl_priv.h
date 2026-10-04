/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file win_ctrl_priv.h
 * @brief WinCtrl internals: events, input snapshot, output buffer, guards and actions.
 *
 * Included only by the files of src/app/win_ctrl/ and by their unit tests.
 */

#ifndef WIN_CTRL_PRIV_H
#define WIN_CTRL_PRIV_H

#include "ls_common/ls_evset.h"
#include "rte/rte_win_ctrl.h"

/* Event bits, drained lowest first (LS-DCU-SAD-001 section 8.4); B = stage B, not posted. */
#define WIN_CTRL_EV_SAFE          ((LsEvSet_EventType)0u)  /**< EcuMode is SAFE */
#define WIN_CTRL_EV_DRIVER_FAULT  ((LsEvSet_EventType)1u)  /**< EN/DIAG reflex latch newly set */
#define WIN_CTRL_EV_LIMIT_UP      ((LsEvSet_EventType)2u)  /**< B */
#define WIN_CTRL_EV_LIMIT_DN      ((LsEvSet_EventType)3u)  /**< B */
#define WIN_CTRL_EV_MOTION_FAULT  ((LsEvSet_EventType)4u)  /**< New NO_MOTION or DIR_MISMATCH */
#define WIN_CTRL_EV_DRIVE_REFUSED ((LsEvSet_EventType)5u)  /**< Gate refused a drive */
#define WIN_CTRL_EV_TM_BRAKE      ((LsEvSet_EventType)6u)  /**< Brake time elapsed */
#define WIN_CTRL_EV_TM_DEAD       ((LsEvSet_EventType)7u)  /**< Dead time elapsed */
#define WIN_CTRL_EV_TICK          ((LsEvSet_EventType)8u)  /**< Once per cycle */
#define WIN_CTRL_EV_REQ_UP        ((LsEvSet_EventType)9u)  /**< New press UP */
#define WIN_CTRL_EV_REQ_DOWN      ((LsEvSet_EventType)10u) /**< New press DOWN */

/** @brief Drain budget per cycle: 11 events plus one re-post. */
#define WIN_CTRL_DRAIN_BUDGET (12u)

/** @brief Number of model timers. */
#define WIN_CTRL_TIMER_COUNT (2u)

/** @brief Adapter phase, mirrored from the engine state for the status mapping. */
typedef uint8_t WinCtrl_PhaseType;

#define WIN_CTRL_PHASE_INIT        ((WinCtrl_PhaseType)0u) /**< Init */
#define WIN_CTRL_PHASE_IDLE        ((WinCtrl_PhaseType)1u) /**< Operational.Idle */
#define WIN_CTRL_PHASE_MOVING_UP   ((WinCtrl_PhaseType)2u) /**< Operational.Moving.MovingUp */
#define WIN_CTRL_PHASE_MOVING_DOWN ((WinCtrl_PhaseType)3u) /**< Operational.Moving.MovingDown */
#define WIN_CTRL_PHASE_BRAKE       ((WinCtrl_PhaseType)4u) /**< Operational.Brake */
#define WIN_CTRL_PHASE_DEAD        ((WinCtrl_PhaseType)5u) /**< Operational.Dead */
#define WIN_CTRL_PHASE_FAULT       ((WinCtrl_PhaseType)6u) /**< Fault */

/** @brief Input snapshot taken once per cycle. */
typedef struct
{
    Rte_WinRequestType request; /**< Port WinRequest */
    Rte_EcuModeType mode;       /**< Port EcuMode */
    uint32_t nowMs;             /**< Time stamp of the snapshot */
} WinCtrl_InType;

/** @brief Output buffer written by the actions and applied once per cycle. */
typedef struct
{
    Rte_BridgeCmdType bridgeCmd;        /**< Requested bridge command */
    uint16_t dutyPermille;              /**< Requested drive duty */
    WinCtrl_PhaseType phase;            /**< Phase entered last */
    Ls_WindowStopReasonType stopReason; /**< StopReason of the last stop or refusal */
    Ls_CommandResultType result;        /**< WinResult */
    uint8_t latchPressId;               /**< PressId to latch, 0 for none */
    uint8_t trace;                      /**< Numeric identifier of the last transition */
    uint8_t timerStartMask;             /**< Timers to start in the apply phase (bit = slot) */
    uint8_t timerStopMask;              /**< Timers to stop in the apply phase (bit = slot) */
    uint32_t timerDurationMs[WIN_CTRL_TIMER_COUNT]; /**< Durations of the timers to start */
} WinCtrl_OutType;

/**
 * @brief Guard xModeReady: NodeMode is NORMAL or DEGRADED.
 *
 * @param[in] mode DCU NodeMode.
 * @return true when the mode allows motion.
 */
bool WinCtrl_GuardModeReady(Ls_NodeModeType mode);

/**
 * @brief Start condition S5: the PressId is neither 0 nor the latched PressId.
 *
 * @param[in] pressId        PressId of the request.
 * @param[in] latchedPressId PressId held by the latch store.
 * @return true for a new press.
 */
bool WinCtrl_GuardNewPress(uint8_t pressId, uint8_t latchedPressId);

/**
 * @brief Selects the StopReason of highest priority from a set of active stop causes.
 *
 * Priority: DRIVER_FAULT > OVERCURRENT > DIR_MISMATCH > STALL > OBSTACLE > UPPER_LIMIT >
 * LOWER_LIMIT > OVERVOLTAGE > UNDERVOLTAGE > OVERTEMP > MODE_INHIBIT > E2E_ERROR >
 * CAN_TIMEOUT > HOLD_TIMEOUT > MAX_RUNTIME > RELEASED.
 *
 * @param[in] causes Bit n set: StopReason value n is active.
 * @return StopReason of highest priority; NONE when no cause is set.
 */
Ls_WindowStopReasonType WinCtrl_StopReasonArbitrate(uint32_t causes);

/**
 * @brief Maps the adapter phase to the WindowState reported on CAN.
 *
 * @param[in] phase Adapter phase.
 * @return WindowState; UNKNOWN for an unknown phase.
 */
Ls_WindowStateType WinCtrl_MapState(WinCtrl_PhaseType phase);

/**
 * @brief Clears the output buffer to the safe values (bridge off, no timer request).
 */
void WinCtrl_ActionsReset(void);

/**
 * @brief Returns the output buffer.
 *
 * @return Output buffer of the current cycle.
 */
const WinCtrl_OutType *WinCtrl_ActionsOutput(void);

/**
 * @brief Clears the timer requests after the apply phase.
 */
void WinCtrl_ActionsClearTimerRequests(void);

/**
 * @brief Action aTr: records the numeric transition identifier.
 *
 * @param[in] trId Numeric identifier, 1-19.
 */
void WinCtrl_ActTrace(uint8_t trId);

/**
 * @brief Action: requests a bridge command and enters a phase.
 *
 * @param[in] cmd          Bridge command.
 * @param[in] dutyPermille Drive duty; ignored for OFF and BRAKE.
 * @param[in] phase        Phase entered.
 */
void WinCtrl_ActBridge(Rte_BridgeCmdType cmd, uint16_t dutyPermille, WinCtrl_PhaseType phase);

/**
 * @brief Action: records the stop reason and the result of a stop or refusal.
 *
 * @param[in] reason StopReason.
 * @param[in] result WinResult.
 */
void WinCtrl_ActStop(Ls_WindowStopReasonType reason, Ls_CommandResultType result);

/**
 * @brief Action: requests the PressId latch for a press.
 *
 * @param[in] pressId PressId to latch.
 */
void WinCtrl_ActLatchPress(uint8_t pressId);

/**
 * @brief Action aTmStart: requests a timer start.
 *
 * @param[in] slot       Timer slot (0: evTmBrake, 1: evTmDead).
 * @param[in] durationMs Duration in ms.
 */
void WinCtrl_ActTimerStart(uint8_t slot, uint32_t durationMs);

/**
 * @brief Generated aTmStart stop variant: requests a timer stop.
 *
 * @param[in] slot Timer slot.
 */
void WinCtrl_ActTimerStop(uint8_t slot);

#endif /* WIN_CTRL_PRIV_H */
