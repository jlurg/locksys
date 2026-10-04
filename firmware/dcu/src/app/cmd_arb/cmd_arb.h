/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cmd_arb.h
 * @brief Communication-level validation of window and door commands.
 */

#ifndef CMD_ARB_H
#define CMD_ARB_H

#include "platform/ls_std_types.h"

/**
 * @brief Initialises the arbitration state (not re-armed, no PressId seen).
 */
void CmdArb_Init(void);

/**
 * @brief Validates CGW_WinCmd, CGW_DoorCmd and CGW_NodeSts and writes the request ports.
 *
 * @note 10 ms task.
 */
void CmdArb_Main10ms(void);

#endif /* CMD_ARB_H */
