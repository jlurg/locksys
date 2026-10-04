/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file pwr.c
 * @brief Programmable voltage detector and backup registers.
 *
 * Skeleton: the behaviour is delivered at milestone M1; until then every
 * function returns its safe value.
 */

#include "mcal/pwr/pwr.h"

Ls_ReturnType Pwr_Init(void)
{
    return LS_E_NOT_OK;
}

void Pwr_PvdIsr(void)
{
}

uint16_t Pwr_BkpRead(uint8_t index)
{
    (void)index;
    return (uint16_t)0;
}

Ls_ReturnType Pwr_BkpWrite(uint8_t index, uint16_t value)
{
    (void)index;
    (void)value;
    return LS_E_NOT_OK;
}
