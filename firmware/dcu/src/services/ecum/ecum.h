/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ecum.h
 * @brief ECU state manager: start-up sequence, reset reason and controlled reset.
 */

#ifndef ECUM_H
#define ECUM_H

#include "ls_enums_gen.h"
#include "platform/ls_std_types.h"

/**
 * @brief Start-up step S1: safe outputs before the clock set-up.
 *
 * @note Called from the reset handler before the C runtime initialisation: uses no
 *       initialised or zero-initialised data.
 */
void EcuM_EarlyInit(void);

/**
 * @brief Start-up steps S2 to S8 (LS-DCU-SAD-001 section 9.1).
 *
 * @pre EcuM_EarlyInit() has completed.
 */
void EcuM_Init(void);

/**
 * @brief Decodes RCC_CSR reset flags with the priority IWDG > WWDG > SFT > LPWR > POR > PIN.
 *
 * @param[in] flags Reset flags (CLK_RESET_FLAG_* bits).
 * @return Reset reason; never LS_RESET_REASON_BROWNOUT.
 */
Ls_ResetReasonType EcuM_DecodeResetReason(uint8_t flags);

/**
 * @brief Returns the reset reason decoded at start-up.
 *
 * @return Reset reason.
 */
Ls_ResetReasonType EcuM_GetResetReason(void);

/**
 * @brief Switches the outputs off and requests a system reset.
 *
 * @note Does not return on the target.
 */
void EcuM_RequestReset(void);

#endif /* ECUM_H */
