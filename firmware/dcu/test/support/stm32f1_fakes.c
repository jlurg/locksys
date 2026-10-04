/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file stm32f1_fakes.c
 * @brief Register fakes and host core registers of the unit tests.
 */

#include <string.h>

#include "stm32f1_fakes.h"

#include "platform/ls_compiler.h"

RCC_TypeDef LsFake_RCC;
GPIO_TypeDef LsFake_GPIOA;
GPIO_TypeDef LsFake_GPIOB;
GPIO_TypeDef LsFake_GPIOC;
GPIO_TypeDef LsFake_GPIOD;
AFIO_TypeDef LsFake_AFIO;
FLASH_TypeDef LsFake_FLASH;
IWDG_TypeDef LsFake_IWDG;
CRC_TypeDef LsFake_CRC;

uint32_t LsHost_Basepri;
uint32_t LsHost_Primask;

void LsFake_Reset(void)
{
    (void)memset(&LsFake_RCC, 0, sizeof(LsFake_RCC));
    (void)memset(&LsFake_GPIOA, 0, sizeof(LsFake_GPIOA));
    (void)memset(&LsFake_GPIOB, 0, sizeof(LsFake_GPIOB));
    (void)memset(&LsFake_GPIOC, 0, sizeof(LsFake_GPIOC));
    (void)memset(&LsFake_GPIOD, 0, sizeof(LsFake_GPIOD));
    (void)memset(&LsFake_AFIO, 0, sizeof(LsFake_AFIO));
    (void)memset(&LsFake_FLASH, 0, sizeof(LsFake_FLASH));
    (void)memset(&LsFake_IWDG, 0, sizeof(LsFake_IWDG));
    (void)memset(&LsFake_CRC, 0, sizeof(LsFake_CRC));
    LsHost_Basepri = 0u;
    LsHost_Primask = 0u;
}
