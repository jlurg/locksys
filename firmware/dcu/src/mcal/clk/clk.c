/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file clk.c
 * @brief Clock tree, flash wait states and reset flags (RCC).
 */

#include "mcal/clk/clk.h"

#include "stm32f1xx.h"

/* Bounded waits: about 5 ms at 8 MHz with at least 5 cycles per poll. */
#define CLK_HSE_READY_POLLS (8000u)
#define CLK_PLL_READY_POLLS (8000u)
#define CLK_SWITCH_POLLS    (8000u)

#define CLK_HSI_HZ   (8000000u)
#define CLK_HSE72_HZ (72000000u)
#define CLK_HSI64_HZ (64000000u)

#define CLK_CSR_FLAGS_SHIFT (26u)

static Clk_ProfileType s_profile = CLK_PROFILE_RESET;
static uint32_t s_sysclkHz = CLK_HSI_HZ;

static bool Clk_WaitSet(volatile const uint32_t *reg, uint32_t mask, uint32_t polls)
{
    uint32_t remaining = polls;
    bool ready = ((*reg & mask) == mask);

    while ((!ready) && (remaining > 0u))
    {
        remaining--;
        ready = ((*reg & mask) == mask);
    }
    return ready;
}

static bool Clk_SwitchToPll(void)
{
    uint32_t remaining = CLK_SWITCH_POLLS;
    bool switched;

    RCC->CR |= RCC_CR_PLLON;
    if (!Clk_WaitSet(&RCC->CR, RCC_CR_PLLRDY, CLK_PLL_READY_POLLS))
    {
        return false;
    }
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    switched = ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL);
    while ((!switched) && (remaining > 0u))
    {
        remaining--;
        switched = ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL);
    }
    return switched;
}

static bool Clk_StartHse72(void)
{
    RCC->CR |= RCC_CR_HSEBYP;
    RCC->CR |= RCC_CR_HSEON;
    if (!Clk_WaitSet(&RCC->CR, RCC_CR_HSERDY, CLK_HSE_READY_POLLS))
    {
        RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_HSEBYP);
        return false;
    }
    /* RM0008 section 3.3.3: two wait states above 48 MHz, set before the switch. */
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;
    RCC->CFGR = RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6;
    return Clk_SwitchToPll();
}

static bool Clk_StartHsi64(void)
{
    uint32_t remaining = CLK_PLL_READY_POLLS;

    /* The PLL can be reconfigured only while it is not the clock source and is stopped. */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CR &= ~RCC_CR_PLLON;
    while (((RCC->CR & RCC_CR_PLLRDY) != 0u) && (remaining > 0u))
    {
        remaining--;
    }
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;
    /* PLLSRC = 0: HSI/2 = 4 MHz into the PLL. */
    RCC->CFGR = RCC_CFGR_PLLMULL16 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_ADCPRE_DIV6;
    return Clk_SwitchToPll();
}

/* @satisfies SWR-DCU-075 */
Ls_ReturnType Clk_Init(void)
{
    Ls_ReturnType result = LS_E_NOT_OK;

    if (Clk_StartHse72())
    {
        s_profile = CLK_PROFILE_HSE72;
        s_sysclkHz = CLK_HSE72_HZ;
        result = LS_E_OK;
    }
    else if (Clk_StartHsi64())
    {
        s_profile = CLK_PROFILE_HSI64;
        s_sysclkHz = CLK_HSI64_HZ;
    }
    else
    {
        /* PLL unusable: run from HSI 8 MHz. */
        RCC->CFGR = RCC_CFGR_SW_HSI;
        s_profile = CLK_PROFILE_RESET;
        s_sysclkHz = CLK_HSI_HZ;
    }
    return result;
}

Clk_ProfileType Clk_GetProfile(void)
{
    return s_profile;
}

uint32_t Clk_GetSysclkHz(void)
{
    return s_sysclkHz;
}

uint32_t Clk_GetPclk1Hz(void)
{
    /* APB1 runs at HCLK/2 in both PLL profiles and at HCLK after reset. */
    return (s_profile == CLK_PROFILE_RESET) ? s_sysclkHz : (s_sysclkHz / 2u);
}

Clk_ResetFlagsType Clk_GetResetFlags(void)
{
    return (Clk_ResetFlagsType)((RCC->CSR >> CLK_CSR_FLAGS_SHIFT) & 0x3Fu);
}

void Clk_ClearResetFlags(void)
{
    RCC->CSR |= RCC_CSR_RMVF;
}

void Clk_CssNmi(void)
{
    /* Clock security system handling is delivered at milestone M6. */
}
