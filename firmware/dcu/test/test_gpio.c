/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "unity.h"

#include "mcal/gpio/gpio.h"
#include "mcal/gpio_cfg.h"
#include "stm32f1_fakes.h"

#define CR_FIELD(reg, pin) (((reg) >> (((pin) % 8u) * 4u)) & 0xFu)

void setUp(void)
{
    LsFake_Reset();
}

void tearDown(void)
{
}

/* @verifies SWR-DCU-070 */
void test_Gpio_InitSafe_BridgeInputsLowOutputsAndDiagFloating(void)
{
    /* Reset value of every CRx field: floating input (0x4). */
    LsFake_GPIOA.CRH = 0x44444444u;
    LsFake_GPIOB.CRL = 0x44444444u;
    LsFake_GPIOB.CRH = 0x44444444u;
    LsFake_GPIOC.CRL = 0x44444444u;

    Gpio_InitSafe();

    TEST_ASSERT_BITS(RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN |
                         RCC_APB2ENR_IOPCEN,
                     0xFFFFFFFFu, LsFake_RCC.APB2ENR);
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUT_PP_2MHZ, CR_FIELD(LsFake_GPIOC.CRL, 7u));  /* PC7 */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUT_PP_2MHZ, CR_FIELD(LsFake_GPIOA.CRH, 10u)); /* PA10 */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUT_PP_2MHZ, CR_FIELD(LsFake_GPIOB.CRL, 5u));  /* PB5 */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUT_PP_2MHZ, CR_FIELD(LsFake_GPIOB.CRL, 6u));  /* PB6 */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUT_PP_2MHZ, CR_FIELD(LsFake_GPIOA.CRH, 8u));  /* PA8 */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUT_PP_2MHZ, CR_FIELD(LsFake_GPIOA.CRH, 9u));  /* PA9 */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_IN_FLOATING, CR_FIELD(LsFake_GPIOB.CRH, 10u)); /* PB10 */
    TEST_ASSERT_EQUAL_HEX32(GPIO_MODE_OUT_PP_10MHZ, CR_FIELD(LsFake_GPIOC.CRL, 0u)); /* PC0 */
    /* MAPR is not written before Gpio_Init(). */
    TEST_ASSERT_EQUAL_HEX32(0u, LsFake_AFIO.MAPR);
}

/* @verifies SWR-DCU-070 */
void test_Gpio_Init_WritesMaprOnceWithTheRemapValue(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_E_OK, Gpio_Init());
    TEST_ASSERT_EQUAL_HEX32(0x02000D02u, LsFake_AFIO.MAPR);

    LsFake_AFIO.MAPR = 0u;
    TEST_ASSERT_EQUAL_UINT8(LS_E_NOT_OK, Gpio_Init());
    TEST_ASSERT_EQUAL_HEX32(0u, LsFake_AFIO.MAPR);
}

void test_Gpio_Write_SingleBsrrStore(void)
{
    Gpio_Write(GPIO_PIN_LD2, LS_HIGH);
    TEST_ASSERT_EQUAL_HEX32(1UL << 5u, LsFake_GPIOA.BSRR);
    Gpio_Write(GPIO_PIN_LD2, LS_LOW);
    TEST_ASSERT_EQUAL_HEX32(1UL << (5u + 16u), LsFake_GPIOA.BSRR);
}

void test_Gpio_ReadAndReadOutput_ReflectIdrAndOdr(void)
{
    LsFake_GPIOB.IDR = 1UL << 12u;
    LsFake_GPIOA.ODR = 1UL << 5u;
    TEST_ASSERT_EQUAL_UINT8(LS_HIGH, Gpio_Read(GPIO_PIN_LOCK_SW));
    TEST_ASSERT_EQUAL_UINT8(LS_LOW, Gpio_Read(GPIO_PIN_WIN_DIAG));
    TEST_ASSERT_EQUAL_UINT8(LS_HIGH, Gpio_ReadOutput(GPIO_PIN_LD2));
    TEST_ASSERT_EQUAL_UINT8(LS_LOW, Gpio_ReadOutput(GPIO_PIN_WIN_INA));
}

void test_Gpio_InvalidPin_IsIgnored(void)
{
    Gpio_Write((Gpio_PinIdType)GPIO_CFG_PIN_COUNT, LS_HIGH);
    Gpio_SetMode((Gpio_PinIdType)GPIO_CFG_PIN_COUNT, GPIO_MODE_AF_PP_10MHZ);
    TEST_ASSERT_EQUAL_UINT8(LS_LOW, Gpio_Read((Gpio_PinIdType)GPIO_CFG_PIN_COUNT));
    TEST_ASSERT_EQUAL_UINT8(LS_LOW, Gpio_ReadOutput((Gpio_PinIdType)GPIO_CFG_PIN_COUNT));
    TEST_ASSERT_EQUAL_HEX32(0u, LsFake_GPIOA.BSRR);
}

void test_Gpio_SetMode_ChangesOnlyThatField(void)
{
    LsFake_GPIOC.CRL = 0x44444444u;
    Gpio_SetMode(GPIO_PIN_WIN_PWM, GPIO_MODE_AF_PP_10MHZ);
    TEST_ASSERT_EQUAL_HEX32(0x94444444u, LsFake_GPIOC.CRL);
}

/* @verifies SWR-DCU-103 */
void test_Gpio_TraceSet_OneBsrrStoreOnGpioc(void)
{
    Gpio_TraceSet(0x01u, 0x08u);
    TEST_ASSERT_EQUAL_HEX32(0x00080001u, LsFake_GPIOC.BSRR);
    Gpio_TraceSet(0xF0u, 0xF0u);
    TEST_ASSERT_EQUAL_HEX32(0u, LsFake_GPIOC.BSRR);
}
