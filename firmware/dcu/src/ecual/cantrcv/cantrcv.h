/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cantrcv.h
 * @brief CAN transceiver mode interface.
 *
 * The SN65HVD230 board has no standby pin: the mode is always NORMAL. Standby control is LATER
 * and changes only this module.
 */

#ifndef CANTRCV_H
#define CANTRCV_H

#include "platform/ls_std_types.h"

/** @brief Transceiver mode. */
typedef uint8_t CanTrcv_ModeType;

#define CAN_TRCV_MODE_NORMAL  ((CanTrcv_ModeType)0u) /**< Normal */
#define CAN_TRCV_MODE_STANDBY ((CanTrcv_ModeType)1u) /**< Standby (LATER) */

/**
 * @brief Initialises the transceiver interface.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType CanTrcv_Init(void);

/**
 * @brief Returns the transceiver mode.
 *
 * @return CAN_TRCV_MODE_NORMAL.
 */
CanTrcv_ModeType CanTrcv_GetMode(void);

#endif /* CANTRCV_H */
