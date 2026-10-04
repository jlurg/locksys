/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "unity.h"

#include "ls_params_gen.h"
#include "services/sched/sched.h"

/* Test doubles of the task table and the bindings of cfg/services/sched_cfg.c. */
const Sched_TaskCfgType SchedCfg_Tasks[SCHED_CFG_TASK_COUNT] = {
    {1u, 0u}, {5u, 1u}, {10u, 2u}, {100u, 4u}, {1000u, 8u},
};

#define LOG_SIZE (16u)

static Sched_TaskIdType s_log[LOG_SIZE];
static uint32_t s_logCount;
static uint32_t s_begins;
static uint32_t s_ends;
static uint32_t s_hangReactions;
static uint32_t s_ticksInsideTask;
static Sched_TaskIdType s_tickingTask;

void SchedCfg_RunTask(Sched_TaskIdType task)
{
    uint32_t i;

    if (s_logCount < LOG_SIZE)
    {
        s_log[s_logCount] = task;
    }
    s_logCount++;
    if (task == s_tickingTask)
    {
        for (i = 0u; i < s_ticksInsideTask; i++)
        {
            Sched_TickIsr();
        }
    }
}

void SchedCfg_TaskBegin(Sched_TaskIdType task)
{
    (void)task;
    s_begins++;
}

void SchedCfg_TaskEnd(Sched_TaskIdType task)
{
    (void)task;
    s_ends++;
}

void SchedCfg_HangReaction(void)
{
    s_hangReactions++;
}

static void Tick(uint32_t count)
{
    uint32_t i;

    for (i = 0u; i < count; i++)
    {
        Sched_TickIsr();
    }
}

static void TickAndDispatch(uint32_t count)
{
    uint32_t i;

    for (i = 0u; i < count; i++)
    {
        Sched_TickIsr();
        TEST_ASSERT_TRUE(Sched_Dispatch());
    }
}

void setUp(void)
{
    s_logCount = 0u;
    s_begins = 0u;
    s_ends = 0u;
    s_hangReactions = 0u;
    s_ticksInsideTask = 0u;
    s_tickingTask = SCHED_TASK_NONE;
    Sched_Init();
}

void tearDown(void)
{
}

void test_Sched_Dispatch_NoNewTick_ReturnsFalse(void)
{
    TEST_ASSERT_FALSE(Sched_Dispatch());
    TEST_ASSERT_EQUAL_UINT32(0u, s_logCount);
}

/* @verifies SWR-DCU-112 */
void test_Sched_Dispatch_FirstTicks_RunTasksAtTheirOffsetsInTableOrder(void)
{
    TickAndDispatch(1u);
    TEST_ASSERT_EQUAL_UINT32(2u, s_logCount);
    TEST_ASSERT_EQUAL_UINT8(SCHED_TASK_T1, s_log[0]);
    TEST_ASSERT_EQUAL_UINT8(SCHED_TASK_T5, s_log[1]);

    TickAndDispatch(1u);
    TEST_ASSERT_EQUAL_UINT32(4u, s_logCount);
    TEST_ASSERT_EQUAL_UINT8(SCHED_TASK_T1, s_log[2]);
    TEST_ASSERT_EQUAL_UINT8(SCHED_TASK_T10, s_log[3]);
}

/* @verifies SWR-DCU-112 */
void test_Sched_Dispatch_OneSecond_RunsEveryTaskAtItsRate(void)
{
    Sched_StatsType stats;

    TickAndDispatch(1000u);
    Sched_GetStats(&stats);
    TEST_ASSERT_EQUAL_UINT32(1000u, stats.taskRuns[SCHED_TASK_T1]);
    TEST_ASSERT_EQUAL_UINT32(200u, stats.taskRuns[SCHED_TASK_T5]);
    TEST_ASSERT_EQUAL_UINT32(100u, stats.taskRuns[SCHED_TASK_T10]);
    TEST_ASSERT_EQUAL_UINT32(10u, stats.taskRuns[SCHED_TASK_T100]);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.taskRuns[SCHED_TASK_T1000]);
    TEST_ASSERT_EQUAL_UINT32(0u, stats.overruns);
    TEST_ASSERT_EQUAL_UINT32(s_begins, s_ends);
}

/* @verifies SWR-DCU-112 */
void test_Sched_Dispatch_MissedTicks_CountedNotReplayed(void)
{
    Sched_StatsType stats;

    Tick(3u);
    TEST_ASSERT_TRUE(Sched_Dispatch());
    TEST_ASSERT_FALSE(Sched_Dispatch());
    Sched_GetStats(&stats);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.overruns);
    TEST_ASSERT_EQUAL_UINT32(2u, stats.missedTicks);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.taskRuns[SCHED_TASK_T1]);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.taskMissed[SCHED_TASK_T1]);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.taskRuns[SCHED_TASK_T5]);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.taskRuns[SCHED_TASK_T10]);
}

void test_Sched_Dispatch_AfterMissedRelease_KeepsTaskPhase(void)
{
    Sched_StatsType stats;

    TickAndDispatch(2u);
    Tick(22u);
    TEST_ASSERT_TRUE(Sched_Dispatch());
    Sched_GetStats(&stats);
    /* T10 ran at tick 2 and late at tick 24; the release at tick 22 was skipped. */
    TEST_ASSERT_EQUAL_UINT32(2u, stats.taskRuns[SCHED_TASK_T10]);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.taskMissed[SCHED_TASK_T10]);
    TickAndDispatch(7u);
    Sched_GetStats(&stats);
    TEST_ASSERT_EQUAL_UINT32(2u, stats.taskRuns[SCHED_TASK_T10]);
    /* Next release realigned to the task phase: tick 32 (offset 2). */
    TickAndDispatch(1u);
    Sched_GetStats(&stats);
    TEST_ASSERT_EQUAL_UINT32(3u, stats.taskRuns[SCHED_TASK_T10]);
}

/* @verifies SWR-DCU-077 */
void test_Sched_TickIsr_TaskRunsLongerThanHangLimit_TripsOnce(void)
{
    Sched_StatsType stats;

    s_tickingTask = SCHED_TASK_T5;
    s_ticksInsideTask = LS_T_HANG_DETECT_MS + 5u;
    Tick(1u);
    TEST_ASSERT_TRUE(Sched_Dispatch());
    Sched_GetStats(&stats);
    TEST_ASSERT_EQUAL_UINT32(1u, s_hangReactions);
    TEST_ASSERT_EQUAL_UINT32(1u, stats.hangTrips);
}

/* @verifies SWR-DCU-077 */
void test_Sched_TickIsr_TaskWithinHangLimit_DoesNotTrip(void)
{
    s_tickingTask = SCHED_TASK_T5;
    s_ticksInsideTask = LS_T_HANG_DETECT_MS;
    Tick(1u);
    TEST_ASSERT_TRUE(Sched_Dispatch());
    TEST_ASSERT_EQUAL_UINT32(0u, s_hangReactions);
}

void test_Sched_TickIsr_NoTaskRunning_DoesNotTrip(void)
{
    Tick(LS_T_HANG_DETECT_MS + 10u);
    TEST_ASSERT_EQUAL_UINT32(0u, s_hangReactions);
}

void test_Sched_GetStats_NullDestination_IsIgnored(void)
{
    Sched_GetStats(NULL);
    TEST_PASS();
}
