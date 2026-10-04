/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file tempsens.c
 * @brief Temperature sensor port; the TMP117 adapter is the default.
 *
 * Skeleton: the behaviour is delivered at milestone M2; until then every
 * function returns its safe value.
 */

#include "ecual/tempsens/tempsens.h"

Ls_ReturnType TempSens_StartIdentify(void)
{
    return LS_E_NOT_OK;
}

Ls_ReturnType TempSens_StartSample(void)
{
    return LS_E_NOT_OK;
}

Ls_ReturnType TempSens_Poll(TempSens_ResultType *result)
{
    (void)result;
    return LS_E_NOT_OK;
}
