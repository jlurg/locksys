/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file canif.h
 * @brief CAN interface: RX queues by filter match index, TX pending slots, bus-off state machine.
 */

#ifndef CANIF_H
#define CANIF_H

#include "platform/ls_std_types.h"

/** @brief CAN frame. */
typedef struct
{
    uint32_t id;      /**< Standard identifier */
    uint8_t dlc;      /**< Data length code */
    uint8_t data[8];  /**< Payload */
    uint32_t stampMs; /**< Reception time stamp */
} CanIf_FrameType;

/** @brief Bus state. */
typedef uint8_t CanIf_BusStateType;

#define CAN_IF_BUS_STOPPED ((CanIf_BusStateType)0u) /**< Controller in initialisation mode */
#define CAN_IF_BUS_ON      ((CanIf_BusStateType)1u) /**< Bus on */
#define CAN_IF_BUS_RECOVER ((CanIf_BusStateType)2u) /**< Bus-off recovery running */

/**
 * @brief Queues a received frame.
 *
 * @param[in] filterIndex Filter match index.
 * @param[in] frame Received frame.
 * @note CAN RX interrupt, priority 3.
 */
void CanIf_RxIndication(uint8_t filterIndex, const CanIf_FrameType *frame);

/**
 * @brief Takes the oldest frame of the Com queue.
 *
 * @param[out] frame Oldest application frame.
 * @retval LS_E_OK     Frame returned.
 * @retval LS_E_NOT_OK Queue empty.
 */
Ls_ReturnType CanIf_RxPopCom(CanIf_FrameType *frame);

/**
 * @brief Takes the oldest frame of the Diag queue.
 *
 * @param[out] frame Oldest diagnostic frame.
 * @retval LS_E_OK     Frame returned.
 * @retval LS_E_NOT_OK Queue empty.
 */
Ls_ReturnType CanIf_RxPopDiag(CanIf_FrameType *frame);

/**
 * @brief Stores a frame in its pending slot (newest wins).
 *
 * @param[in] frame Frame to send.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType CanIf_Transmit(const CanIf_FrameType *frame);

/**
 * @brief Bus-off supervision and recovery.
 *
 * @note 10 ms task.
 */
void CanIf_Main10ms(void);

/**
 * @brief Returns the bus state.
 *
 * @return One of CAN_IF_BUS_*.
 */
CanIf_BusStateType CanIf_GetBusState(void);

#endif /* CANIF_H */
