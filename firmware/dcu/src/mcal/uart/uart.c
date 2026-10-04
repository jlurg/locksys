/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file uart.c
 * @brief USART2 (ST-LINK VCP) 115200 8N1 with DMA transmission.
 *
 * Skeleton: the behaviour is delivered at milestone M1; until then every
 * function returns its safe value.
 */

#include "mcal/uart/uart.h"

Ls_ReturnType Uart_Init(void)
{
    return LS_E_NOT_OK;
}

Ls_ReturnType Uart_Send(const uint8_t *data, uint16_t length)
{
    (void)data;
    (void)length;
    return LS_E_NOT_OK;
}

bool Uart_IsBusy(void)
{
    return false;
}

uint16_t Uart_RxRead(uint8_t *buffer, uint16_t capacity)
{
    (void)buffer;
    (void)capacity;
    return (uint16_t)0;
}

void Uart_DmaTxIsr(void)
{
}
