/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file dem.h
 * @brief Diagnostic event manager: DTC status, noinit fault memory and fan-out.
 */

#ifndef DEM_H
#define DEM_H

#include "platform/ls_std_types.h"
#include "dem_dtc_gen.h"

/** @brief Reported event status. */
typedef uint8_t Dem_EventStatusType;

#define DEM_EVENT_PASSED ((Dem_EventStatusType)0u) /**< Test passed */
#define DEM_EVENT_FAILED ((Dem_EventStatusType)1u) /**< Test failed */

/**
 * @brief Restores the fault memory from noinit RAM and starts a new operation cycle.
 */
void Dem_Init(void);

/**
 * @brief Reports a test result from task context.
 *
 * @param[in] dtc DTC.
 * @param[in] status Test result.
 */
void Dem_Report(Dem_DtcIdType dtc, Dem_EventStatusType status);

/**
 * @brief Reports a failed test from interrupt context (one flag per event).
 *
 * @param[in] dtc DTC.
 * @note Any interrupt priority.
 */
void Dem_ReportIsr(Dem_DtcIdType dtc);

/**
 * @brief Processes interrupt flags, confirmation, aging and the fan-out.
 *
 * @note 100 ms task.
 */
void Dem_Main100ms(void);

/**
 * @brief Returns the status byte of a DTC.
 *
 * @param[in] dtc DTC.
 * @return Status bits 0, 2, 3 and 5.
 */
uint8_t Dem_GetStatus(Dem_DtcIdType dtc);

/**
 * @brief Returns the number of confirmed DTCs.
 *
 * @return Count.
 */
uint8_t Dem_ConfirmedCount(void);

/**
 * @brief Clears the fault memory.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Dem_ClearAll(void);

#endif /* DEM_H */
