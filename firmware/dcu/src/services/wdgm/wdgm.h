/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file wdgm.h
 * @brief Watchdog manager: alive supervision of the tasks and the only IWDG refresh after start-up.
 */

#ifndef WDGM_H
#define WDGM_H

#include "platform/ls_std_types.h"

/** @brief Supervised entity: one per task. */
typedef uint8_t WdgM_EntityType;

#define WDGM_SE_T1    ((WdgM_EntityType)0u) /**< 1 ms task: 10 +- 1 checkpoints per 10 ms */
#define WDGM_SE_T5    ((WdgM_EntityType)1u) /**< 5 ms task: 2 +- 1 per 10 ms */
#define WDGM_SE_T10   ((WdgM_EntityType)2u) /**< 10 ms task: 1 per 10 ms */
#define WDGM_SE_T100  ((WdgM_EntityType)3u) /**< 100 ms task: 1 per 100 ms */
#define WDGM_SE_T1000 ((WdgM_EntityType)4u) /**< 1000 ms task: 1 per 1000 ms */
#define WDGM_SE_COUNT (5u)                  /**< Number of supervised entities */

/**
 * @brief Clears the checkpoint counters.
 */
void WdgM_Init(void);

/**
 * @brief Records one alive checkpoint of a supervised entity.
 *
 * @param[in] entity Supervised entity.
 */
void WdgM_Checkpoint(WdgM_EntityType entity);

/**
 * @brief Checks the checkpoint counters and refreshes the IWDG.
 *
 * @note 10 ms task, last runnable. Skeleton: the alive windows are evaluated from milestone
 *       M1; until then the IWDG is refreshed while the 10 ms task runs.
 */
void WdgM_Main10ms(void);

#endif /* WDGM_H */
