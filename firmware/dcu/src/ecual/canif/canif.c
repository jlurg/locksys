/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file canif.c
 * @brief CAN interface: RX queues by filter match index, TX pending slots, bus-off state machine.
 *
 * Skeleton: the behaviour is delivered at milestone M1; until then every
 * function returns its safe value.
 */

#include "ecual/canif/canif.h"

void CanIf_RxIndication(uint8_t filterIndex, const CanIf_FrameType *frame)
{
    (void)filterIndex;
    (void)frame;
}

Ls_ReturnType CanIf_RxPopCom(CanIf_FrameType *frame)
{
    (void)frame;
    return LS_E_NOT_OK;
}

Ls_ReturnType CanIf_RxPopDiag(CanIf_FrameType *frame)
{
    (void)frame;
    return LS_E_NOT_OK;
}

Ls_ReturnType CanIf_Transmit(const CanIf_FrameType *frame)
{
    (void)frame;
    return LS_E_NOT_OK;
}

void CanIf_Main10ms(void)
{
}

CanIf_BusStateType CanIf_GetBusState(void)
{
    return (CanIf_BusStateType)0;
}
