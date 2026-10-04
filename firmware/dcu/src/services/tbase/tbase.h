/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file tbase.h
 * @brief 32-bit millisecond time base, written only by the SysTick interrupt.
 */

#ifndef TBASE_H
#define TBASE_H

#include "platform/ls_std_types.h"

/**
 * @brief Advances the time base by 1 ms.
 *
 * @note SysTick interrupt only.
 */
void TBase_TickIsr(void);

/**
 * @brief Returns the milliseconds since start-up (wraps after about 49.7 days).
 *
 * @return Time stamp in ms.
 */
uint32_t TBase_Ms(void);

/**
 * @brief Returns the wrap-safe time elapsed since a time stamp.
 *
 * @param[in] sinceMs Earlier value of TBase_Ms().
 * @return Elapsed time in ms.
 */
uint32_t TBase_Since(uint32_t sinceMs);

#endif /* TBASE_H */
