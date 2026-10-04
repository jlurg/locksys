/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file mode_mgr.c
 * @brief ModeMgr adapter: the only file that calls the ModeMgr engine.
 *
 * Skeleton: with LS_CFG_VS_ENGINE_PRESENT = 0 NodeMode stays INIT with every actuator
 * inhibited; a SAFE latch is forwarded. The engine path and the inhibit computation are
 * delivered at milestone M2.
 */

#include "app/mode_mgr/mode_mgr.h"

#include "app/mode_mgr/mode_mgr_priv.h"
#include "ls_cfg.h"

static LsEvSet_Type s_events;
static bool s_safeLatched = false;

void ModeMgr_Init(void)
{
    (void)LsEvSet_Init(&s_events, MODE_MGR_DRAIN_BUDGET);
    ModeMgr_ActSetMode(LS_NODE_MODE_INIT);
    s_safeLatched = false;
}

void ModeMgr_Main10ms(void)
{
    Rte_EcuModeType mode;

    if (ModeMgr_GuardSafeRequest(s_safeLatched, false))
    {
        (void)LsEvSet_Post(&s_events, MODE_MGR_EV_SAFE_REQ);
    }
    /* Without a linked engine the events have no consumer. */
    LsEvSet_Clear(&s_events);

    mode.mode = ModeMgr_ActionsMode();
    mode.inhibitUp = true;
    mode.inhibitDown = true;
    mode.inhibitLock = true;
    mode.safeLatched = s_safeLatched;
    Rte_Write_EcuMode(&mode);
}
