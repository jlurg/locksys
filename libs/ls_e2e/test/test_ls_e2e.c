/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "unity.h"

#include <stddef.h>
#include <string.h>

#include "ls_common/ls_crc8.h"
#include "ls_e2e/ls_e2e.h"
#include "ls_e2e_vectors_gen.h"

/* CGW_WinCmd: DataID 0x1001, DLC 4, cyclic, MaxDelta 2. */
static const LsE2e_ConfigType s_winCmd = {
    0x1001u, 4u, LS_E2E_MODE_CYCLIC, 2u, LS_E2E_VEC_N_OK_VALID, LS_E2E_VEC_N_ERR_INVALID,
};

/* CGW_DoorCmd: DataID 0x1002, DLC 4, event mode. */
static const LsE2e_ConfigType s_doorCmd = {
    0x1002u, 4u, LS_E2E_MODE_EVENT, 0u, LS_E2E_VEC_N_OK_VALID, LS_E2E_VEC_N_ERR_INVALID,
};

static LsE2e_RxStateType s_rx;
static LsE2e_TxStateType s_tx;

void setUp(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_OK, LsE2e_RxInit(&s_rx));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_OK, LsE2e_TxInit(&s_tx));
}

void tearDown(void)
{
}

/* Protect the next WinCmd frame (payload byte 2 = PressId 7) and check it at the receiver. */
static LsE2e_CheckStatusType SendNext(void)
{
    uint8_t frame[4] = {0x00u, 0x00u, 0x07u, 0x00u};

    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_OK, LsE2e_Protect(&s_winCmd, &s_tx, frame, 4u));
    return LsE2e_Check(&s_winCmd, &s_rx, frame, 4u);
}

/* @verifies SYS-037 */
void test_CrcCheckValueMatchesTheContract(void)
{
    const char *check = LS_E2E_VEC_CRC_CHECK_INPUT;

    TEST_ASSERT_EQUAL_HEX8(LS_E2E_VEC_CRC_CHECK_VALUE,
                           LsCrc8_J1850((const uint8_t *)check, (uint32_t)strlen(check)));
}

/* @verifies SYS-037 */
void test_ProtectReproducesEveryContractVector(void)
{
    for (uint8_t i = 0u; i < LS_E2E_VEC_NUM_FRAMES; i++)
    {
        const LsE2eVec_FrameType *vec = &LsE2eVec_Frames[i];
        const LsE2e_ConfigType cfg = {
            vec->dataId, vec->dlc, LS_E2E_MODE_CYCLIC, 3u, 1u, 1u,
        };
        uint8_t frame[LS_E2E_MAX_DLC];

        (void)memcpy(frame, vec->payload, sizeof(frame));
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(
            LS_E2E_E_OK, LsE2e_ProtectWithCounter(&cfg, vec->counter, frame, vec->dlc), vec->name);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(vec->expected, frame, vec->dlc, vec->name);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(vec->expected[0],
                                       LsE2e_Crc(vec->dataId, vec->expected, vec->dlc), vec->name);
    }
}

/* @verifies SYS-037 */
void test_ReceiverSequencesMatchTheVectors(void)
{
    for (uint8_t s = 0u; s < LS_E2E_VEC_NUM_SEQUENCES; s++)
    {
        const LsE2eVec_SequenceType *seq = &LsE2eVec_Sequences[s];

        TEST_ASSERT_TRUE_MESSAGE(LsE2e_IsConfigValid(&seq->config), seq->name);
        TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_OK, LsE2e_RxInit(&s_rx));
        for (uint8_t k = 0u; k < seq->numSteps; k++)
        {
            const LsE2eVec_StepType *step = &seq->steps[k];

            if (step->isTimeout)
            {
                LsE2e_RxTimeout(&s_rx);
            }
            else
            {
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(
                    step->status, LsE2e_Check(&seq->config, &s_rx, step->bytes, step->len),
                    seq->name);
            }
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(step->state, s_rx.state, seq->name);
            TEST_ASSERT_EQUAL_MESSAGE(step->delivered, LsE2e_IsDataValid(&s_rx), seq->name);
        }
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(seq->crcErrors, s_rx.crcErrors, seq->name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(seq->seqErrors, s_rx.seqErrors, seq->name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(seq->repeated, s_rx.repeated, seq->name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(seq->timeouts, s_rx.timeouts, seq->name);
    }
}

void test_ConfigValidation(void)
{
    LsE2e_ConfigType cfg = s_winCmd;

    TEST_ASSERT_FALSE(LsE2e_IsConfigValid(NULL));
    TEST_ASSERT_TRUE(LsE2e_IsConfigValid(&cfg));
    cfg.dlc = LS_E2E_MIN_DLC - 1u;
    TEST_ASSERT_FALSE(LsE2e_IsConfigValid(&cfg));
    cfg.dlc = LS_E2E_MAX_DLC + 1u;
    TEST_ASSERT_FALSE(LsE2e_IsConfigValid(&cfg));
    cfg.dlc = LS_E2E_MIN_DLC;
    TEST_ASSERT_TRUE(LsE2e_IsConfigValid(&cfg));
    cfg.dlc = LS_E2E_MAX_DLC;
    TEST_ASSERT_TRUE(LsE2e_IsConfigValid(&cfg));
    cfg.nOkValid = 0u;
    TEST_ASSERT_FALSE(LsE2e_IsConfigValid(&cfg));
    cfg.nOkValid = 1u;
    cfg.nErrInvalid = 0u;
    TEST_ASSERT_FALSE(LsE2e_IsConfigValid(&cfg));
    cfg.nErrInvalid = 1u;
    cfg.maxDelta = 0u;
    TEST_ASSERT_FALSE(LsE2e_IsConfigValid(&cfg));
    cfg.maxDelta = LS_E2E_MAX_DELTA_MAX + 1u;
    TEST_ASSERT_FALSE(LsE2e_IsConfigValid(&cfg));
    cfg.maxDelta = LS_E2E_MAX_DELTA_MAX;
    TEST_ASSERT_TRUE(LsE2e_IsConfigValid(&cfg));
    cfg.mode = LS_E2E_MODE_EVENT;
    cfg.maxDelta = 0u;
    TEST_ASSERT_TRUE(LsE2e_IsConfigValid(&cfg));
    cfg.mode = LS_E2E_MODE_NONE;
    TEST_ASSERT_FALSE(LsE2e_IsConfigValid(&cfg));
    cfg.mode = 3u;
    TEST_ASSERT_FALSE(LsE2e_IsConfigValid(&cfg));
}

void test_CrcCoversTheDataIdWhenThereIsNoPayload(void)
{
    const uint8_t id[2] = {0x01u, 0x10u};
    const uint8_t frame[2] = {0x00u, 0x05u};
    const uint8_t expected = LsCrc8_J1850(id, 2u);

    TEST_ASSERT_EQUAL_HEX8(expected, LsE2e_Crc(0x1001u, NULL, 4u));
    TEST_ASSERT_EQUAL_HEX8(expected, LsE2e_Crc(0x1001u, frame, 0u));
    TEST_ASSERT_EQUAL_HEX8(expected, LsE2e_Crc(0x1001u, frame, 1u));
    TEST_ASSERT_NOT_EQUAL(expected, LsE2e_Crc(0x1001u, frame, 2u));
}

void test_InitFunctionsRejectNull(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_BAD_ARG, LsE2e_TxInit(NULL));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_BAD_ARG, LsE2e_RxInit(NULL));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATE_INIT, s_rx.state);
    TEST_ASSERT_FALSE(s_rx.refValid);
    TEST_ASSERT_EQUAL_UINT8(0u, s_tx.counter);
}

void test_ProtectRejectsInvalidArgumentsWithoutChanges(void)
{
    LsE2e_ConfigType bad = s_winCmd;
    uint8_t frame[4] = {0x5Au, 0xA0u, 0x07u, 0x00u};
    const uint8_t original[4] = {0x5Au, 0xA0u, 0x07u, 0x00u};

    bad.nOkValid = 0u;
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_BAD_ARG, LsE2e_ProtectWithCounter(NULL, 1u, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_BAD_ARG, LsE2e_ProtectWithCounter(&bad, 1u, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_BAD_ARG, LsE2e_ProtectWithCounter(&s_winCmd, 1u, NULL, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_BAD_ARG, LsE2e_ProtectWithCounter(&s_winCmd, 1u, frame, 3u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_BAD_ARG, LsE2e_Protect(&s_winCmd, NULL, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_BAD_ARG, LsE2e_Protect(&s_winCmd, &s_tx, frame, 5u));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(original, frame, 4u);
    TEST_ASSERT_EQUAL_UINT8(0u, s_tx.counter);
}

void test_ProtectKeepsSignalBitsOfByte1AndMasksTheCounter(void)
{
    uint8_t frame[4] = {0x00u, 0xAFu, 0x07u, 0x00u};

    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_OK, LsE2e_ProtectWithCounter(&s_winCmd, 0x13u, frame, 4u));
    TEST_ASSERT_EQUAL_HEX8(0xA3u, frame[1]);
    TEST_ASSERT_EQUAL_HEX8(LsE2e_Crc(0x1001u, frame, 4u), frame[0]);
}

void test_SenderCounterWrapsAfter15(void)
{
    for (uint8_t i = 0u; i < 16u; i++)
    {
        uint8_t frame[4] = {0u};

        TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_OK, LsE2e_Protect(&s_winCmd, &s_tx, frame, 4u));
        TEST_ASSERT_EQUAL_UINT8(i, frame[1] & LS_E2E_COUNTER_MASK);
    }
    TEST_ASSERT_EQUAL_UINT8(0u, s_tx.counter);
}

/* @verifies SYS-037 */
void test_CheckRejectsInvalidArgumentsWithoutChanges(void)
{
    LsE2e_ConfigType bad = s_winCmd;
    const uint8_t frame[4] = {0xA6u, 0x00u, 0x07u, 0x00u};

    bad.dlc = 1u;
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_BAD_ARG, LsE2e_Check(&s_winCmd, NULL, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_BAD_ARG, LsE2e_Check(NULL, &s_rx, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_BAD_ARG, LsE2e_Check(&bad, &s_rx, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_BAD_ARG, LsE2e_Check(&s_winCmd, &s_rx, NULL, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATE_INIT, s_rx.state);
    TEST_ASSERT_EQUAL_UINT8(0u, s_rx.errCount);
    TEST_ASSERT_EQUAL_UINT16(0u, s_rx.crcErrors);
}

/* @verifies SYS-037 */
void test_FrameUnderAnotherIdentifierFailsTheCrc(void)
{
    uint8_t frame[4] = {0x00u, 0x10u, 0x2Au, 0x00u};

    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_OK, LsE2e_ProtectWithCounter(&s_doorCmd, 0u, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_CRC_ERROR, LsE2e_Check(&s_winCmd, &s_rx, frame, 4u));
    TEST_ASSERT_FALSE(s_rx.refValid);
}

/* @verifies SYS-037 */
void test_ErrorsBeforeTheFirstFrameGiveInvalid(void)
{
    const uint8_t frame[4] = {0x00u, 0x00u, 0x07u, 0x00u};

    for (uint8_t i = 0u; i < LS_E2E_VEC_N_ERR_INVALID; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_CRC_ERROR, LsE2e_Check(&s_winCmd, &s_rx, frame, 4u));
    }
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATE_INVALID, s_rx.state);
    TEST_ASSERT_FALSE(LsE2e_IsDataValid(&s_rx));
}

void test_OkAndErrorCountsSaturate(void)
{
    const uint8_t bad[4] = {0x00u, 0x00u, 0x07u, 0x00u};

    for (uint16_t i = 0u; i < 300u; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_OK, SendNext());
    }
    TEST_ASSERT_EQUAL_UINT8(0xFFu, s_rx.okCount);
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATE_VALID, s_rx.state);
    for (uint16_t i = 0u; i < 300u; i++)
    {
        (void)LsE2e_Check(&s_winCmd, &s_rx, bad, 4u);
    }
    TEST_ASSERT_EQUAL_UINT8(0xFFu, s_rx.errCount);
    TEST_ASSERT_EQUAL_UINT16(300u, s_rx.crcErrors);
}

void test_DiagnosticCountersSaturate(void)
{
    const uint8_t bad[4] = {0x00u, 0x00u, 0x07u, 0x00u};
    uint8_t frame[4] = {0x00u, 0x00u, 0x07u, 0x00u};

    s_rx.crcErrors = 0xFFFFu;
    s_rx.seqErrors = 0xFFFFu;
    s_rx.repeated = 0xFFFFu;
    s_rx.timeouts = 0xFFFFu;
    (void)LsE2e_Check(&s_winCmd, &s_rx, bad, 4u);
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_OK, LsE2e_ProtectWithCounter(&s_winCmd, 0u, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_OK, LsE2e_Check(&s_winCmd, &s_rx, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_REPEATED, LsE2e_Check(&s_winCmd, &s_rx, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_E_OK, LsE2e_ProtectWithCounter(&s_winCmd, 8u, frame, 4u));
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_WRONG_SEQUENCE, LsE2e_Check(&s_winCmd, &s_rx, frame, 4u));
    LsE2e_RxTimeout(&s_rx);
    TEST_ASSERT_EQUAL_UINT16(0xFFFFu, s_rx.crcErrors);
    TEST_ASSERT_EQUAL_UINT16(0xFFFFu, s_rx.seqErrors);
    TEST_ASSERT_EQUAL_UINT16(0xFFFFu, s_rx.repeated);
    TEST_ASSERT_EQUAL_UINT16(0xFFFFu, s_rx.timeouts);
}

/* @verifies SYS-037 */
void test_TimeoutGivesInvalidAndClearsTheReference(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_OK, SendNext());
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATUS_OK, SendNext());
    TEST_ASSERT_TRUE(LsE2e_IsDataValid(&s_rx));
    LsE2e_RxTimeout(&s_rx);
    LsE2e_RxTimeout(NULL);
    TEST_ASSERT_EQUAL_UINT8(LS_E2E_STATE_INVALID, s_rx.state);
    TEST_ASSERT_FALSE(s_rx.refValid);
    TEST_ASSERT_EQUAL_UINT8(0u, s_rx.okCount);
    TEST_ASSERT_EQUAL_UINT16(1u, s_rx.timeouts);
    TEST_ASSERT_FALSE(LsE2e_IsDataValid(&s_rx));
}

void test_IsDataValidRequiresAnOkFrameInValidState(void)
{
    TEST_ASSERT_FALSE(LsE2e_IsDataValid(NULL));
    TEST_ASSERT_FALSE(LsE2e_IsDataValid(&s_rx));
    s_rx.state = LS_E2E_STATE_VALID;
    s_rx.lastStatus = LS_E2E_STATUS_REPEATED;
    TEST_ASSERT_FALSE(LsE2e_IsDataValid(&s_rx));
    s_rx.lastStatus = LS_E2E_STATUS_OK;
    TEST_ASSERT_TRUE(LsE2e_IsDataValid(&s_rx));
}
