/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file can.c
 * @brief bxCAN driver: 500 kbit/s, identifier-list filters, mailboxes, error state.
 *
 * Skeleton: the behaviour is delivered at milestone M1; until then every
 * function returns its safe value.
 */

#include "mcal/can/can.h"

Ls_ReturnType Can_Init(void)
{
    return LS_E_NOT_OK;
}

Ls_ReturnType Can_Start(void)
{
    return LS_E_NOT_OK;
}

Ls_ReturnType Can_Stop(void)
{
    return LS_E_NOT_OK;
}

Ls_ReturnType Can_Write(uint32_t id, const uint8_t *data, uint8_t dlc)
{
    (void)id;
    (void)data;
    (void)dlc;
    return LS_E_NOT_OK;
}

Can_ErrorStateType Can_GetErrorState(void)
{
    return (Can_ErrorStateType)0;
}

Ls_ReturnType Can_Recover(void)
{
    return LS_E_NOT_OK;
}

void Can_RxIsr(void)
{
}

void Can_TxIsr(void)
{
}

void Can_SceIsr(void)
{
}
