/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file gpio.h
 * @brief Pin configuration, safe initialisation, AFIO remap and pin access.
 *
 * Logical pins (GPIO_PIN_*) and their configuration are defined in cfg/mcal/gpio_cfg.h
 * (pin allocation of LS-SAIC-001 section 3.1).
 */

#ifndef GPIO_H
#define GPIO_H

#include "platform/ls_std_types.h"

/** @brief GPIO port. */
typedef uint8_t Gpio_PortType;

#define GPIO_PORT_A     ((Gpio_PortType)0u) /**< GPIOA */
#define GPIO_PORT_B     ((Gpio_PortType)1u) /**< GPIOB */
#define GPIO_PORT_C     ((Gpio_PortType)2u) /**< GPIOC */
#define GPIO_PORT_D     ((Gpio_PortType)3u) /**< GPIOD */
#define GPIO_PORT_COUNT (4u)                /**< Number of ports used */

/** @brief Pin mode: the 4-bit CNF[1:0] MODE[1:0] field of GPIOx_CRL/CRH (RM0008 section 9.2). */
typedef uint8_t Gpio_ModeType;

#define GPIO_MODE_ANALOG      ((Gpio_ModeType)0x0u) /**< Analog input */
#define GPIO_MODE_IN_FLOATING ((Gpio_ModeType)0x4u) /**< Floating input */
#define GPIO_MODE_IN_PULL ((Gpio_ModeType)0x8u) /**< Input with pull-up (level high) or pull-down */
#define GPIO_MODE_OUT_PP_2MHZ  ((Gpio_ModeType)0x2u) /**< General-purpose push-pull, 2 MHz */
#define GPIO_MODE_OUT_PP_10MHZ ((Gpio_ModeType)0x1u) /**< General-purpose push-pull, 10 MHz */
#define GPIO_MODE_AF_PP_10MHZ  ((Gpio_ModeType)0x9u) /**< Alternate function push-pull, 10 MHz */
#define GPIO_MODE_AF_OD_2MHZ   ((Gpio_ModeType)0xEu) /**< Alternate function open drain, 2 MHz */

/** @brief Logical pin identifier; index into the pin table of gpio_cfg.c. */
typedef uint8_t Gpio_PinIdType;

/** @brief Configuration of one logical pin. */
typedef struct
{
    Gpio_PortType port;     /**< Port */
    uint8_t pin;            /**< Pin number 0-15 */
    Gpio_ModeType mode;     /**< Mode after Gpio_Init() */
    Ls_LevelType initLevel; /**< Output level, or pull direction for GPIO_MODE_IN_PULL */
} Gpio_PinCfgType;

/**
 * @brief First initialisation step: drives the bridge inputs low and makes EN/DIAG inputs.
 *
 * Enables the GPIO and AFIO clocks, drives both bridge PWM pins and INA/INB low as
 * push-pull outputs, configures EN/DIAG as floating inputs and the trace pins as low outputs.
 *
 * @pre Called before the clock set-up; touches only the pins of GpioCfg_SafePins.
 */
void Gpio_InitSafe(void);

/**
 * @brief Writes AFIO_MAPR once and configures every pin of the pin table.
 *
 * @retval LS_E_OK     Configuration written.
 * @retval LS_E_NOT_OK Called more than once; nothing written.
 * @pre Gpio_InitSafe() has completed; no timer output is enabled yet.
 */
Ls_ReturnType Gpio_Init(void);

/**
 * @brief Changes the mode of one pin (for example PWM pins to alternate function).
 *
 * @param[in] pin  Logical pin.
 * @param[in] mode New mode.
 * @note Read-modify-write of CRL/CRH: call from initialisation only.
 */
void Gpio_SetMode(Gpio_PinIdType pin, Gpio_ModeType mode);

/**
 * @brief Sets an output pin with a single BSRR store.
 *
 * @param[in] pin   Logical pin.
 * @param[in] level LS_LOW or LS_HIGH.
 */
void Gpio_Write(Gpio_PinIdType pin, Ls_LevelType level);

/**
 * @brief Reads the input level of a pin (IDR).
 *
 * @param[in] pin Logical pin.
 * @return LS_LOW or LS_HIGH.
 */
Ls_LevelType Gpio_Read(Gpio_PinIdType pin);

/**
 * @brief Reads back the output latch of a pin (ODR).
 *
 * @param[in] pin Logical pin.
 * @return LS_LOW or LS_HIGH.
 */
Ls_LevelType Gpio_ReadOutput(Gpio_PinIdType pin);

/**
 * @brief Sets and clears trace pins TRACE0-TRACE3 (PC0-PC3) with one GPIOC BSRR store.
 *
 * @param[in] setMask   Bits 0-3: trace pins to set.
 * @param[in] clearMask Bits 0-3: trace pins to clear; set wins when a bit is in both masks.
 * @note Compiles to nothing when LS_CFG_TRACE is 0.
 */
void Gpio_TraceSet(uint8_t setMask, uint8_t clearMask);

#endif /* GPIO_H */
