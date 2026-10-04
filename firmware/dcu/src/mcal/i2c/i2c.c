/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file i2c.c
 * @brief I2C1 master job state machine (PB8/PB9, remap) with the ES096 section 2.8 workarounds.
 *
 * Skeleton: the behaviour is delivered at milestone M2; until then every
 * function returns its safe value.
 */

#include "mcal/i2c/i2c.h"

Ls_ReturnType I2c_Init(void)
{
    return LS_E_NOT_OK;
}

Ls_ReturnType I2c_Submit(const I2c_JobType *job)
{
    (void)job;
    return LS_E_NOT_OK;
}

I2c_StatusType I2c_GetStatus(void)
{
    return (I2c_StatusType)0;
}

void I2c_MainFunction(void)
{
}

void I2c_EvIsr(void)
{
}

void I2c_ErIsr(void)
{
}
