/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_std_types.h
 * @brief Standard return type and fixed-width types of the DCU firmware.
 */

#ifndef LS_STD_TYPES_H
#define LS_STD_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Standard return type of DCU APIs; LS_E_OK is 0. */
typedef uint8_t Ls_ReturnType;

#define LS_E_OK      ((Ls_ReturnType)0u) /**< Operation completed successfully */
#define LS_E_NOT_OK  ((Ls_ReturnType)1u) /**< Operation failed or is not available */
#define LS_E_BUSY    ((Ls_ReturnType)2u) /**< Resource busy; retry later */
#define LS_E_PENDING ((Ls_ReturnType)3u) /**< Operation started; completion reported later */

/** @brief Logic level of a digital pin. */
typedef uint8_t Ls_LevelType;

#define LS_LOW  ((Ls_LevelType)0u) /**< Low level */
#define LS_HIGH ((Ls_LevelType)1u) /**< High level */

#endif /* LS_STD_TYPES_H */
