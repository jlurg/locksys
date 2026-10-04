/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file fi.c
 * @brief Fault injection through `!LSFI` commands (LS-IF-003 section 7); DEV and HIL builds only.
 *
 * Skeleton: the behaviour is delivered at milestone M1; until then every
 * function returns its safe value.
 */

#include "services/fi/fi.h"

#include "ls_cfg.h"

#if LS_CFG_FI == 1

void Fi_Main10ms(void)
{
}

#endif /* LS_CFG_FI == 1 */
