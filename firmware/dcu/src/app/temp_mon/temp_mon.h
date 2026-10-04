/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file temp_mon.h
 * @brief Temperature sampling, plausibility and over-temperature.
 */

#ifndef TEMP_MON_H
#define TEMP_MON_H

#include "platform/ls_std_types.h"

/**
 * @brief Initialises the sampling state.
 */
void TempMon_Init(void);

/**
 * @brief Completes a running sample.
 *
 * @note 5 ms task.
 */
void TempMon_Main5ms(void);

/**
 * @brief Starts a sample.
 *
 * @note 1000 ms task.
 */
void TempMon_Main1000ms(void);

#endif /* TEMP_MON_H */
