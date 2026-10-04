/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_crit.h
 * @brief Nestable critical sections on BASEPRI (LS-DCU-SAD-001 section 5.3).
 *
 * A section only raises the masking level; leaving it restores the level found on entry, so
 * sections nest in any combination. The I2C handlers (priority 0) are never masked.
 */

#ifndef LS_CRIT_H
#define LS_CRIT_H

#include <stdint.h>

/** @brief BASEPRI value found on entry; passed back to LsCrit_Exit(). */
typedef uint32_t LsCrit_StateType;

/** @brief BASEPRI of a task section: masks priorities 3 and lower (CAN, DMA). */
#define LS_CRIT_BASEPRI_TASK (0x30u)
/** @brief BASEPRI of a reflex section: masks priorities 1 and lower (EXTI reflexes, SysTick). */
#define LS_CRIT_BASEPRI_REFLEX (0x10u)

/**
 * @brief Enters a task critical section.
 *
 * @return BASEPRI value on entry, to be passed to LsCrit_Exit().
 * @note Callable from task and interrupt context.
 */
LsCrit_StateType LsCrit_Enter(void);

/**
 * @brief Enters a reflex critical section (at most 2 us long).
 *
 * @return BASEPRI value on entry, to be passed to LsCrit_Exit().
 * @note Used for check-then-write sequences against the reflex handlers.
 */
LsCrit_StateType LsCrit_EnterReflex(void);

/**
 * @brief Leaves a critical section.
 *
 * @param[in] state Value returned by the matching LsCrit_Enter() or LsCrit_EnterReflex().
 */
void LsCrit_Exit(LsCrit_StateType state);

#endif /* LS_CRIT_H */
