/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file tlm.h
 * @brief UART telemetry version 1 (LS-IF-003): sentences, logs and the TX ring.
 */

#ifndef TLM_H
#define TLM_H

#include "platform/ls_std_types.h"
#include "ls_enums_gen.h"

/** @brief Log level. */
typedef uint8_t Tlm_LevelType;

#define TLM_LEVEL_E ((Tlm_LevelType)0u) /**< Error */
#define TLM_LEVEL_W ((Tlm_LevelType)1u) /**< Warning */
#define TLM_LEVEL_I ((Tlm_LevelType)2u) /**< Information */
#define TLM_LEVEL_D ((Tlm_LevelType)3u) /**< Debug; compiled out of Release */

/** @brief Content of a `$LSMOT` sentence. */
typedef struct
{
    Ls_EncoderStatusType encoderStatus; /**< EncoderStatus */
    int16_t speedRpmX10;                /**< Output-shaft speed in 0.1 rpm */
    uint16_t dutyPermille;              /**< Commanded duty */
    uint16_t currentMa;                 /**< Filtered window current */
    int32_t position;                   /**< Relative position in counts */
} Tlm_MotionType;

/**
 * @brief Initialises the ring and the sequence counters.
 */
void Tlm_Init(void);

/**
 * @brief Emits `$LSTMP`.
 *
 * @param[in] cdeg Temperature in 0.01 degC.
 * @param[in] status TempStatus.
 */
void Tlm_Temp(int16_t cdeg, Ls_TempStatusType status);

/**
 * @brief Emits `$LSSTA` from RTE data.
 */
void Tlm_Status(void);

/**
 * @brief Emits `$LSMOT`.
 *
 * @param[in] motion Sentence content.
 */
void Tlm_Motion(const Tlm_MotionType *motion);

/**
 * @brief Emits `$LSVER`.
 */
void Tlm_Version(void);

/**
 * @brief Emits `$LSRST`.
 *
 * @param[in] reason Decoded reset reason.
 */
void Tlm_Reset(Ls_ResetReasonType reason);

/**
 * @brief Emits `$LSDTC`.
 *
 * @param[in] dtc DTC code with failure type byte.
 * @param[in] status Status byte.
 */
void Tlm_Dtc(uint32_t dtc, uint8_t status);

/**
 * @brief Emits a `#LOG` line, limited to n_log_max_per_s lines per second.
 *
 * @param[in] level Level.
 * @param[in] module Module code (LS-DCU-SAD-001 section 10.4).
 * @param[in] code Event code.
 * @param[in] text Optional text; NULL for none.
 */
void Tlm_Log(Tlm_LevelType level, const char *module, uint16_t code, const char *text);

/**
 * @brief Starts the DMA transmission of pending ring content.
 *
 * @note 10 ms task.
 */
void Tlm_Main10ms(void);

/**
 * @brief Emits the periodic sentences.
 *
 * @note 1000 ms task.
 */
void Tlm_Main1000ms(void);

#endif /* TLM_H */
