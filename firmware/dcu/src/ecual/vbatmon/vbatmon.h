/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file vbatmon.h
 * @brief KL30 supply monitoring from PA4 with VDDA plausibility.
 */

#ifndef VBATMON_H
#define VBATMON_H

#include "platform/ls_std_types.h"

/** @brief Supply state. */
typedef uint8_t VbatMon_StateType;

#define VBAT_MON_INVALID  ((VbatMon_StateType)0u) /**< Not fitted, VDDA implausible or ADC stale */
#define VBAT_MON_START_OK ((VbatMon_StateType)1u) /**< Inside the start window */
#define VBAT_MON_RUN_OK   ((VbatMon_StateType)2u) /**< Inside the run window */
#define VBAT_MON_UNDER    ((VbatMon_StateType)3u) /**< Under-voltage */
#define VBAT_MON_OVER     ((VbatMon_StateType)4u) /**< Over-voltage, including ADC saturation */

/** @brief Supply status. */
typedef struct
{
    VbatMon_StateType state; /**< Supply state */
    uint16_t kl30Dv;         /**< KL30 in 0.1 V */
} VbatMon_StatusType;

/**
 * @brief Converts the KL30 sample and updates the supply state.
 *
 * @note 10 ms task.
 */
void VbatMon_Main10ms(void);

/**
 * @brief Returns the supply status.
 *
 * @param[out] status Supply status.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType VbatMon_Get(VbatMon_StatusType *status);

#endif /* VBATMON_H */
