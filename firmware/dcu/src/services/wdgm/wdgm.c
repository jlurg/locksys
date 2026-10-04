/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file wdgm.c
 * @brief Watchdog manager.
 */

#include "services/wdgm/wdgm.h"

#include "mcal/wdg/wdg.h"

static uint16_t s_checkpoints[WDGM_SE_COUNT];

void WdgM_Init(void)
{
    WdgM_EntityType entity;

    for (entity = 0u; entity < WDGM_SE_COUNT; entity++)
    {
        s_checkpoints[entity] = 0u;
    }
}

void WdgM_Checkpoint(WdgM_EntityType entity)
{
    if ((entity < WDGM_SE_COUNT) && (s_checkpoints[entity] < UINT16_MAX))
    {
        s_checkpoints[entity]++;
    }
}

void WdgM_Main10ms(void)
{
    Wdg_Refresh();
}
