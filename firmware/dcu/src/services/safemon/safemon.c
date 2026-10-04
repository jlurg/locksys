/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file safemon.c
 * @brief Safety monitor.
 *
 * Skeleton: SAFE entry and the fault entry switch the outputs off; the noinit fault record,
 * stack painting and the ROM CRC check are delivered at milestone M1.
 */

#include "services/safemon/safemon.h"

#include "platform/ls_compiler.h"
#include "services/safemon_cfg.h"

static volatile SafeMon_CauseType s_cause = SAFEMON_CAUSE_NONE;

void SafeMon_Init(void)
{
    s_cause = SAFEMON_CAUSE_NONE;
}

void SafeMon_EnterSafe(SafeMon_CauseType cause)
{
    SafeMonCfg_SafeOutputs();
    if (s_cause == SAFEMON_CAUSE_NONE)
    {
        s_cause = cause;
    }
}

bool SafeMon_IsSafe(void)
{
    return (s_cause != SAFEMON_CAUSE_NONE);
}

void SafeMon_FaultEntry(void)
{
    LS_DISABLE_IRQ();
    SafeMonCfg_SafeOutputs();
    for (;;)
    {
        /* The IWDG resets the MCU; it is never refreshed from here. */
    }
}

Ls_ReturnType SafeMon_RomCheckFull(void)
{
    return LS_E_NOT_OK;
}

void SafeMon_Main100ms(void)
{
}
