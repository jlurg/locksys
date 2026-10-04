/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file clk.h
 * @brief Clock tree, flash wait states and reset flags (RCC).
 *
 * Profile HSE72: HSE bypass 8 MHz from the ST-LINK MCO, PLL x9 = 72 MHz, APB1 36 MHz,
 * APB2 72 MHz, ADC 12 MHz, flash 2 wait states. Profile HSI64 (fallback, SAFE only):
 * HSI/2 x16 = 64 MHz, APB1 32 MHz, ADC 10.67 MHz.
 */

#ifndef CLK_H
#define CLK_H

#include "platform/ls_std_types.h"

/** @brief Active clock profile. */
typedef uint8_t Clk_ProfileType;

#define CLK_PROFILE_RESET ((Clk_ProfileType)0u) /**< HSI 8 MHz after reset; Clk_Init() not run */
#define CLK_PROFILE_HSE72 ((Clk_ProfileType)1u) /**< HSE bypass, 72 MHz */
#define CLK_PROFILE_HSI64 ((Clk_ProfileType)2u) /**< HSI fallback, 64 MHz */

/** @brief Reset flags of RCC_CSR (bits 26-31), right-aligned. */
typedef uint8_t Clk_ResetFlagsType;

#define CLK_RESET_FLAG_PIN  ((Clk_ResetFlagsType)0x01u) /**< PINRSTF */
#define CLK_RESET_FLAG_POR  ((Clk_ResetFlagsType)0x02u) /**< PORRSTF */
#define CLK_RESET_FLAG_SFT  ((Clk_ResetFlagsType)0x04u) /**< SFTRSTF */
#define CLK_RESET_FLAG_IWDG ((Clk_ResetFlagsType)0x08u) /**< IWDGRSTF */
#define CLK_RESET_FLAG_WWDG ((Clk_ResetFlagsType)0x10u) /**< WWDGRSTF */
#define CLK_RESET_FLAG_LPWR ((Clk_ResetFlagsType)0x20u) /**< LPWRRSTF */

/**
 * @brief Configures the clock tree: HSE72, or HSI64 when HSE is not ready within 5 ms.
 *
 * @retval LS_E_OK     HSE72 profile active.
 * @retval LS_E_NOT_OK HSE or PLL failed; HSI64 profile active (the caller enters SAFE).
 * @pre Called once at start-up, before any peripheral that depends on the bus clocks.
 */
Ls_ReturnType Clk_Init(void);

/**
 * @brief Returns the active clock profile.
 *
 * @return One of CLK_PROFILE_*.
 */
Clk_ProfileType Clk_GetProfile(void);

/**
 * @brief Returns the system clock (HCLK) frequency.
 *
 * @return Frequency in Hz.
 */
uint32_t Clk_GetSysclkHz(void);

/**
 * @brief Returns the APB1 clock frequency.
 *
 * @return Frequency in Hz.
 */
uint32_t Clk_GetPclk1Hz(void);

/**
 * @brief Returns the reset flags captured from RCC_CSR.
 *
 * @return Bit set of CLK_RESET_FLAG_*.
 */
Clk_ResetFlagsType Clk_GetResetFlags(void);

/**
 * @brief Clears the reset flags in RCC_CSR (RMVF).
 */
void Clk_ClearResetFlags(void);

/**
 * @brief Clock security system reaction, called from the NMI handler.
 *
 * @note Implemented at milestone M6; until then the CSS stays disabled.
 */
void Clk_CssNmi(void);

#endif /* CLK_H */
