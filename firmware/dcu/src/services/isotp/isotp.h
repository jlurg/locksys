/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file isotp.h
 * @brief ISO 15765-2 transport on 0x7A0/0x7A8.
 */

#ifndef ISOTP_H
#define ISOTP_H

#include "platform/ls_std_types.h"

/**
 * @brief Resets the transport state.
 */
void IsoTp_Init(void);

/**
 * @brief Segmentation, reassembly and flow control.
 *
 * @note 5 ms task.
 */
void IsoTp_Main5ms(void);

#endif /* ISOTP_H */
