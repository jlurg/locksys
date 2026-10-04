/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file safemon.h
 * @brief Safety monitor: SAFE entry, fault-handler entry, stack canary and ROM CRC.
 */

#ifndef SAFEMON_H
#define SAFEMON_H

#include "platform/ls_std_types.h"

/** @brief Cause of a SAFE entry. */
typedef uint8_t SafeMon_CauseType;

#define SAFEMON_CAUSE_NONE   ((SafeMon_CauseType)0u) /**< Not in SAFE */
#define SAFEMON_CAUSE_MODE   ((SafeMon_CauseType)1u) /**< Critical cause from ModeMgr */
#define SAFEMON_CAUSE_ENGINE ((SafeMon_CauseType)2u) /**< State-machine engine fault */
#define SAFEMON_CAUSE_WDGM   ((SafeMon_CauseType)3u) /**< Alive supervision failure */
#define SAFEMON_CAUSE_ROMCRC ((SafeMon_CauseType)4u) /**< ROM CRC mismatch */
#define SAFEMON_CAUSE_STACK  ((SafeMon_CauseType)5u) /**< Stack canary corrupted */
#define SAFEMON_CAUSE_CLOCK  ((SafeMon_CauseType)6u) /**< Clock failure (HSI64 profile) */

/**
 * @brief Clears the runtime state; the noinit SAFE latch is evaluated by EcuM.
 */
void SafeMon_Init(void);

/**
 * @brief Enters SAFE: both bridges off and the SAFE latch set.
 *
 * @param[in] cause Cause of the entry; the first cause is kept.
 * @note Callable from any context.
 */
void SafeMon_EnterSafe(SafeMon_CauseType cause);

/**
 * @brief Reports whether SAFE has been entered since start-up.
 *
 * @return true in SAFE.
 */
bool SafeMon_IsSafe(void);

/**
 * @brief Fault-handler entry, reached on the fault stack from the startup stub.
 *
 * Switches the outputs to the safe state, then waits for the watchdog reset.
 *
 * @note Does not return.
 */
void SafeMon_FaultEntry(void);

/**
 * @brief Computes the ROM CRC over the application region and compares it with ls_rom_crc.
 *
 * @retval LS_E_OK     CRC matches.
 * @retval LS_E_NOT_OK Mismatch or not evaluated.
 */
Ls_ReturnType SafeMon_RomCheckFull(void);

/**
 * @brief Stack canary check and statistics.
 *
 * @note 100 ms task.
 */
void SafeMon_Main100ms(void);

#endif /* SAFEMON_H */
