/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file vbatmon.c
 * @brief KL30 supply monitoring from PA4 with VDDA plausibility.
 *
 * Skeleton: the behaviour is delivered at milestone M2; until then every
 * function returns its safe value.
 */

#include "ecual/vbatmon/vbatmon.h"

void VbatMon_Main10ms(void)
{
}

Ls_ReturnType VbatMon_Get(VbatMon_StatusType *status)
{
    (void)status;
    return LS_E_NOT_OK;
}
