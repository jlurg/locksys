/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ecum.c
 * @brief ECU state manager.
 *
 * Skeleton: the start-up order is implemented; the noinit record, the reset counters, the
 * ROM CRC step and the SAFE latch are delivered at milestone M1.
 */

#include "services/ecum/ecum.h"

#include "mcal/clk/clk.h"
#include "mcal/nvic/nvic.h"
#include "mcal/stk/stk.h"
#include "mcal/wdg/wdg.h"
#include "services/ecum_cfg.h"
#include "services/safemon/safemon.h"

#define ECUM_RESET_PRIO_COUNT (6u)

typedef struct
{
    Clk_ResetFlagsType flag;
    Ls_ResetReasonType reason;
} EcuM_ResetPrioType;

/* Priority IWDG > WWDG > SFT > LPWR > POR > PIN; BROWNOUT is never reported. */
static const EcuM_ResetPrioType s_resetPriority[ECUM_RESET_PRIO_COUNT] = {
    {CLK_RESET_FLAG_IWDG, LS_RESET_REASON_WATCHDOG},
    {CLK_RESET_FLAG_WWDG, LS_RESET_REASON_WINDOW_WATCHDOG},
    {CLK_RESET_FLAG_SFT, LS_RESET_REASON_SOFTWARE},
    {CLK_RESET_FLAG_LPWR, LS_RESET_REASON_LOW_POWER},
    {CLK_RESET_FLAG_POR, LS_RESET_REASON_POWER_ON},
    {CLK_RESET_FLAG_PIN, LS_RESET_REASON_PIN},
};

static Ls_ResetReasonType s_resetReason = LS_RESET_REASON_UNKNOWN;

/* @satisfies SWR-DCU-070 */
void EcuM_EarlyInit(void)
{
    EcuMCfg_SafeOutputs();
}

/* @satisfies SWR-DCU-076 */
void EcuM_Init(void)
{
    const Ls_ReturnType clock = Clk_Init();

    Nvic_Init();
    (void)Stk_Init(Clk_GetSysclkHz());
    s_resetReason = EcuM_DecodeResetReason(Clk_GetResetFlags());
    Clk_ClearResetFlags();
    SafeMon_Init();
    if (clock != LS_E_OK)
    {
        SafeMon_EnterSafe(SAFEMON_CAUSE_CLOCK);
    }

    (void)Wdg_Start();
    EcuMCfg_InitDrivers();
    Wdg_Refresh();
    EcuMCfg_InitEcual();
    Wdg_Refresh();
    EcuMCfg_InitServices();
    Wdg_Refresh();
    EcuMCfg_InitSwcs();
    Wdg_Refresh();
}

/* @satisfies SWR-DCU-080 */
Ls_ResetReasonType EcuM_DecodeResetReason(uint8_t flags)
{
    Ls_ResetReasonType reason = LS_RESET_REASON_UNKNOWN;
    uint32_t i;

    for (i = 0u; (i < ECUM_RESET_PRIO_COUNT) && (reason == LS_RESET_REASON_UNKNOWN); i++)
    {
        if ((flags & s_resetPriority[i].flag) != 0u)
        {
            reason = s_resetPriority[i].reason;
        }
    }
    return reason;
}

Ls_ResetReasonType EcuM_GetResetReason(void)
{
    return s_resetReason;
}

void EcuM_RequestReset(void)
{
    EcuMCfg_SafeOutputs();
    Nvic_SystemReset();
}
