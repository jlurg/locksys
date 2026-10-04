/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file adc.c
 * @brief ADC1 regular scan of IN0, IN1, IN4 and IN17 at 1 kHz into DMA1 channel 1.
 *
 * Skeleton: the behaviour is delivered at milestone M1; until then every
 * function returns its safe value.
 */

#include "mcal/adc/adc.h"

Ls_ReturnType Adc_Init(void)
{
    return LS_E_NOT_OK;
}

uint16_t Adc_GetRaw(Adc_ChannelType channel)
{
    (void)channel;
    return (uint16_t)0;
}

bool Adc_IsFresh(void)
{
    return false;
}
