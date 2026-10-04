/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file tlm.c
 * @brief UART telemetry version 1 (LS-IF-003): sentences, logs and the TX ring.
 *
 * Skeleton: the behaviour is delivered at milestone M1; until then every
 * function returns its safe value.
 */

#include "services/tlm/tlm.h"

void Tlm_Init(void)
{
}

void Tlm_Temp(int16_t cdeg, Ls_TempStatusType status)
{
    (void)cdeg;
    (void)status;
}

void Tlm_Status(void)
{
}

void Tlm_Motion(const Tlm_MotionType *motion)
{
    (void)motion;
}

void Tlm_Version(void)
{
}

void Tlm_Reset(Ls_ResetReasonType reason)
{
    (void)reason;
}

void Tlm_Dtc(uint32_t dtc, uint8_t status)
{
    (void)dtc;
    (void)status;
}

void Tlm_Log(Tlm_LevelType level, const char *module, uint16_t code, const char *text)
{
    (void)level;
    (void)module;
    (void)code;
    (void)text;
}

void Tlm_Main10ms(void)
{
}

void Tlm_Main1000ms(void)
{
}
