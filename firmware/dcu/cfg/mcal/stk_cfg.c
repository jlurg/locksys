/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file stk_cfg.c
 * @brief Binding of the SysTick interrupt to TBase and Sched.
 */

#include "mcal/stk_cfg.h"

#include "services/sched/sched.h"
#include "services/tbase/tbase.h"

void StkCfg_TickNotification(void)
{
    TBase_TickIsr();
    Sched_TickIsr();
}
