/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file tbase.c
 * @brief 32-bit millisecond time base.
 */

#include "services/tbase/tbase.h"

/* Single writer (SysTick); 32-bit aligned reads are atomic on Cortex-M3. */
static volatile uint32_t s_ms = 0u;

void TBase_TickIsr(void)
{
    s_ms = s_ms + 1u;
}

uint32_t TBase_Ms(void)
{
    return s_ms;
}

uint32_t TBase_Since(uint32_t sinceMs)
{
    return s_ms - sinceMs;
}
