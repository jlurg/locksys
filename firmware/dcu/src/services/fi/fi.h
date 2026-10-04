/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file fi.h
 * @brief Fault injection through `!LSFI` commands (LS-IF-003 section 7); DEV and HIL builds only.
 *
 * With LS_CFG_FI = 0 the module contains no code, so RC and RELEASE images contain no Fi_
 * symbols.
 */

#ifndef FI_H
#define FI_H

#include "platform/ls_std_types.h"

/**
 * @brief Parses `!LSFI` commands from the USART2 RX buffer and applies the injections.
 *
 * @note 10 ms task.
 */
void Fi_Main10ms(void);

#endif /* FI_H */
