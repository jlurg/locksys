/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file rte_mode_mgr.h
 * @brief RTE view of ModeMgr: the only RTE header ModeMgr includes.
 */

#ifndef RTE_MODE_MGR_H
#define RTE_MODE_MGR_H

#include "rte/rte_types.h"

/**
 * @brief Reads port CgwStatus.
 *
 * @param[out] data Port value.
 * @retval LS_E_OK     Value written.
 * @retval LS_E_NOT_OK @p data is NULL.
 */
Ls_ReturnType Rte_Read_CgwStatus(Rte_CgwStatusType *data);

/**
 * @brief Reads port WinStatus.
 *
 * @param[out] data Port value.
 * @retval LS_E_OK     Value written.
 * @retval LS_E_NOT_OK @p data is NULL.
 */
Ls_ReturnType Rte_Read_WinStatus(Rte_WinStatusType *data);

/**
 * @brief Reads port DoorStatus.
 *
 * @param[out] data Port value.
 * @retval LS_E_OK     Value written.
 * @retval LS_E_NOT_OK @p data is NULL.
 */
Ls_ReturnType Rte_Read_DoorStatus(Rte_DoorStatusType *data);

/**
 * @brief Writes port EcuMode.
 *
 * @param[in] data Port value.
 */
void Rte_Write_EcuMode(const Rte_EcuModeType *data);

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

#endif /* RTE_MODE_MGR_H */
