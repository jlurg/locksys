/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file wdg.c
 * @brief Independent watchdog.
 */

#include "mcal/wdg/wdg.h"

#include "stm32f1xx.h"

#define WDG_KEY_START   (0xCCCCu)
#define WDG_KEY_UNLOCK  (0x5555u)
#define WDG_KEY_RELOAD  (0xAAAAu)
#define WDG_PRESCALER_4 (0u)
#define WDG_RELOAD      (499u)
/* RM0008 section 19.4: an update takes up to 5 LSI periods (about 125 us at 40 kHz). */
#define WDG_UPDATE_POLLS (20000u)

/* @satisfies SWR-DCU-076 */
Ls_ReturnType Wdg_Start(void)
{
    uint32_t remaining = WDG_UPDATE_POLLS;

    IWDG->KR = WDG_KEY_START;
    IWDG->KR = WDG_KEY_UNLOCK;
    IWDG->PR = WDG_PRESCALER_4;
    IWDG->RLR = WDG_RELOAD;
    while (((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0u) && (remaining > 0u))
    {
        remaining--;
    }
    IWDG->KR = WDG_KEY_RELOAD;
    return (remaining > 0u) ? LS_E_OK : LS_E_NOT_OK;
}

void Wdg_Refresh(void)
{
    IWDG->KR = WDG_KEY_RELOAD;
}
