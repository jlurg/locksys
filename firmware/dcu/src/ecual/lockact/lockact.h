/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file lockact.h
 * @brief Lock actuator pulse on bridge channel M2 with hard cap and end detection.
 */

#ifndef LOCKACT_H
#define LOCKACT_H

#include "platform/ls_std_types.h"

/** @brief Lock pulse direction. */
typedef uint8_t LockAct_DirType;

#define LOCK_ACT_DIR_LOCK   ((LockAct_DirType)0u) /**< Towards LOCKED */
#define LOCK_ACT_DIR_UNLOCK ((LockAct_DirType)1u) /**< Towards UNLOCKED */

/** @brief End of the last pulse. */
typedef uint8_t LockAct_EndType;

#define LOCK_ACT_END_NONE ((LockAct_EndType)0u) /**< No pulse ended yet */
#define LOCK_ACT_END_TIME ((LockAct_EndType)1u) /**< Pulse time elapsed */
#define LOCK_ACT_END_STROKE \
    ((LockAct_EndType)2u) /**< End of stroke (current after the minimum stroke) */
#define LOCK_ACT_END_CAP ((LockAct_EndType)3u) /**< Hard cap reached */
#define LOCK_ACT_END_FAULT \
    ((LockAct_EndType)4u) /**< EN/DIAG or over-current before the minimum stroke */

/**
 * @brief Starts a lock pulse at 100 % duty.
 *
 * @param[in] direction Pulse direction.
 * @param[in] durationMs Pulse length, capped at t_lock_pulse_hard_max_ms.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType LockAct_Pulse(LockAct_DirType direction, uint16_t durationMs);

/**
 * @brief Ends a running pulse (bridge off).
 */
void LockAct_Stop(void);

/**
 * @brief Reports whether a pulse runs.
 *
 * @return true while a pulse runs.
 */
bool LockAct_IsActive(void);

/**
 * @brief Returns how the last pulse ended.
 *
 * @return One of LOCK_ACT_END_*.
 */
LockAct_EndType LockAct_GetEnd(void);

/**
 * @brief Returns the peak current of the last pulse.
 *
 * @return Current in mA.
 */
uint16_t LockAct_GetPeakMa(void);

/**
 * @brief Pulse timing, hard cap and over-current evaluation.
 *
 * @note 1 ms task.
 */
void LockAct_Main1ms(void);

/**
 * @brief Lock EN/DIAG falling-edge reflex: bridge off and reflex latch.
 *
 * @note Interrupt priority 1.
 */
void LockAct_DiagIsr(void);

#endif /* LOCKACT_H */
