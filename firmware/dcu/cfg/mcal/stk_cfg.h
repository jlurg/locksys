/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file stk_cfg.h
 * @brief Binding of the SysTick interrupt to its upper-layer consumers.
 */

#ifndef STK_CFG_H
#define STK_CFG_H

/**
 * @brief Called by SysTick_Handler() every 1 ms.
 *
 * @note Interrupt context, priority 2; budget 2 us.
 */
void StkCfg_TickNotification(void);

#endif /* STK_CFG_H */
