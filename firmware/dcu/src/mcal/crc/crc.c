/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file crc.c
 * @brief CRC-32 unit.
 */

#include "mcal/crc/crc.h"

#include "stm32f1xx.h"

void Crc_Reset(void)
{
    RCC->AHBENR |= RCC_AHBENR_CRCEN;
    (void)RCC->AHBENR;
    CRC->CR = CRC_CR_RESET;
}

void Crc_Feed(const uint32_t *words, uint32_t count)
{
    uint32_t i;

    if (words != NULL)
    {
        for (i = 0u; i < count; i++)
        {
            CRC->DR = words[i];
        }
    }
}

uint32_t Crc_Value(void)
{
    return CRC->DR;
}
