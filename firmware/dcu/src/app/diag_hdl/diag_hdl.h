/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file diag_hdl.h
 * @brief DID read handlers, ECU reset and DTC clear coordination.
 *
 * DID 0xFD09 (transition coverage bitmap and trace ring of the three adapters) is reserved
 * with layout version 1.
 */

#ifndef DIAG_HDL_H
#define DIAG_HDL_H

#include "platform/ls_std_types.h"

/** @brief DID of the transition coverage bitmap and trace ring. */
#define DIAG_HDL_DID_TRANSITION_TRACE (0xFD09u)

/**
 * @brief Reads the data of a DID.
 *
 * @param[in] did Data identifier.
 * @param[out] buffer Response data.
 * @param[in] capacity Size of @p buffer.
 * @param[out] length Number of bytes written.
 * @retval LS_E_OK     Data written.
 * @retval LS_E_NOT_OK DID not supported or buffer too small.
 */
Ls_ReturnType DiagHdl_ReadDid(uint16_t did, uint8_t *buffer, uint16_t capacity, uint16_t *length);

/**
 * @brief Coordinates an ECU reset (SAFE latch rule).
 *
 * @param[in] extendedSession Request received in the extended session.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType DiagHdl_EcuReset(bool extendedSession);

/**
 * @brief Coordinates clearing of the fault memory.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType DiagHdl_ClearDtc(void);

#endif /* DIAG_HDL_H */
