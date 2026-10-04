/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cantrcv.c
 * @brief CAN transceiver mode interface.
 *
 * Skeleton: the behaviour is delivered at milestone M1; until then every
 * function returns its safe value.
 */

#include "ecual/cantrcv/cantrcv.h"

Ls_ReturnType CanTrcv_Init(void)
{
    return LS_E_OK;
}

CanTrcv_ModeType CanTrcv_GetMode(void)
{
    return CAN_TRCV_MODE_NORMAL;
}
