/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file enc.c
 * @brief Window encoder on TIM2 (PA15/PB3, remap 01), encoder mode x4.
 *
 * Skeleton: the behaviour is delivered at milestone M2; until then every
 * function returns its safe value.
 */

#include "mcal/enc/enc.h"

Ls_ReturnType Enc_Init(void)
{
    return LS_E_NOT_OK;
}

uint16_t Enc_GetCount(void)
{
    return (uint16_t)0;
}
