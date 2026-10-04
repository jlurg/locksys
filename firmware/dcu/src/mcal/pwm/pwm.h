/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file pwm.h
 * @brief Bridge PWM: TIM3_CH2 on PC7 (window) and TIM4_CH1 on PB6 (lock), 20 kHz.
 *
 * PSC 0, ARR 3599 at 72 MHz; compare 0 at start. Unused remapped channels (TIM3 CH1, CH3,
 * CH4) keep CCxE = 0. TIM3 is not used as an ADC trigger.
 */

#ifndef PWM_H
#define PWM_H

#include "platform/ls_std_types.h"

/** @brief PWM channel. */
typedef uint8_t Pwm_ChannelType;

#define PWM_CH_WIN   ((Pwm_ChannelType)0u) /**< Window bridge, TIM3_CH2 */
#define PWM_CH_LOCK  ((Pwm_ChannelType)1u) /**< Lock bridge, TIM4_CH1 */
#define PWM_CH_COUNT (2u)                  /**< Number of channels */

/** @brief Full-scale duty in permille. */
#define PWM_DUTY_MAX (1000u)

/**
 * @brief Configures both timers with compare 0, then switches the PWM pins to the timers.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Pwm_Init(void);

/**
 * @brief Sets the duty of a channel.
 *
 * @param[in] channel Channel.
 * @param[in] dutyPermille Duty, 0 to PWM_DUTY_MAX.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Pwm_SetDuty(Pwm_ChannelType channel, uint16_t dutyPermille);

/**
 * @brief Returns the commanded duty of a channel.
 *
 * @param[in] channel Channel.
 * @return Duty in permille.
 */
uint16_t Pwm_GetDuty(Pwm_ChannelType channel);

#endif /* PWM_H */
