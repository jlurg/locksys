/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file win_ctrl.h
 * @brief WinCtrl SWC: hold-to-run window control around the Visual State engine.
 */

#ifndef WIN_CTRL_H
#define WIN_CTRL_H

/**
 * @brief Initialises the adapter, the event set, the timer pool and the engine.
 */
void WinCtrl_Init(void);

/**
 * @brief Runs one adapter cycle: snapshot, timers, events, guards, drain, apply, status.
 *
 * @note 10 ms task; the only caller of the WinCtrl engine.
 */
void WinCtrl_Main10ms(void);

#endif /* WIN_CTRL_H */
