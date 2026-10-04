/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "unity.h"

#include <stddef.h>

#include "ls_common/ls_crc8.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* Bitwise reference: polynomial 0x1D, MSB first. */
static uint8_t ReferenceUpdate(uint8_t crc, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0u; i < len; i++)
    {
        crc = (uint8_t)(crc ^ data[i]);
        for (uint8_t bit = 0u; bit < 8u; bit++)
        {
            if ((crc & 0x80u) != 0u)
            {
                crc = (uint8_t)((uint8_t)(crc << 1u) ^ 0x1Du);
            }
            else
            {
                crc = (uint8_t)(crc << 1u);
            }
        }
    }
    return crc;
}

/* @verifies SYS-037 */
void test_CheckValueOf123456789Is4B(void)
{
    const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

    TEST_ASSERT_EQUAL_HEX8(0x4Bu, LsCrc8_J1850(check, (uint32_t)sizeof(check)));
}

void test_EmptyInputGivesInitXorOut(void)
{
    const uint8_t unused = 0u;

    TEST_ASSERT_EQUAL_HEX8(0x00u, LsCrc8_J1850(&unused, 0u));
    TEST_ASSERT_EQUAL_HEX8(0x00u, LsCrc8_J1850(NULL, 0u));
}

void test_NullDataLeavesIntermediateValueUnchanged(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x5Au, LsCrc8_J1850Update(0x5Au, NULL, 3u));
}

void test_TableMatchesBitwiseReferenceForEveryByteAndStart(void)
{
    for (uint32_t start = 0u; start < 256u; start++)
    {
        for (uint32_t value = 0u; value < 256u; value++)
        {
            const uint8_t byte = (uint8_t)value;

            TEST_ASSERT_EQUAL_HEX8(ReferenceUpdate((uint8_t)start, &byte, 1u),
                                   LsCrc8_J1850Update((uint8_t)start, &byte, 1u));
        }
    }
}

void test_ChainedUpdatesEqualOneComputation(void)
{
    const uint8_t data[] = {0x01u, 0x10u, 0x13u, 0x07u, 0x05u};
    uint8_t crc = LsCrc8_J1850Update(LS_CRC8_J1850_INIT, data, 2u);

    crc = LsCrc8_J1850Update(crc, &data[2], 3u);
    TEST_ASSERT_EQUAL_HEX8(LsCrc8_J1850(data, 5u), (uint8_t)(crc ^ LS_CRC8_J1850_XOROUT));
    /* CGW_WinCmd UP, counter 3 (LS-SAIC-001 7.10): DataID 0x1001, bytes 13 07 05 -> CRC 0x5B. */
    TEST_ASSERT_EQUAL_HEX8(0x5Bu, LsCrc8_J1850(data, 5u));
}
