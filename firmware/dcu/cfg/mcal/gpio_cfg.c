/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file gpio_cfg.c
 * @brief Pin table of the DCU (LS-SAIC-001 section 3.1, LS-DCU-SAD-001 section 12).
 */

#include "mcal/gpio_cfg.h"

#include "ls_cfg.h"

#if LS_CFG_LOCK_DIAG_OPTION_B == 1
#define GPIO_CFG_LOCK_DIAG_PORT GPIO_PORT_B
#define GPIO_CFG_LOCK_DIAG_PIN  (4u)
#else
#define GPIO_CFG_LOCK_DIAG_PORT GPIO_PORT_A
#define GPIO_CFG_LOCK_DIAG_PIN  (6u)
#endif

/*
 * Bridge PWM pins start as low push-pull outputs; Pwm_Init() switches them to the timer
 * after the compare registers are 0. Unused remapped timer channels keep CCxE = 0.
 */
const Gpio_PinCfgType GpioCfg_Pins[GPIO_CFG_PIN_COUNT] = {
    {GPIO_PORT_C, 7u, GPIO_MODE_OUT_PP_2MHZ, LS_LOW},  /* WIN_PWM */
    {GPIO_PORT_A, 10u, GPIO_MODE_OUT_PP_2MHZ, LS_LOW}, /* WIN_INA */
    {GPIO_PORT_B, 5u, GPIO_MODE_OUT_PP_2MHZ, LS_LOW},  /* WIN_INB */
    {GPIO_PORT_B, 10u, GPIO_MODE_IN_FLOATING, LS_LOW}, /* WIN_DIAG */
    {GPIO_PORT_B, 6u, GPIO_MODE_OUT_PP_2MHZ, LS_LOW},  /* LOCK_PWM */
    {GPIO_PORT_A, 8u, GPIO_MODE_OUT_PP_2MHZ, LS_LOW},  /* LOCK_INA */
    {GPIO_PORT_A, 9u, GPIO_MODE_OUT_PP_2MHZ, LS_LOW},  /* LOCK_INB */
    {GPIO_CFG_LOCK_DIAG_PORT, GPIO_CFG_LOCK_DIAG_PIN, GPIO_MODE_IN_FLOATING,
     LS_LOW},                                           /* LOCK_DIAG */
    {GPIO_PORT_C, 0u, GPIO_MODE_OUT_PP_10MHZ, LS_LOW},  /* TRACE0 */
    {GPIO_PORT_C, 1u, GPIO_MODE_OUT_PP_10MHZ, LS_LOW},  /* TRACE1 */
    {GPIO_PORT_C, 2u, GPIO_MODE_OUT_PP_10MHZ, LS_LOW},  /* TRACE2 */
    {GPIO_PORT_C, 3u, GPIO_MODE_OUT_PP_10MHZ, LS_LOW},  /* TRACE3 */
    {GPIO_PORT_A, 0u, GPIO_MODE_ANALOG, LS_LOW},        /* WIN_CS */
    {GPIO_PORT_A, 1u, GPIO_MODE_ANALOG, LS_LOW},        /* LOCK_CS */
    {GPIO_PORT_A, 4u, GPIO_MODE_ANALOG, LS_LOW},        /* KL30 */
    {GPIO_PORT_A, 15u, GPIO_MODE_IN_FLOATING, LS_LOW},  /* ENC_A */
    {GPIO_PORT_B, 3u, GPIO_MODE_IN_FLOATING, LS_LOW},   /* ENC_B */
    {GPIO_PORT_A, 11u, GPIO_MODE_IN_PULL, LS_HIGH},     /* CAN_RX */
    {GPIO_PORT_A, 12u, GPIO_MODE_AF_PP_10MHZ, LS_HIGH}, /* CAN_TX */
    {GPIO_PORT_B, 8u, GPIO_MODE_AF_OD_2MHZ, LS_HIGH},   /* I2C_SCL */
    {GPIO_PORT_B, 9u, GPIO_MODE_AF_OD_2MHZ, LS_HIGH},   /* I2C_SDA */
    {GPIO_PORT_A, 2u, GPIO_MODE_AF_PP_10MHZ, LS_HIGH},  /* VCP_TX */
    {GPIO_PORT_A, 3u, GPIO_MODE_IN_PULL, LS_HIGH},      /* VCP_RX */
    {GPIO_PORT_B, 12u, GPIO_MODE_IN_PULL, LS_HIGH},     /* LOCK_SW */
    {GPIO_PORT_A, 5u, GPIO_MODE_OUT_PP_2MHZ, LS_LOW},   /* LD2 */
    {GPIO_PORT_C, 13u, GPIO_MODE_IN_FLOATING, LS_LOW},  /* B1 */
};

/* Order: bridge inputs first (both channels), then the fault inputs, then the trace pins. */
const Gpio_PinIdType GpioCfg_SafePins[GPIO_CFG_SAFE_PIN_COUNT] = {
    GPIO_PIN_WIN_PWM,  GPIO_PIN_LOCK_PWM, GPIO_PIN_WIN_INA,  GPIO_PIN_WIN_INB,
    GPIO_PIN_LOCK_INA, GPIO_PIN_LOCK_INB, GPIO_PIN_WIN_DIAG, GPIO_PIN_LOCK_DIAG,
    GPIO_PIN_TRACE0,   GPIO_PIN_TRACE1,   GPIO_PIN_TRACE2,   GPIO_PIN_TRACE3,
};
