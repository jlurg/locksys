/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file pwr.h
 * @brief Programmable voltage detector and backup registers.
 */

#ifndef PWR_H
#define PWR_H

#include "platform/ls_std_types.h"

/**
 * @brief Enables the PVD (PLS = 111) on EXTI16 and backup register access.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Pwr_Init(void);

/**
 * @brief PVD interrupt service: safe outputs and the noinit flag.
 *
 * @note Interrupt priority 1.
 */
void Pwr_PvdIsr(void);

/**
 * @brief Reads a backup data register.
 *
 * @param[in] index Backup data register index, 0-9.
 * @return Register value; 0 for an invalid index.
 */
uint16_t Pwr_BkpRead(uint8_t index);

/**
 * @brief Writes a backup data register.
 *
 * @param[in] index Backup data register index, 0-9.
 * @param[in] value Value.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Pwr_BkpWrite(uint8_t index, uint16_t value);

#endif /* PWR_H */
