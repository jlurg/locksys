/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "unity.h"

#include <stddef.h>

#include "ls_common/ls_evset.h"

static LsEvSet_Type s_set;

void setUp(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Init(&s_set, 10u));
}

void tearDown(void)
{
}

void test_InitRejectsNullSetAndZeroBudget(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_E_PARAM, LsEvSet_Init(NULL, 1u));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_E_PARAM, LsEvSet_Init(&s_set, 0u));
}

void test_InitLeavesTheSetEmpty(void)
{
    LsEvSet_EventType event = 0xFFu;

    TEST_ASSERT_TRUE(LsEvSet_IsEmpty(&s_set));
    LsEvSet_BeginDrain(&s_set);
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_EMPTY, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, event);
}

void test_PostAndCancelRejectInvalidArguments(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_E_PARAM, LsEvSet_Post(NULL, 0u));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_E_PARAM, LsEvSet_Post(&s_set, LS_EVSET_MAX_EVENTS));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_E_PARAM, LsEvSet_Cancel(NULL, 0u));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_E_PARAM, LsEvSet_Cancel(&s_set, LS_EVSET_MAX_EVENTS));
    TEST_ASSERT_TRUE(LsEvSet_IsEmpty(&s_set));
}

void test_IsPendingIsFalseForInvalidArguments(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 3u));
    TEST_ASSERT_FALSE(LsEvSet_IsPending(NULL, 3u));
    TEST_ASSERT_FALSE(LsEvSet_IsPending(&s_set, LS_EVSET_MAX_EVENTS));
    TEST_ASSERT_TRUE(LsEvSet_IsPending(&s_set, 3u));
    TEST_ASSERT_FALSE(LsEvSet_IsPending(&s_set, 4u));
}

void test_PostingTwiceKeepsOnePendingEvent(void)
{
    LsEvSet_EventType event = 0u;

    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 7u));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 7u));
    LsEvSet_BeginDrain(&s_set);
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_EQUAL_UINT8(7u, event);
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_EMPTY, LsEvSet_Next(&s_set, &event));
}

void test_CancelWithdrawsOnlyTheGivenEvent(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 1u));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 2u));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Cancel(&s_set, 1u));
    TEST_ASSERT_FALSE(LsEvSet_IsPending(&s_set, 1u));
    TEST_ASSERT_TRUE(LsEvSet_IsPending(&s_set, 2u));
}

void test_DrainDeliversLowestEventNumberFirst(void)
{
    LsEvSet_EventType event = 0u;

    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 31u));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 5u));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 0u));
    LsEvSet_BeginDrain(&s_set);
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_EQUAL_UINT8(0u, event);
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_EQUAL_UINT8(5u, event);
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_EQUAL_UINT8(31u, event);
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_EMPTY, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_TRUE(LsEvSet_IsEmpty(&s_set));
}

void test_EveryEventNumberIsDeliveredAlone(void)
{
    for (LsEvSet_EventType expected = 0u; expected < LS_EVSET_MAX_EVENTS; expected++)
    {
        LsEvSet_EventType event = 0xFFu;

        TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, expected));
        LsEvSet_BeginDrain(&s_set);
        TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Next(&s_set, &event));
        TEST_ASSERT_EQUAL_UINT8(expected, event);
        TEST_ASSERT_TRUE(LsEvSet_IsEmpty(&s_set));
    }
}

void test_DrainStopsAtTheBudgetAndKeepsEventsPending(void)
{
    LsEvSet_EventType event = 0u;

    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Init(&s_set, 2u));
    for (LsEvSet_EventType i = 0u; i < 3u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, i));
    }
    LsEvSet_BeginDrain(&s_set);
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_E_LIMIT, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_EQUAL_UINT8(1u, event);
    TEST_ASSERT_TRUE(LsEvSet_IsPending(&s_set, 2u));
    LsEvSet_BeginDrain(&s_set);
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_EQUAL_UINT8(2u, event);
}

void test_EventPostedDuringDrainCountsAgainstTheBudget(void)
{
    LsEvSet_EventType event = 0u;

    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Init(&s_set, 1u));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 4u));
    LsEvSet_BeginDrain(&s_set);
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Next(&s_set, &event));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_E_LIMIT, LsEvSet_Next(&s_set, &event));
}

void test_ClearAndNullArgumentsAreHandled(void)
{
    LsEvSet_EventType event = 0u;

    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Post(&s_set, 9u));
    TEST_ASSERT_FALSE(LsEvSet_IsEmpty(&s_set));
    LsEvSet_Clear(&s_set);
    TEST_ASSERT_TRUE(LsEvSet_IsEmpty(&s_set));
    LsEvSet_Clear(NULL);
    LsEvSet_BeginDrain(NULL);
    TEST_ASSERT_TRUE(LsEvSet_IsEmpty(NULL));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_E_PARAM, LsEvSet_Next(NULL, &event));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_E_PARAM, LsEvSet_Next(&s_set, NULL));
}
