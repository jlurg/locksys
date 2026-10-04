/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file wdg.h
 * @brief Independent watchdog: prescaler /4, reload 499, 50 ms nominal (33-67 ms over LSI).
 */

#ifndef WDG_H
#define WDG_H

#include "platform/ls_std_types.h"

/**
 * @brief Starts the IWDG. Once started it cannot be stopped.
 *
 * @retval LS_E_OK     IWDG running with the configured timeout.
 * @retval LS_E_NOT_OK The prescaler or reload update did not complete in time; the IWDG runs
 *                     with the values accepted so far.
 */
Ls_ReturnType Wdg_Start(void);

/**
 * @brief Reloads the IWDG counter.
 *
 * @note Called by EcuM during initialisation and by WdgM only; never from an interrupt.
 */
void Wdg_Refresh(void);

#endif /* WDG_H */
