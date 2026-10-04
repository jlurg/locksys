/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "unity.h"

#include <stddef.h>

#include "ls_common/ls_ring.h"

#define CAPACITY (8u)

static uint8_t s_storage[CAPACITY];
static LsRing_Type s_ring;

void setUp(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Init(&s_ring, s_storage, CAPACITY));
}

void tearDown(void)
{
}

void test_InitGivesAnEmptyRing(void)
{
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Used(&s_ring));
    TEST_ASSERT_EQUAL_UINT32(CAPACITY, LsRing_Free(&s_ring));
}

void test_InitRejectsInvalidArguments(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_PARAM, LsRing_Init(NULL, s_storage, CAPACITY));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_PARAM, LsRing_Init(&s_ring, NULL, CAPACITY));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_PARAM, LsRing_Init(&s_ring, s_storage, 0u));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_PARAM, LsRing_Init(&s_ring, s_storage, 1u));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_PARAM, LsRing_Init(&s_ring, s_storage, 6u));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_PARAM, LsRing_Init(&s_ring, s_storage, 0x80000001u));
}

void test_InitAcceptsTheCapacityLimits(void)
{
    /* Init only records the storage; the largest capacity is not accessed here. */
    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Init(&s_ring, s_storage, 0x80000000u));
    TEST_ASSERT_EQUAL_UINT32(0x80000000u, LsRing_Free(&s_ring));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Init(&s_ring, s_storage, 2u));
    TEST_ASSERT_EQUAL_UINT32(2u, LsRing_Free(&s_ring));
}

void test_UninitialisedRingIsUnusable(void)
{
    const uint8_t data[1] = {0x11u};
    uint8_t dst[1] = {0u};
    const uint8_t *block = s_storage;

    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_PARAM, LsRing_Init(&s_ring, NULL, CAPACITY));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Used(&s_ring));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Free(&s_ring));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_PARAM, LsRing_Push(&s_ring, data, 1u));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Pop(&s_ring, dst, 1u));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_PeekContiguous(&s_ring, &block));
    TEST_ASSERT_NULL(block);
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Consume(&s_ring, 1u));
}

void test_NullArgumentsAreRejected(void)
{
    uint8_t dst[1] = {0u};
    const uint8_t data[1] = {0x22u};

    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Used(NULL));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Free(NULL));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_PARAM, LsRing_Push(NULL, data, 1u));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_PARAM, LsRing_Push(&s_ring, NULL, 1u));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Push(&s_ring, data, 1u));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Pop(NULL, dst, 1u));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Pop(&s_ring, NULL, 1u));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_PeekContiguous(&s_ring, NULL));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Consume(NULL, 1u));
    TEST_ASSERT_EQUAL_UINT32(1u, LsRing_Used(&s_ring));
}

void test_PushAndPopPreserveOrder(void)
{
    const uint8_t data[5] = {1u, 2u, 3u, 4u, 5u};
    uint8_t dst[8] = {0u};

    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Push(&s_ring, data, 5u));
    TEST_ASSERT_EQUAL_UINT32(5u, LsRing_Used(&s_ring));
    TEST_ASSERT_EQUAL_UINT32(3u, LsRing_Free(&s_ring));
    TEST_ASSERT_EQUAL_UINT32(2u, LsRing_Pop(&s_ring, dst, 2u));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, dst, 2u);
    TEST_ASSERT_EQUAL_UINT32(3u, LsRing_Pop(&s_ring, dst, 8u));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(&data[2], dst, 3u);
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Pop(&s_ring, dst, 8u));
}

void test_PushIsAllOrNothing(void)
{
    const uint8_t data[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    uint8_t dst[8] = {0u};

    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Push(&s_ring, data, 6u));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_FULL, LsRing_Push(&s_ring, data, 3u));
    TEST_ASSERT_EQUAL_UINT32(6u, LsRing_Used(&s_ring));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Push(&s_ring, &data[6], 2u));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Free(&s_ring));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_E_FULL, LsRing_Push(&s_ring, data, 1u));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Push(&s_ring, data, 0u));
    TEST_ASSERT_EQUAL_UINT32(8u, LsRing_Pop(&s_ring, dst, 8u));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, dst, 8u);
}

void test_DataWrapsAroundTheEndOfTheStorage(void)
{
    const uint8_t first[6] = {1u, 2u, 3u, 4u, 5u, 6u};
    const uint8_t second[5] = {7u, 8u, 9u, 10u, 11u};
    uint8_t dst[8] = {0u};

    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Push(&s_ring, first, 6u));
    TEST_ASSERT_EQUAL_UINT32(6u, LsRing_Pop(&s_ring, dst, 6u));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Push(&s_ring, second, 5u));
    TEST_ASSERT_EQUAL_UINT32(5u, LsRing_Pop(&s_ring, dst, 8u));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(second, dst, 5u);
}

void test_PeekAndConsumeReturnContiguousBlocks(void)
{
    const uint8_t first[6] = {1u, 2u, 3u, 4u, 5u, 6u};
    const uint8_t second[5] = {7u, 8u, 9u, 10u, 11u};
    uint8_t dst[6] = {0u};
    const uint8_t *block = NULL;

    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_PeekContiguous(&s_ring, &block));
    TEST_ASSERT_NULL(block);

    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Push(&s_ring, first, 6u));
    TEST_ASSERT_EQUAL_UINT32(6u, LsRing_Pop(&s_ring, dst, 6u));
    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Push(&s_ring, second, 5u));

    /* Offset 6: two bytes up to the end of the storage, three after the wrap. */
    TEST_ASSERT_EQUAL_UINT32(2u, LsRing_PeekContiguous(&s_ring, &block));
    TEST_ASSERT_EQUAL_PTR(&s_storage[6], block);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(second, block, 2u);
    TEST_ASSERT_EQUAL_UINT32(2u, LsRing_Consume(&s_ring, 2u));

    TEST_ASSERT_EQUAL_UINT32(3u, LsRing_PeekContiguous(&s_ring, &block));
    TEST_ASSERT_EQUAL_PTR(&s_storage[0], block);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(&second[2], block, 3u);
    TEST_ASSERT_EQUAL_UINT32(3u, LsRing_Consume(&s_ring, 10u));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Used(&s_ring));
    TEST_ASSERT_EQUAL_UINT32(0u, LsRing_Consume(&s_ring, 1u));
}

void test_IndicesWrapAround32Bits(void)
{
    const uint8_t data[3] = {0xA1u, 0xA2u, 0xA3u};
    uint8_t dst[3] = {0u};

    s_ring.head = 0xFFFFFFFEu;
    s_ring.tail = 0xFFFFFFFEu;
    TEST_ASSERT_EQUAL_UINT8(LS_RING_OK, LsRing_Push(&s_ring, data, 3u));
    TEST_ASSERT_EQUAL_UINT32(3u, LsRing_Used(&s_ring));
    TEST_ASSERT_EQUAL_UINT32(5u, LsRing_Free(&s_ring));
    TEST_ASSERT_EQUAL_UINT32(3u, LsRing_Pop(&s_ring, dst, 3u));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, dst, 3u);
}
