/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file enc.h
 * @brief Window encoder on TIM2 (PA15/PB3, remap 01), encoder mode x4.
 */

#ifndef ENC_H
#define ENC_H

#include "platform/ls_std_types.h"

/**
 * @brief Configures TIM2 in encoder mode 3 with input filter 0xF and ARR 0xFFFF.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Enc_Init(void);

/**
 * @brief Returns the free-running 16-bit counter.
 *
 * @return Counter value.
 */
uint16_t Enc_GetCount(void);

#endif /* ENC_H */
