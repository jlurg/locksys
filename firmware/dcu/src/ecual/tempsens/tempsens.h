/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file tempsens.h
 * @brief Temperature sensor port; the TMP117 adapter is the default.
 */

#ifndef TEMPSENS_H
#define TEMPSENS_H

#include "platform/ls_std_types.h"

/** @brief Result of a completed sample. */
typedef struct
{
    int16_t cdeg; /**< Temperature in 0.01 degC */
    bool valid;   /**< false for a failed sample */
} TempSens_ResultType;

/**
 * @brief Starts the identity check (TMP117: 0x0117).
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType TempSens_StartIdentify(void);

/**
 * @brief Starts a one-shot conversion.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType TempSens_StartSample(void);

/**
 * @brief Polls the running job.
 *
 * @param[out] result Sample, written when LS_E_OK is returned.
 * @retval LS_E_OK      Sample completed.
 * @retval LS_E_PENDING Job still running.
 * @retval LS_E_NOT_OK  Job failed or no job started.
 */
Ls_ReturnType TempSens_Poll(TempSens_ResultType *result);

#endif /* TEMPSENS_H */
