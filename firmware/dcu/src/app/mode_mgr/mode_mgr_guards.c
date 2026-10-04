/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file mode_mgr_guards.c
 * @brief ModeMgr guards: pure functions.
 */

#include "app/mode_mgr/mode_mgr_priv.h"

bool ModeMgr_GuardSafeRequest(bool safeLatched, bool criticalActive)
{
    return safeLatched || criticalActive;
}
