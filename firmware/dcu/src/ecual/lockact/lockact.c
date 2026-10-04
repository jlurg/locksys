/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file lockact.c
 * @brief Lock actuator pulse on bridge channel M2 with hard cap and end detection.
 *
 * Skeleton: the behaviour is delivered at milestone M2; until then every
 * function returns its safe value.
 */

#include "ecual/lockact/lockact.h"

Ls_ReturnType LockAct_Pulse(LockAct_DirType direction, uint16_t durationMs)
{
    (void)direction;
    (void)durationMs;
    return LS_E_NOT_OK;
}

void LockAct_Stop(void)
{
}

bool LockAct_IsActive(void)
{
    return false;
}

LockAct_EndType LockAct_GetEnd(void)
{
    return (LockAct_EndType)0;
}

uint16_t LockAct_GetPeakMa(void)
{
    return (uint16_t)0;
}

void LockAct_Main1ms(void)
{
}

void LockAct_DiagIsr(void)
{
}
