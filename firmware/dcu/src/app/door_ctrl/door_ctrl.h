/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file door_ctrl.h
 * @brief DoorCtrl SWC: lock transactions around the Visual State engine.
 */

#ifndef DOOR_CTRL_H
#define DOOR_CTRL_H

/**
 * @brief Initialises the adapter, the event set, the timer pool and the engine.
 */
void DoorCtrl_Init(void);

/**
 * @brief Runs one adapter cycle.
 *
 * @note 10 ms task; the only caller of the DoorCtrl engine.
 */
void DoorCtrl_Main10ms(void);

#endif /* DOOR_CTRL_H */
