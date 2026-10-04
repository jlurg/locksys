/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file gpio_cfg.h
 * @brief Logical pins and AFIO remap of the DCU (LS-SAIC-001 section 3.1).
 */

#ifndef GPIO_CFG_H
#define GPIO_CFG_H

#include "mcal/gpio/gpio.h"

/**
 * @brief AFIO_MAPR value, written once: SWJ_CFG = 010 (SWD only), TIM3_REMAP = 11,
 *        TIM2_REMAP = 01, I2C1_REMAP = 1, CAN_REMAP = 00, USART2_REMAP = 0.
 */
#define GPIO_CFG_AFIO_MAPR (0x02000D02u)

#define GPIO_PIN_WIN_PWM ((Gpio_PinIdType)0u) /**< PC7  TIM3_CH2, window PWM */
#define GPIO_PIN_WIN_INA ((Gpio_PinIdType)1u) /**< PA10 window INA */
#define GPIO_PIN_WIN_INB \
    ((Gpio_PinIdType)2u) /**< PB5  window INB (never AF, ES096 section 2.3.8) */
#define GPIO_PIN_WIN_DIAG ((Gpio_PinIdType)3u) /**< PB10 window EN/DIAG, never driven */
#define GPIO_PIN_LOCK_PWM ((Gpio_PinIdType)4u) /**< PB6  TIM4_CH1, lock PWM */
#define GPIO_PIN_LOCK_INA ((Gpio_PinIdType)5u) /**< PA8  lock INA */
#define GPIO_PIN_LOCK_INB ((Gpio_PinIdType)6u) /**< PA9  lock INB */
#define GPIO_PIN_LOCK_DIAG \
    ((Gpio_PinIdType)7u) /**< PB4 (option B) or PA6 (option A), never driven */
#define GPIO_PIN_TRACE0    ((Gpio_PinIdType)8u)  /**< PC0 */
#define GPIO_PIN_TRACE1    ((Gpio_PinIdType)9u)  /**< PC1 */
#define GPIO_PIN_TRACE2    ((Gpio_PinIdType)10u) /**< PC2 */
#define GPIO_PIN_TRACE3    ((Gpio_PinIdType)11u) /**< PC3 */
#define GPIO_PIN_WIN_CS    ((Gpio_PinIdType)12u) /**< PA0  ADC12_IN0 */
#define GPIO_PIN_LOCK_CS   ((Gpio_PinIdType)13u) /**< PA1  ADC12_IN1 */
#define GPIO_PIN_KL30      ((Gpio_PinIdType)14u) /**< PA4  ADC12_IN4 */
#define GPIO_PIN_ENC_A     ((Gpio_PinIdType)15u) /**< PA15 TIM2_CH1 */
#define GPIO_PIN_ENC_B     ((Gpio_PinIdType)16u) /**< PB3  TIM2_CH2 */
#define GPIO_PIN_CAN_RX    ((Gpio_PinIdType)17u) /**< PA11 */
#define GPIO_PIN_CAN_TX    ((Gpio_PinIdType)18u) /**< PA12 */
#define GPIO_PIN_I2C_SCL   ((Gpio_PinIdType)19u) /**< PB8 */
#define GPIO_PIN_I2C_SDA   ((Gpio_PinIdType)20u) /**< PB9 */
#define GPIO_PIN_VCP_TX    ((Gpio_PinIdType)21u) /**< PA2  USART2_TX */
#define GPIO_PIN_VCP_RX    ((Gpio_PinIdType)22u) /**< PA3  USART2_RX */
#define GPIO_PIN_LOCK_SW   ((Gpio_PinIdType)23u) /**< PB12 lock position switch, pull-up */
#define GPIO_PIN_LD2       ((Gpio_PinIdType)24u) /**< PA5  heartbeat LED */
#define GPIO_PIN_B1        ((Gpio_PinIdType)25u) /**< PC13 user button */
#define GPIO_CFG_PIN_COUNT (26u)                 /**< Number of logical pins */

/** @brief Number of pins configured by Gpio_InitSafe(). */
#define GPIO_CFG_SAFE_PIN_COUNT (12u)

/** @brief Pin table indexed by Gpio_PinIdType. */
extern const Gpio_PinCfgType GpioCfg_Pins[GPIO_CFG_PIN_COUNT];

/** @brief Pins configured by Gpio_InitSafe(), in configuration order. */
extern const Gpio_PinIdType GpioCfg_SafePins[GPIO_CFG_SAFE_PIN_COUNT];

#endif /* GPIO_CFG_H */
