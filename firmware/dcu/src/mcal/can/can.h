/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file can.h
 * @brief bxCAN driver: 500 kbit/s, identifier-list filters, mailboxes, error state.
 *
 * Bit timing CAN_BTR = 0x00050008 (BRP 9, 1 + 6 + 1 tq, sample point 87.5 %), ABOM = 0,
 * TTCM = 0 (ES096 section 2.11.1), TXFP = 0, NART = 0. The controller stays in initialisation
 * mode until Can_Start().
 */

#ifndef CAN_H
#define CAN_H

#include "platform/ls_std_types.h"

/** @brief Error state of the controller. */
typedef uint8_t Can_ErrorStateType;

#define CAN_ERRSTATE_ACTIVE  ((Can_ErrorStateType)0u) /**< Error active */
#define CAN_ERRSTATE_PASSIVE ((Can_ErrorStateType)1u) /**< Error passive */
#define CAN_ERRSTATE_BUSOFF  ((Can_ErrorStateType)2u) /**< Bus-off */

/** @brief Bit timing register value for 500 kbit/s at PCLK1 = 36 MHz. */
#define CAN_BTR_500K (0x00050008u)

/**
 * @brief Configures bit timing and filters; leaves the controller in initialisation mode.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Can_Init(void);

/**
 * @brief Leaves initialisation mode.
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 * @note Called by Sched_Start() context only, never after a clock failure.
 */
Ls_ReturnType Can_Start(void);

/**
 * @brief Returns the controller to initialisation mode (CAN silent).
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Can_Stop(void);

/**
 * @brief Loads a frame into a free transmit mailbox.
 *
 * @param[in] id Standard identifier.
 * @param[in] data Payload.
 * @param[in] dlc Data length code, 0-8.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Can_Write(uint32_t id, const uint8_t *data, uint8_t dlc);

/**
 * @brief Returns the error state of the controller.
 *
 * @return One of CAN_ERRSTATE_*.
 */
Can_ErrorStateType Can_GetErrorState(void);

/**
 * @brief Starts a bus-off recovery through initialisation mode (INRQ).
 *
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Can_Recover(void);

/**
 * @brief Receive FIFO interrupt service: frame and filter match index to CanIf.
 *
 * @note Interrupt priority 3.
 */
void Can_RxIsr(void);

/**
 * @brief Transmit mailbox interrupt service: loads the next pending frame.
 *
 * @note Interrupt priority 4.
 */
void Can_TxIsr(void);

/**
 * @brief Status change and error interrupt service.
 *
 * @note Interrupt priority 3.
 */
void Can_SceIsr(void);

#endif /* CAN_H */
