/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file rte_win_ctrl.h
 * @brief RTE view of WinCtrl: the only RTE header WinCtrl includes.
 */

#ifndef RTE_WIN_CTRL_H
#define RTE_WIN_CTRL_H

#include "rte/rte_types.h"

/**
 * @brief Reads port WinRequest.
 *
 * @param[out] data Port value.
 * @retval LS_E_OK     Value written.
 * @retval LS_E_NOT_OK @p data is NULL.
 */
Ls_ReturnType Rte_Read_WinRequest(Rte_WinRequestType *data);

/**
 * @brief Reads port EcuMode.
 *
 * @param[out] data Port value.
 * @retval LS_E_OK     Value written.
 * @retval LS_E_NOT_OK @p data is NULL.
 */
Ls_ReturnType Rte_Read_EcuMode(Rte_EcuModeType *data);

/**
 * @brief Writes port WinStatus and forwards it to Com (DCU_WinSts).
 *
 * @param[in] data Port value.
 */
void Rte_Write_WinStatus(const Rte_WinStatusType *data);

/**
 * @brief Applies a window bridge command (HBridge_Set).
 *
 * @param[in] cmd          Bridge command.
 * @param[in] dutyPermille Drive duty in permille.
 * @retval LS_E_OK     Command applied.
 * @retval LS_E_NOT_OK Drive refused.
 */
Ls_ReturnType Rte_Call_WinBridge_Set(Rte_BridgeCmdType cmd, uint16_t dutyPermille);

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

#endif /* RTE_WIN_CTRL_H */
