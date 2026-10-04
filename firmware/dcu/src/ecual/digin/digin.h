/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file digin.h
 * @brief Debounced digital inputs: lock position switch (PB12) and B1 (DEV).
 */

#ifndef DIGIN_H
#define DIGIN_H

#include "platform/ls_std_types.h"

/** @brief Digital input. */
typedef uint8_t DigIn_InputType;

#define DIG_IN_LOCK_SWITCH ((DigIn_InputType)0u) /**< Lock position switch */
#define DIG_IN_B1          ((DigIn_InputType)1u) /**< User button, DEV builds */

/** @brief Debounced state. */
typedef uint8_t DigIn_StateType;

#define DIG_IN_UNKNOWN ((DigIn_StateType)0u) /**< Not yet debounced */
#define DIG_IN_LOW     ((DigIn_StateType)1u) /**< Stable low */
#define DIG_IN_HIGH    ((DigIn_StateType)2u) /**< Stable high */

/**
 * @brief Samples the inputs; a state is accepted after 4 equal samples.
 *
 * @note 5 ms task.
 */
void DigIn_Main5ms(void);

/**
 * @brief Returns the debounced state of an input.
 *
 * @param[in] input Input.
 * @return One of DIG_IN_*.
 */
DigIn_StateType DigIn_Get(DigIn_InputType input);

#endif /* DIGIN_H */
