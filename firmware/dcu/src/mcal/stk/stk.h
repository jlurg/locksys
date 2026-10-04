/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file stk.h
 * @brief SysTick 1 ms time base and DWT cycle counter.
 *
 * The SysTick interrupt calls StkCfg_TickNotification() (cfg/mcal/stk_cfg.c), which binds the
 * tick to TBase and Sched.
 */

#ifndef STK_H
#define STK_H

#include "platform/ls_std_types.h"

/**
 * @brief Starts SysTick with a 1 ms period and the DWT cycle counter.
 *
 * @param[in] hclkHz Core clock in Hz (Clk_GetSysclkHz()).
 * @retval LS_E_OK     SysTick running.
 * @retval LS_E_NOT_OK @p hclkHz gives a reload value outside the 24-bit range.
 * @pre Nvic_Init() has set the SysTick priority.
 */
Ls_ReturnType Stk_Init(uint32_t hclkHz);

/**
 * @brief Returns the DWT cycle counter.
 *
 * @return Free-running core cycle count.
 */
uint32_t Stk_CycNow(void);

/**
 * @brief SysTick exception handler.
 */
void SysTick_Handler(void);

#endif /* STK_H */
