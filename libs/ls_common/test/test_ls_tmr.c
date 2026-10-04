/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "unity.h"

#include <stddef.h>

#include "ls_common/ls_evset.h"
#include "ls_common/ls_tmr.h"

#define EV_BRAKE   (2u)
#define EV_TIMEOUT (5u)
#define EV_LAST    (31u)
#define NUM_SLOTS  (3u)

static const LsEvSet_EventType s_eventIds[NUM_SLOTS] = {EV_BRAKE, EV_TIMEOUT, EV_LAST};
static LsTmr_SlotType s_slots[NUM_SLOTS];
static LsTmr_PoolType s_pool;
static LsEvSet_Type s_events;

void setUp(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Init(&s_pool, s_slots, s_eventIds, NUM_SLOTS));
    TEST_ASSERT_EQUAL_UINT8(LS_EVSET_OK, LsEvSet_Init(&s_events, 8u));
}

void tearDown(void)
{
}

void test_InitStopsEveryTimer(void)
{
    for (uint8_t i = 0u; i < NUM_SLOTS; i++)
    {
        TEST_ASSERT_FALSE(LsTmr_IsActive(&s_pool, s_eventIds[i]));
    }
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(&s_pool, 1000u, &s_events));
    TEST_ASSERT_TRUE(LsEvSet_IsEmpty(&s_events));
}

void test_InitRejectsInvalidArguments(void)
{
    LsTmr_SlotType slots[LS_EVSET_MAX_EVENTS + 1u];
    LsEvSet_EventType ids[LS_EVSET_MAX_EVENTS + 1u];

    for (uint8_t i = 0u; i < (LS_EVSET_MAX_EVENTS + 1u); i++)
    {
        ids[i] = i;
    }
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Init(NULL, s_slots, s_eventIds, NUM_SLOTS));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Init(&s_pool, NULL, s_eventIds, NUM_SLOTS));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Init(&s_pool, s_slots, NULL, NUM_SLOTS));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Init(&s_pool, s_slots, s_eventIds, 0u));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM,
                            LsTmr_Init(&s_pool, slots, ids, (uint8_t)(LS_EVSET_MAX_EVENTS + 1u)));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Init(&s_pool, slots, ids, LS_EVSET_MAX_EVENTS));
}

void test_InitRejectsInvalidOrRepeatedEvents(void)
{
    const LsEvSet_EventType outOfRange[2] = {1u, LS_EVSET_MAX_EVENTS};
    const LsEvSet_EventType repeated[3] = {4u, 7u, 4u};
    LsTmr_SlotType slots[3];

    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Init(&s_pool, slots, outOfRange, 2u));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Init(&s_pool, slots, repeated, 3u));
}

void test_FailedInitLeavesAnUnusablePool(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Init(&s_pool, NULL, s_eventIds, NUM_SLOTS));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Start(&s_pool, EV_BRAKE, 0u, 10u));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Stop(&s_pool, EV_BRAKE));
    TEST_ASSERT_FALSE(LsTmr_IsActive(&s_pool, EV_BRAKE));
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(&s_pool, 100u, &s_events));
}

void test_NullArgumentsAreRejected(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Start(NULL, EV_BRAKE, 0u, 10u));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Stop(NULL, EV_BRAKE));
    TEST_ASSERT_FALSE(LsTmr_IsActive(NULL, EV_BRAKE));
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(NULL, 0u, &s_events));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_BRAKE, 0u, 0u));
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(&s_pool, 0u, NULL));
    TEST_ASSERT_TRUE(LsTmr_IsActive(&s_pool, EV_BRAKE));
}

void test_UnknownEventIsRejected(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Start(&s_pool, 3u, 0u, 10u));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM, LsTmr_Stop(&s_pool, 3u));
    TEST_ASSERT_FALSE(LsTmr_IsActive(&s_pool, 3u));
}

void test_DurationAboveMaximumIsRejected(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_E_PARAM,
                            LsTmr_Start(&s_pool, EV_BRAKE, 0u, LS_TMR_MAX_DURATION_MS + 1u));
    TEST_ASSERT_FALSE(LsTmr_IsActive(&s_pool, EV_BRAKE));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_BRAKE, 0u, LS_TMR_MAX_DURATION_MS));
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(&s_pool, LS_TMR_MAX_DURATION_MS - 1u, &s_events));
    TEST_ASSERT_EQUAL_UINT8(1u, LsTmr_Poll(&s_pool, LS_TMR_MAX_DURATION_MS, &s_events));
}

void test_TimerExpiresWhenElapsedReachesDuration(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_TIMEOUT, 100u, 50u));
    TEST_ASSERT_TRUE(LsTmr_IsActive(&s_pool, EV_TIMEOUT));
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(&s_pool, 149u, &s_events));
    TEST_ASSERT_FALSE(LsEvSet_IsPending(&s_events, EV_TIMEOUT));
    TEST_ASSERT_EQUAL_UINT8(1u, LsTmr_Poll(&s_pool, 150u, &s_events));
    TEST_ASSERT_TRUE(LsEvSet_IsPending(&s_events, EV_TIMEOUT));
    TEST_ASSERT_FALSE(LsTmr_IsActive(&s_pool, EV_TIMEOUT));
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(&s_pool, 1000u, &s_events));
}

void test_LatePollStillExpiresOnce(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_BRAKE, 0u, 10u));
    TEST_ASSERT_EQUAL_UINT8(1u, LsTmr_Poll(&s_pool, 5000u, &s_events));
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(&s_pool, 5001u, &s_events));
}

void test_ZeroDurationExpiresAtTheNextPoll(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_LAST, 77u, 0u));
    TEST_ASSERT_EQUAL_UINT8(1u, LsTmr_Poll(&s_pool, 77u, &s_events));
    TEST_ASSERT_TRUE(LsEvSet_IsPending(&s_events, EV_LAST));
}

void test_RestartUsesTheNewStartAndDuration(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_BRAKE, 0u, 10u));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_BRAKE, 8u, 10u));
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(&s_pool, 17u, &s_events));
    TEST_ASSERT_EQUAL_UINT8(1u, LsTmr_Poll(&s_pool, 18u, &s_events));
}

void test_StoppedTimerNeverExpires(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_BRAKE, 0u, 10u));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Stop(&s_pool, EV_BRAKE));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Stop(&s_pool, EV_BRAKE));
    TEST_ASSERT_FALSE(LsTmr_IsActive(&s_pool, EV_BRAKE));
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(&s_pool, 100u, &s_events));
    TEST_ASSERT_TRUE(LsEvSet_IsEmpty(&s_events));
}

void test_SeveralTimersExpireInOnePoll(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_BRAKE, 0u, 10u));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_TIMEOUT, 0u, 20u));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_LAST, 0u, 30u));
    TEST_ASSERT_EQUAL_UINT8(2u, LsTmr_Poll(&s_pool, 20u, &s_events));
    TEST_ASSERT_TRUE(LsEvSet_IsPending(&s_events, EV_BRAKE));
    TEST_ASSERT_TRUE(LsEvSet_IsPending(&s_events, EV_TIMEOUT));
    TEST_ASSERT_TRUE(LsTmr_IsActive(&s_pool, EV_LAST));
}

void test_TimestampWrapIsHandled(void)
{
    TEST_ASSERT_EQUAL_UINT32(20u, LsTmr_Elapsed(10u, 0xFFFFFFF6u));
    TEST_ASSERT_EQUAL_UINT8(LS_TMR_OK, LsTmr_Start(&s_pool, EV_BRAKE, 0xFFFFFFF0u, 32u));
    TEST_ASSERT_EQUAL_UINT8(0u, LsTmr_Poll(&s_pool, 0xFu, &s_events));
    TEST_ASSERT_EQUAL_UINT8(1u, LsTmr_Poll(&s_pool, 0x10u, &s_events));
}
