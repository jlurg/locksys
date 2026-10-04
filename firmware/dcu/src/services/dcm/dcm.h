/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file dcm.h
 * @brief UDS-lite: sessions, services and negative responses.
 */

#ifndef DCM_H
#define DCM_H

#include "platform/ls_std_types.h"

/** @brief Diagnostic session. */
typedef uint8_t Dcm_SessionType;

#define DCM_SESSION_DEFAULT  ((Dcm_SessionType)1u) /**< Default session (0x01) */
#define DCM_SESSION_EXTENDED ((Dcm_SessionType)3u) /**< Extended session (0x03) */

/**
 * @brief Enters the default session.
 */
void Dcm_Init(void);

/**
 * @brief Request processing and S3 supervision.
 *
 * @note 10 ms task.
 */
void Dcm_Main10ms(void);

/**
 * @brief Returns the active session.
 *
 * @return One of DCM_SESSION_*.
 */
Dcm_SessionType Dcm_GetSession(void);

#endif /* DCM_H */
