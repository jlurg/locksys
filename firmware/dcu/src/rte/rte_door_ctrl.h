/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file rte_door_ctrl.h
 * @brief RTE view of DoorCtrl: the only RTE header DoorCtrl includes.
 */

#ifndef RTE_DOOR_CTRL_H
#define RTE_DOOR_CTRL_H

#include "rte/rte_types.h"

/**
 * @brief Reads port DoorRequest.
 *
 * @param[out] data Port value.
 * @retval LS_E_OK     Value written.
 * @retval LS_E_NOT_OK @p data is NULL.
 */
Ls_ReturnType Rte_Read_DoorRequest(Rte_DoorRequestType *data);

/**
 * @brief Reads port EcuMode.
 *
 * @param[out] data Port value.
 * @retval LS_E_OK     Value written.
 * @retval LS_E_NOT_OK @p data is NULL.
 */
Ls_ReturnType Rte_Read_EcuMode(Rte_EcuModeType *data);

/**
 * @brief Writes port DoorStatus and forwards it to Com (DCU_DoorSts).
 *
 * @param[in] data Port value.
 */
void Rte_Write_DoorStatus(const Rte_DoorStatusType *data);

/**
 * @brief Ends a running lock pulse (LockAct_Stop).
 */
void Rte_Call_LockAct_Stop(void);

/**
 * @brief Requests SAFE (SafeMon_EnterSafe).
 *
 * @param[in] cause Cause.
 */
void Rte_Call_EnterSafe(Rte_SafeCauseType cause);

/**
 * @brief Returns the millisecond time base (TBase_Ms).
 *
 * @return Time stamp in ms.
 */
uint32_t Rte_Call_TimeMs(void);

#endif /* RTE_DOOR_CTRL_H */
