/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file hbridge.h
 * @brief VNH5019 bridge channels: commands, reflex latch and safe switch-off.
 *
 * Commands: DRIVE_UP (INA 1, INB 0, PWM duty), DRIVE_DOWN (0, 1, duty), BRAKE (0, 0, 100 %),
 * OFF (0, 0, 0 %). EN/DIAG is never driven.
 */

#ifndef HBRIDGE_H
#define HBRIDGE_H

#include "platform/ls_std_types.h"

/** @brief Bridge channel. */
typedef uint8_t HBridge_ChannelType;

#define HBRIDGE_CH_WIN   ((HBridge_ChannelType)0u) /**< M1, window */
#define HBRIDGE_CH_LOCK  ((HBridge_ChannelType)1u) /**< M2, lock */
#define HBRIDGE_CH_COUNT (2u)                      /**< Number of channels */

/** @brief Bridge command. */
typedef uint8_t HBridge_CmdType;

#define HBRIDGE_CMD_OFF        ((HBridge_CmdType)0u) /**< Coast: INA = INB = 0, PWM 0 */
#define HBRIDGE_CMD_DRIVE_UP   ((HBridge_CmdType)1u) /**< INA = 1, INB = 0, PWM duty */
#define HBRIDGE_CMD_DRIVE_DOWN ((HBridge_CmdType)2u) /**< INA = 0, INB = 1, PWM duty */
#define HBRIDGE_CMD_BRAKE      ((HBridge_CmdType)3u) /**< INA = INB = 0, PWM 100 % */

/** @brief Reflex latch cause bits. */
typedef uint8_t HBridge_ReflexType;

#define HBRIDGE_REFLEX_DIAG     ((HBridge_ReflexType)0x01u) /**< EN/DIAG falling edge */
#define HBRIDGE_REFLEX_OC       ((HBridge_ReflexType)0x02u) /**< Over-current backstop */
#define HBRIDGE_REFLEX_MOTION   ((HBridge_ReflexType)0x04u) /**< NO_MOTION */
#define HBRIDGE_REFLEX_DIRM     ((HBridge_ReflexType)0x08u) /**< DIR_MISMATCH */
#define HBRIDGE_REFLEX_HANG     ((HBridge_ReflexType)0x10u) /**< Hang monitor */
#define HBRIDGE_REFLEX_READBACK ((HBridge_ReflexType)0x20u) /**< INA/INB readback mismatch */

/** @brief Channel status. */
typedef struct
{
    HBridge_CmdType cmd;       /**< Applied command */
    uint16_t dutyPermille;     /**< Applied duty */
    HBridge_ReflexType reflex; /**< Reflex latch causes */
    uint16_t currentMa;        /**< Filtered current */
    uint32_t driveOnMs;        /**< Time stamp of the last drive-on */
} HBridge_StatusType;

/**
 * @brief Switches both channels off and clears the reflex latches.
 */
void HBridge_Init(void);

/**
 * @brief Applies a command to a channel unless a reflex latch cause blocks it.
 *
 * @param[in] channel      Channel.
 * @param[in] cmd          Command.
 * @param[in] dutyPermille Drive duty for DRIVE_UP and DRIVE_DOWN; ignored otherwise.
 * @retval LS_E_OK     Command applied.
 * @retval LS_E_NOT_OK Drive refused (reflex latch, invalid argument or drive not available);
 *                     OFF and BRAKE are never refused for a valid channel.
 */
Ls_ReturnType HBridge_Set(HBridge_ChannelType channel, HBridge_CmdType cmd, uint16_t dutyPermille);

/**
 * @brief Sets a reflex latch cause and stops the channel.
 *
 * @param[in] channel Channel.
 * @param[in] cause   One HBRIDGE_REFLEX_* bit.
 * @note Callable from interrupt context.
 */
void HBridge_ReflexStop(HBridge_ChannelType channel, HBridge_ReflexType cause);

/**
 * @brief Window EN/DIAG falling-edge reflex.
 *
 * @note Interrupt priority 1.
 */
void HBridge_DiagIsr(void);

/**
 * @brief Clears reflex latch causes of a channel (release policy of the WinCtrl gate).
 *
 * @param[in] channel Channel.
 * @param[in] causes  HBRIDGE_REFLEX_* bits to clear; HBRIDGE_REFLEX_HANG is never cleared.
 */
void HBridge_ClearReflex(HBridge_ChannelType channel, HBridge_ReflexType causes);

/**
 * @brief Switches every bridge off at once: INA = INB = 0 and PWM low on both channels.
 *
 * @note Callable from any context, including fault and hang handlers.
 */
void HBridge_AllOff(void);

/**
 * @brief Current filtering, over-current backstop, soft start and INA/INB readback.
 *
 * @note 1 ms task.
 */
void HBridge_Main1ms(void);

/**
 * @brief Returns the status of a channel.
 *
 * @param[in]  channel Channel.
 * @param[out] status  Channel status.
 * @retval LS_E_OK     Status written.
 * @retval LS_E_NOT_OK Invalid argument.
 */
Ls_ReturnType HBridge_GetStatus(HBridge_ChannelType channel, HBridge_StatusType *status);

#endif /* HBRIDGE_H */
