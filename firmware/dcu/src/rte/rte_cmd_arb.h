/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file rte_cmd_arb.h
 * @brief RTE view of CmdArb: the only RTE header CmdArb includes.
 */

#ifndef RTE_CMD_ARB_H
#define RTE_CMD_ARB_H

#include "rte/rte_types.h"

/**
 * @brief Writes port WinRequest.
 *
 * @param[in] data Port value.
 */
void Rte_Write_WinRequest(const Rte_WinRequestType *data);

/**
 * @brief Writes port DoorRequest.
 *
 * @param[in] data Port value.
 */
void Rte_Write_DoorRequest(const Rte_DoorRequestType *data);

/**
 * @brief Writes port CgwStatus.
 *
 * @param[in] data Port value.
 */
void Rte_Write_CgwStatus(const Rte_CgwStatusType *data);

#endif /* RTE_CMD_ARB_H */
