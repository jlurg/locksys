/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file stm32f1xx.h
 * @brief Host replacement of the CMSIS device header for unit tests.
 *
 * Found before the vendored header because test/support precedes the CMSIS system include
 * directories. Provides the real register types and redirects the peripheral instances to the
 * register fakes of stm32f1_fakes.h.
 */

#ifndef LS_TEST_STM32F1XX_H
#define LS_TEST_STM32F1XX_H

#include "stm32f103xb.h"
#include "stm32f1_fakes.h"

#endif /* LS_TEST_STM32F1XX_H */
