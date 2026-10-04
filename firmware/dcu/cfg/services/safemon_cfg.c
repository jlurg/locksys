/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file safemon_cfg.c
 * @brief Binding of the SafeMon reactions to the output drivers.
 */

#include "services/safemon_cfg.h"

#include "ecual/hbridge/hbridge.h"

void SafeMonCfg_SafeOutputs(void)
{
    HBridge_AllOff();
}
