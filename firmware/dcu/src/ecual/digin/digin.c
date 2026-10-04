/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file digin.c
 * @brief Debounced digital inputs: lock position switch (PB12) and B1 (DEV).
 *
 * Skeleton: the behaviour is delivered at milestone M2; until then every
 * function returns its safe value.
 */

#include "ecual/digin/digin.h"

void DigIn_Main5ms(void)
{
}

DigIn_StateType DigIn_Get(DigIn_InputType input)
{
    (void)input;
    return (DigIn_StateType)0;
}
