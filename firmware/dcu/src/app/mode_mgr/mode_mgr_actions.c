/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file mode_mgr_actions.c
 * @brief Bodies of the ModeMgr engine actions.
 */

#include "app/mode_mgr/mode_mgr_priv.h"

static Ls_NodeModeType s_mode = LS_NODE_MODE_INIT;

void ModeMgr_ActSetMode(Ls_NodeModeType mode)
{
    s_mode = mode;
}

Ls_NodeModeType ModeMgr_ActionsMode(void)
{
    return s_mode;
}
