/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file stk.c
 * @brief SysTick 1 ms time base and DWT cycle counter.
 */

#include "mcal/stk/stk.h"

#include "mcal/stk_cfg.h"
#include "platform/ls_compiler.h"
#include "stm32f1xx.h"

#define STK_TICKS_PER_S (1000u)

/* @satisfies SWR-DCU-112 */
Ls_ReturnType Stk_Init(uint32_t hclkHz)
{
    const uint32_t reload = (hclkHz / STK_TICKS_PER_S) - 1u;

    if ((reload == 0u) || (reload > SysTick_LOAD_RELOAD_Msk))
    {
        return LS_E_NOT_OK;
    }
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    SysTick->LOAD = reload;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    return LS_E_OK;
}

uint32_t Stk_CycNow(void)
{
    return DWT->CYCCNT;
}

LS_ISR_ROOT
void SysTick_Handler(void)
{
    StkCfg_TickNotification();
}
