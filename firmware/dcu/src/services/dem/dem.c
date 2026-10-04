/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file dem.c
 * @brief Diagnostic event manager: DTC status, noinit fault memory and fan-out.
 *
 * Skeleton: the behaviour is delivered at milestone M2; until then every
 * function returns its safe value.
 */

#include "services/dem/dem.h"

void Dem_Init(void)
{
}

void Dem_Report(Dem_DtcIdType dtc, Dem_EventStatusType status)
{
    (void)dtc;
    (void)status;
}

void Dem_ReportIsr(Dem_DtcIdType dtc)
{
    (void)dtc;
}

void Dem_Main100ms(void)
{
}

uint8_t Dem_GetStatus(Dem_DtcIdType dtc)
{
    (void)dtc;
    return (uint8_t)0;
}

uint8_t Dem_ConfirmedCount(void)
{
    return (uint8_t)0;
}

Ls_ReturnType Dem_ClearAll(void)
{
    return LS_E_NOT_OK;
}
