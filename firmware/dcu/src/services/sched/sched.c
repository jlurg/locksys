/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file sched.c
 * @brief Time-triggered cooperative scheduler with a static task table.
 */

#include "services/sched/sched.h"

#include "ls_params_gen.h"

static volatile uint32_t s_tick = 0u;
static volatile Sched_TaskIdType s_running = SCHED_TASK_NONE;
static volatile uint32_t s_runStartTick = 0u;
static volatile bool s_hangTripped = false;

static uint32_t s_lastTick = 0u;
static uint32_t s_startTick = 0u;
static uint32_t s_next[SCHED_CFG_TASK_COUNT];
static Sched_StatsType s_stats;

static void Sched_ClearStats(void)
{
    uint32_t i;

    s_stats.overruns = 0u;
    s_stats.missedTicks = 0u;
    s_stats.hangTrips = 0u;
    for (i = 0u; i < SCHED_CFG_TASK_COUNT; i++)
    {
        s_stats.taskMissed[i] = 0u;
        s_stats.taskRuns[i] = 0u;
    }
}

static void Sched_Release(Sched_TaskIdType task, uint32_t now)
{
    const uint32_t period = SchedCfg_Tasks[task].periodMs;
    const uint32_t base = s_startTick + SchedCfg_Tasks[task].offsetMs;

    s_next[task] += period;
    if ((int32_t)(now - s_next[task]) >= 0)
    {
        /* Releases inside a gap of missed ticks are skipped; realign to the task phase. */
        s_stats.taskMissed[task]++;
        s_next[task] = (now - ((now - base) % period)) + period;
    }
}

static void Sched_Run(Sched_TaskIdType task)
{
    s_runStartTick = s_tick;
    s_running = task;
    SchedCfg_TaskBegin(task);
    SchedCfg_RunTask(task);
    SchedCfg_TaskEnd(task);
    s_running = SCHED_TASK_NONE;
    s_stats.taskRuns[task]++;
}

void Sched_Init(void)
{
    Sched_TaskIdType task;

    s_running = SCHED_TASK_NONE;
    s_hangTripped = false;
    s_lastTick = s_tick;
    s_startTick = s_lastTick;
    Sched_ClearStats();
    for (task = 0u; task < SCHED_CFG_TASK_COUNT; task++)
    {
        /* A release at the start tick itself is not dispatched; the first one follows it. */
        s_next[task] = s_startTick + SchedCfg_Tasks[task].offsetMs;
        if (SchedCfg_Tasks[task].offsetMs == 0u)
        {
            s_next[task] += SchedCfg_Tasks[task].periodMs;
        }
    }
}

/* @satisfies SWR-DCU-077 */
void Sched_TickIsr(void)
{
    s_tick = s_tick + 1u;
    if ((s_running != SCHED_TASK_NONE) && (!s_hangTripped) &&
        ((s_tick - s_runStartTick) > LS_T_HANG_DETECT_MS))
    {
        s_hangTripped = true;
        s_stats.hangTrips++;
        SchedCfg_HangReaction();
    }
}

/* @satisfies SWR-DCU-112 */
bool Sched_Dispatch(void)
{
    const uint32_t now = s_tick;
    const uint32_t elapsed = now - s_lastTick;
    Sched_TaskIdType task;

    if (elapsed == 0u)
    {
        return false;
    }
    if (elapsed > 1u)
    {
        s_stats.overruns++;
        s_stats.missedTicks += elapsed - 1u;
    }
    s_lastTick = now;
    for (task = 0u; task < SCHED_CFG_TASK_COUNT; task++)
    {
        if ((int32_t)(now - s_next[task]) >= 0)
        {
            Sched_Release(task, now);
            Sched_Run(task);
        }
    }
    return true;
}

void Sched_Start(void)
{
    Sched_Init();
    for (;;)
    {
        (void)Sched_Dispatch();
    }
}

void Sched_GetStats(Sched_StatsType *stats)
{
    if (stats != NULL)
    {
        *stats = s_stats;
    }
}
