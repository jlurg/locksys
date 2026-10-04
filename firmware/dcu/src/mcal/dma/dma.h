/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file dma.h
 * @brief DMA1 channels 1 (ADC1), 6 (USART2 RX, DEV and HIL) and 7 (USART2 TX).
 */

#ifndef DMA_H
#define DMA_H

#include "platform/ls_std_types.h"

/**
 * @brief Enables DMA1 and sets the static channel configuration.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Dma_Init(void);

#endif /* DMA_H */
