/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file det.h
 * @brief Development error reporting (DEV and HIL builds).
 *
 * With LS_CFG_DET = 0 the report compiles to nothing; the caller keeps its safe default branch.
 */

#ifndef DET_H
#define DET_H

#include "platform/ls_std_types.h"

/**
 * @brief Reports a development error: `#LOG E DET`, entry store and breakpoint with a debugger attached.
 *
 * @param[in] moduleId Reporting module.
 * @param[in] apiId Reporting API.
 * @param[in] errorId Error.
 */
void Det_Report(uint8_t moduleId, uint8_t apiId, uint8_t errorId);

#endif /* DET_H */
