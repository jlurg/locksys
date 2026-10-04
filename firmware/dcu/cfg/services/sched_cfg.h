/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file sched_cfg.h
 * @brief Task identifiers of the static task table (LS-DCU-SAD-001 section 5.1).
 */

#ifndef SCHED_CFG_H
#define SCHED_CFG_H

#define SCHED_TASK_T1        (0u) /**< 1 ms, offset 0 */
#define SCHED_TASK_T5        (1u) /**< 5 ms, offset 1 */
#define SCHED_TASK_T10       (2u) /**< 10 ms, offset 2 */
#define SCHED_TASK_T100      (3u) /**< 100 ms, offset 4 */
#define SCHED_TASK_T1000     (4u) /**< 1000 ms, offset 8 */
#define SCHED_CFG_TASK_COUNT (5u) /**< Number of tasks */

#endif /* SCHED_CFG_H */
