/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file adc.h
 * @brief ADC1 regular scan of IN0, IN1, IN4 and IN17 at 1 kHz into DMA1 channel 1.
 *
 * The scan is triggered by TIM1 CC1 (TIM1 outputs disabled: CC1E-CC4E = 0, MOE = 0); TIM3 is
 * never an ADC trigger. Regular conversions only (ES096 section 2.5.1).
 */

#ifndef ADC_H
#define ADC_H

#include "platform/ls_std_types.h"

/** @brief Scan slot. */
typedef uint8_t Adc_ChannelType;

#define ADC_CH_WIN_CS  ((Adc_ChannelType)0u) /**< IN0, window current sense */
#define ADC_CH_LOCK_CS ((Adc_ChannelType)1u) /**< IN1, lock current sense */
#define ADC_CH_KL30    ((Adc_ChannelType)2u) /**< IN4, KL30 divider */
#define ADC_CH_VREFINT ((Adc_ChannelType)3u) /**< IN17, internal reference */
#define ADC_CH_COUNT   (4u)                  /**< Number of slots */

/**
 * @brief Calibrates ADC1 and starts the triggered scan.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Adc_Init(void);

/**
 * @brief Returns the newest 12-bit result of a slot.
 *
 * @param[in] channel Scan slot.
 * @return Raw value 0-4095.
 */
uint16_t Adc_GetRaw(Adc_ChannelType channel);

/**
 * @brief Reports whether a scan completed since the last call.
 *
 * @return true when new results are available.
 */
bool Adc_IsFresh(void);

#endif /* ADC_H */
