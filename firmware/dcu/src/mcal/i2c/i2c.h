/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file i2c.h
 * @brief I2C1 master job state machine (PB8/PB9, remap) with the ES096 section 2.8 workarounds.
 */

#ifndef I2C_H
#define I2C_H

#include "platform/ls_std_types.h"

/** @brief I2C job: write txLen bytes, then read rxLen bytes with a new START. */
typedef struct
{
    uint8_t address;       /**< 7-bit address */
    const uint8_t *txData; /**< Bytes to write; NULL when txLen is 0 */
    uint8_t txLen;         /**< Number of bytes to write */
    uint8_t *rxData;       /**< Read buffer; NULL when rxLen is 0 */
    uint8_t rxLen;         /**< Number of bytes to read */
} I2c_JobType;

/** @brief Job status. */
typedef uint8_t I2c_StatusType;

#define I2C_STATUS_IDLE  ((I2c_StatusType)0u) /**< No job */
#define I2C_STATUS_BUSY  ((I2c_StatusType)1u) /**< Job running */
#define I2C_STATUS_DONE  ((I2c_StatusType)2u) /**< Last job completed */
#define I2C_STATUS_ERROR ((I2c_StatusType)3u) /**< Last job failed (NACK, timeout, bus error) */

/**
 * @brief Configures I2C1 at i2c_clock_hz; clears a stuck bus when SDA is low.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType I2c_Init(void);

/**
 * @brief Starts a job.
 *
 * @param[in] job Job; must stay valid until the job completes.
 * @retval LS_E_OK     Job started.
 * @retval LS_E_BUSY   A job is running.
 * @retval LS_E_NOT_OK Invalid job or driver not ready.
 */
Ls_ReturnType I2c_Submit(const I2c_JobType *job);

/**
 * @brief Returns the status of the current or last job.
 *
 * @return One of I2C_STATUS_*.
 */
I2c_StatusType I2c_GetStatus(void);

/**
 * @brief Job timeout supervision and bus recovery chain.
 *
 * @note 5 ms task.
 */
void I2c_MainFunction(void);

/**
 * @brief I2C1 event interrupt service.
 *
 * @note Interrupt priority 0; never masked by BASEPRI.
 */
void I2c_EvIsr(void);

/**
 * @brief I2C1 error interrupt service.
 *
 * @note Interrupt priority 0.
 */
void I2c_ErIsr(void);

#endif /* I2C_H */
