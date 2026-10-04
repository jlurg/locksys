/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file winpos.h
 * @brief Window motor speed, direction, relative position and encoder supervision.
 */

#ifndef WINPOS_H
#define WINPOS_H

#include "platform/ls_std_types.h"
#include "ls_enums_gen.h"

/** @brief Encoder-derived window motion data. */
typedef struct
{
    Ls_EncoderStatusType encoderStatus; /**< EncoderStatus */
    int16_t speedRpmX10;                /**< Output-shaft speed in 0.1 rpm, positive = UP */
    int32_t position;                   /**< Relative position in counts, positive = UP */
    uint8_t faults;                     /**< Motion fault bits (NO_MOTION, DIR_MISMATCH) */
} WinPos_StatusType;

/**
 * @brief Initialises the encoder sampling.
 */
void WinPos_Init(void);

/**
 * @brief Samples the encoder counter and runs the NO_MOTION and DIR_MISMATCH supervision.
 *
 * @note 10 ms task.
 */
void WinPos_Main10ms(void);

/**
 * @brief Returns the motion data of the last period.
 *
 * @param[out] status Motion data.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType WinPos_GetStatus(WinPos_StatusType *status);

#endif /* WINPOS_H */
