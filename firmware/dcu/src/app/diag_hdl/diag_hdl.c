/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file diag_hdl.c
 * @brief DID read handlers, ECU reset and DTC clear coordination.
 *
 * Skeleton: the behaviour is delivered at milestone M2; until then every
 * function returns its safe value.
 */

#include "app/diag_hdl/diag_hdl.h"

Ls_ReturnType DiagHdl_ReadDid(uint16_t did, uint8_t *buffer, uint16_t capacity, uint16_t *length)
{
    (void)did;
    (void)buffer;
    (void)capacity;
    (void)length;
    return LS_E_NOT_OK;
}

Ls_ReturnType DiagHdl_EcuReset(bool extendedSession)
{
    (void)extendedSession;
    return LS_E_NOT_OK;
}

Ls_ReturnType DiagHdl_ClearDtc(void)
{
    return LS_E_NOT_OK;
}
