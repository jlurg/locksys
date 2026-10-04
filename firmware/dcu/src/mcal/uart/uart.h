/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file uart.h
 * @brief USART2 (ST-LINK VCP) 115200 8N1 with DMA transmission.
 *
 * USART2 TE stays set (LS-SAIC-001 section 3.3). Reception through DMA1 channel 6 exists in
 * DEV and HIL builds only.
 */

#ifndef UART_H
#define UART_H

#include "platform/ls_std_types.h"

/**
 * @brief Configures USART2 and its DMA channels.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Uart_Init(void);

/**
 * @brief Starts a DMA transmission.
 *
 * @param[in] data Bytes to send; must stay valid until Uart_IsBusy() returns false.
 * @param[in] length Number of bytes.
 * @retval LS_E_OK     Transmission started.
 * @retval LS_E_BUSY   A transmission is running.
 * @retval LS_E_NOT_OK Invalid arguments or driver not ready.
 */
Ls_ReturnType Uart_Send(const uint8_t *data, uint16_t length);

/**
 * @brief Reports whether a transmission is running.
 *
 * @return true while the DMA transmission runs.
 */
bool Uart_IsBusy(void);

/**
 * @brief Copies received bytes from the circular DMA buffer.
 *
 * @param[out] buffer Destination.
 * @param[in] capacity Size of @p buffer.
 * @return Number of bytes copied; 0 in builds without reception.
 */
uint16_t Uart_RxRead(uint8_t *buffer, uint16_t capacity);

/**
 * @brief DMA1 channel 7 transfer-complete interrupt service.
 *
 * @note Interrupt priority 5.
 */
void Uart_DmaTxIsr(void);

#endif /* UART_H */
