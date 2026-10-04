/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_e2e.c
 * @brief LockSys CAN E2E profile (LS-SAIC-001 section 7.2).
 */

#include "ls_e2e/ls_e2e.h"

#include <stddef.h>

#include "ls_common/ls_crc8.h"

#define LS_E2E_SAT8  (0xFFu)
#define LS_E2E_SAT16 (0xFFFFu)

static uint8_t LsE2e_Inc8(uint8_t value)
{
    return (value < LS_E2E_SAT8) ? (uint8_t)(value + 1u) : value;
}

static uint16_t LsE2e_Inc16(uint16_t value)
{
    return (value < LS_E2E_SAT16) ? (uint16_t)(value + 1u) : value;
}

static bool LsE2e_IsFrameArgValid(const LsE2e_ConfigType *cfg, const uint8_t *frame, uint8_t len)
{
    return LsE2e_IsConfigValid(cfg) && (frame != NULL) && (len == cfg->dlc);
}

/* Counter and state update after a frame (status other than BAD_ARG). */
static void LsE2e_UpdateState(const LsE2e_ConfigType *cfg, LsE2e_RxStateType *rx,
                              LsE2e_CheckStatusType status)
{
    if (status == LS_E2E_STATUS_OK)
    {
        rx->errCount = 0u;
        rx->okCount = LsE2e_Inc8(rx->okCount);
        if ((cfg->mode == LS_E2E_MODE_EVENT) || (rx->okCount >= cfg->nOkValid))
        {
            rx->state = LS_E2E_STATE_VALID;
        }
    }
    else
    {
        rx->okCount = 0u;
        rx->errCount = LsE2e_Inc8(rx->errCount);
        if (rx->errCount >= cfg->nErrInvalid)
        {
            rx->state = LS_E2E_STATE_INVALID;
        }
    }
    rx->lastStatus = status;
}

/* Alive counter evaluation of a frame whose DLC and CRC are correct. */
static LsE2e_CheckStatusType LsE2e_CheckCounter(const LsE2e_ConfigType *cfg, LsE2e_RxStateType *rx,
                                                uint8_t counter)
{
    LsE2e_CheckStatusType status = LS_E2E_STATUS_OK;

    if ((cfg->mode == LS_E2E_MODE_CYCLIC) && rx->refValid)
    {
        uint8_t delta = (uint8_t)((uint8_t)(counter - rx->refCounter) & LS_E2E_COUNTER_MASK);

        if (delta == 0u)
        {
            status = LS_E2E_STATUS_REPEATED;
            rx->repeated = LsE2e_Inc16(rx->repeated);
        }
        else if (delta > cfg->maxDelta)
        {
            status = LS_E2E_STATUS_WRONG_SEQUENCE;
            rx->seqErrors = LsE2e_Inc16(rx->seqErrors);
            rx->refCounter = counter;
        }
        else
        {
            /* 1 <= delta <= MaxDelta: in sequence. */
        }
    }
    if (status == LS_E2E_STATUS_OK)
    {
        rx->refCounter = counter;
        rx->refValid = true;
    }
    return status;
}

bool LsE2e_IsConfigValid(const LsE2e_ConfigType *cfg)
{
    bool valid = false;

    if ((cfg != NULL) && (cfg->dlc >= LS_E2E_MIN_DLC) && (cfg->dlc <= LS_E2E_MAX_DLC) &&
        (cfg->nOkValid > 0u) && (cfg->nErrInvalid > 0u))
    {
        if (cfg->mode == LS_E2E_MODE_CYCLIC)
        {
            valid = (cfg->maxDelta > 0u) && (cfg->maxDelta <= LS_E2E_MAX_DELTA_MAX);
        }
        else
        {
            valid = cfg->mode == LS_E2E_MODE_EVENT;
        }
    }
    return valid;
}

uint8_t LsE2e_Crc(uint16_t dataId, const uint8_t *frame, uint8_t len)
{
    const uint8_t id[2] = {(uint8_t)(dataId & 0xFFu), (uint8_t)(dataId >> 8u)};
    uint8_t crc = LsCrc8_J1850Update(LS_CRC8_J1850_INIT, id, 2u);

    if ((frame != NULL) && (len > 1u))
    {
        crc = LsCrc8_J1850Update(crc, &frame[1], (uint32_t)len - 1u);
    }
    return (uint8_t)(crc ^ LS_CRC8_J1850_XOROUT);
}

LsE2e_ResultType LsE2e_TxInit(LsE2e_TxStateType *tx)
{
    LsE2e_ResultType result = LS_E2E_E_BAD_ARG;

    if (tx != NULL)
    {
        tx->counter = 0u;
        result = LS_E2E_E_OK;
    }
    return result;
}

LsE2e_ResultType LsE2e_ProtectWithCounter(const LsE2e_ConfigType *cfg, uint8_t counter,
                                          uint8_t *frame, uint8_t len)
{
    LsE2e_ResultType result = LS_E2E_E_BAD_ARG;

    if (LsE2e_IsFrameArgValid(cfg, frame, len))
    {
        frame[1] = (uint8_t)((frame[1] & 0xF0u) | (counter & LS_E2E_COUNTER_MASK));
        frame[0] = LsE2e_Crc(cfg->dataId, frame, len);
        result = LS_E2E_E_OK;
    }
    return result;
}

LsE2e_ResultType LsE2e_Protect(const LsE2e_ConfigType *cfg, LsE2e_TxStateType *tx, uint8_t *frame,
                               uint8_t len)
{
    LsE2e_ResultType result = LS_E2E_E_BAD_ARG;

    if (tx != NULL)
    {
        result = LsE2e_ProtectWithCounter(cfg, tx->counter, frame, len);
        if (result == LS_E2E_E_OK)
        {
            tx->counter = (uint8_t)((uint8_t)(tx->counter + 1u) & LS_E2E_COUNTER_MASK);
        }
    }
    return result;
}

LsE2e_ResultType LsE2e_RxInit(LsE2e_RxStateType *rx)
{
    LsE2e_ResultType result = LS_E2E_E_BAD_ARG;

    if (rx != NULL)
    {
        rx->state = LS_E2E_STATE_INIT;
        rx->lastStatus = LS_E2E_STATUS_OK;
        rx->refCounter = 0u;
        rx->refValid = false;
        rx->okCount = 0u;
        rx->errCount = 0u;
        rx->crcErrors = 0u;
        rx->seqErrors = 0u;
        rx->repeated = 0u;
        rx->timeouts = 0u;
        result = LS_E2E_E_OK;
    }
    return result;
}

LsE2e_CheckStatusType LsE2e_Check(const LsE2e_ConfigType *cfg, LsE2e_RxStateType *rx,
                                  const uint8_t *frame, uint8_t len)
{
    LsE2e_CheckStatusType status = LS_E2E_STATUS_BAD_ARG;

    if ((rx != NULL) && LsE2e_IsConfigValid(cfg) && (frame != NULL))
    {
        if ((len != cfg->dlc) || (frame[0] != LsE2e_Crc(cfg->dataId, frame, len)))
        {
            status = LS_E2E_STATUS_CRC_ERROR;
            rx->crcErrors = LsE2e_Inc16(rx->crcErrors);
        }
        else
        {
            status = LsE2e_CheckCounter(cfg, rx, (uint8_t)(frame[1] & LS_E2E_COUNTER_MASK));
        }
        LsE2e_UpdateState(cfg, rx, status);
    }
    return status;
}

void LsE2e_RxTimeout(LsE2e_RxStateType *rx)
{
    if (rx != NULL)
    {
        rx->state = LS_E2E_STATE_INVALID;
        rx->refValid = false;
        rx->refCounter = 0u;
        rx->okCount = 0u;
        rx->errCount = 0u;
        rx->timeouts = LsE2e_Inc16(rx->timeouts);
    }
}

bool LsE2e_IsDataValid(const LsE2e_RxStateType *rx)
{
    return (rx != NULL) && (rx->lastStatus == LS_E2E_STATUS_OK) &&
           (rx->state == LS_E2E_STATE_VALID);
}
