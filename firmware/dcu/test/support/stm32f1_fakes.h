/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file stm32f1_fakes.h
 * @brief Register fakes of the STM32F103 peripherals used by the MCAL unit tests.
 *
 * Each peripheral instance macro of the CMSIS device header is redirected to a plain object.
 * Writes are stored and reads return the stored value; hardware side effects (BSRR to ODR,
 * ready flags) are not modelled. A test sets status bits it needs before calling the code.
 */

#ifndef STM32F1_FAKES_H
#define STM32F1_FAKES_H

#include "stm32f103xb.h"

extern RCC_TypeDef LsFake_RCC;
extern GPIO_TypeDef LsFake_GPIOA;
extern GPIO_TypeDef LsFake_GPIOB;
extern GPIO_TypeDef LsFake_GPIOC;
extern GPIO_TypeDef LsFake_GPIOD;
extern AFIO_TypeDef LsFake_AFIO;
extern FLASH_TypeDef LsFake_FLASH;
extern IWDG_TypeDef LsFake_IWDG;
extern CRC_TypeDef LsFake_CRC;

#undef RCC
#undef GPIOA
#undef GPIOB
#undef GPIOC
#undef GPIOD
#undef AFIO
#undef FLASH
#undef IWDG
#undef CRC

#define RCC   (&LsFake_RCC)   /**< RCC fake */
#define GPIOA (&LsFake_GPIOA) /**< GPIOA fake */
#define GPIOB (&LsFake_GPIOB) /**< GPIOB fake */
#define GPIOC (&LsFake_GPIOC) /**< GPIOC fake */
#define GPIOD (&LsFake_GPIOD) /**< GPIOD fake */
#define AFIO  (&LsFake_AFIO)  /**< AFIO fake */
#define FLASH (&LsFake_FLASH) /**< FLASH interface fake */
#define IWDG  (&LsFake_IWDG)  /**< IWDG fake */
#define CRC   (&LsFake_CRC)   /**< CRC unit fake */

/**
 * @brief Resets every register fake to zero.
 */
void LsFake_Reset(void);

#endif /* STM32F1_FAKES_H */
