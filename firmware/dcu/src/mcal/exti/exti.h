/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file exti.h
 * @brief External interrupt lines of the EN/DIAG reflexes, B1 and PVD.
 */

#ifndef EXTI_H
#define EXTI_H

#include "platform/ls_std_types.h"

/**
 * @brief Configures lines 10 (window EN/DIAG), 4 or 6 (lock EN/DIAG), 13 (B1, DEV) and 16 (PVD), falling edge.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Exti_Init(void);

#endif /* EXTI_H */
