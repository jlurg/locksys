/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file pwm.c
 * @brief Bridge PWM: TIM3_CH2 on PC7 (window) and TIM4_CH1 on PB6 (lock), 20 kHz.
 *
 * Skeleton: the behaviour is delivered at milestone M1; until then every
 * function returns its safe value.
 */

#include "mcal/pwm/pwm.h"

Ls_ReturnType Pwm_Init(void)
{
    return LS_E_NOT_OK;
}

Ls_ReturnType Pwm_SetDuty(Pwm_ChannelType channel, uint16_t dutyPermille)
{
    (void)channel;
    (void)dutyPermille;
    return LS_E_NOT_OK;
}

uint16_t Pwm_GetDuty(Pwm_ChannelType channel)
{
    (void)channel;
    return (uint16_t)0;
}
