/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file sched.h
 * @brief Time-triggered cooperative scheduler with a static task table.
 *
 * Tasks run in table order within one tick and never preempt each other. Missed ticks are
 * counted, not replayed. The table and the task bodies are bound in cfg/services/sched_cfg.c.
 */

#ifndef SCHED_H
#define SCHED_H

#include "platform/ls_std_types.h"
#include "services/sched_cfg.h"

/** @brief Task identifier (SCHED_TASK_*). */
typedef uint8_t Sched_TaskIdType;

/** @brief No task running. */
#define SCHED_TASK_NONE ((Sched_TaskIdType)0xFFu)

/** @brief Static task configuration. */
typedef struct
{
    uint16_t periodMs; /**< Release period in ms, at least 1 */
    uint16_t offsetMs; /**< Release offset in ms, below the period */
} Sched_TaskCfgType;

/** @brief Scheduler statistics. */
typedef struct
{
    uint32_t overruns;                         /**< Dispatches that found more than one new tick */
    uint32_t missedTicks;                      /**< Ticks not dispatched */
    uint32_t taskMissed[SCHED_CFG_TASK_COUNT]; /**< Releases skipped per task */
    uint32_t taskRuns[SCHED_CFG_TASK_COUNT];   /**< Executions per task */
    uint32_t hangTrips;                        /**< Hang monitor trips */
} Sched_StatsType;

/** @brief Task table, indexed by Sched_TaskIdType; defined in sched_cfg.c. */
extern const Sched_TaskCfgType SchedCfg_Tasks[SCHED_CFG_TASK_COUNT];

/**
 * @brief Runs the runnables of a task in their configured order; defined in sched_cfg.c.
 *
 * @param[in] task Task to run.
 */
void SchedCfg_RunTask(Sched_TaskIdType task);

/**
 * @brief Called before a task runs (TRACE0 high); defined in sched_cfg.c.
 *
 * @param[in] task Task about to run.
 */
void SchedCfg_TaskBegin(Sched_TaskIdType task);

/**
 * @brief Called after a task has run (TRACE0 low); defined in sched_cfg.c.
 *
 * @param[in] task Task that ran.
 */
void SchedCfg_TaskEnd(Sched_TaskIdType task);

/**
 * @brief Hang monitor reaction: all bridges off; defined in sched_cfg.c.
 *
 * @note SysTick interrupt context.
 */
void SchedCfg_HangReaction(void);

/**
 * @brief Resets the statistics and aligns the task releases to the current tick.
 */
void Sched_Init(void);

/**
 * @brief Counts one tick and runs the hang monitor.
 *
 * @note SysTick interrupt only.
 */
void Sched_TickIsr(void);

/**
 * @brief Runs every task released by the newest tick, in table order.
 *
 * @retval true  A new tick was processed.
 * @retval false No new tick since the last call.
 */
bool Sched_Dispatch(void);

/**
 * @brief Initialises the scheduler and runs the dispatch loop; does not return.
 */
void Sched_Start(void);

/**
 * @brief Copies the scheduler statistics.
 *
 * @param[out] stats Destination; not written when NULL.
 */
void Sched_GetStats(Sched_StatsType *stats);

#endif /* SCHED_H */
