/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "unity.h"

#include "mock_clk.h"
#include "mock_ecum_cfg.h"
#include "mock_nvic.h"
#include "mock_safemon.h"
#include "mock_stk.h"
#include "mock_wdg.h"
#include "services/ecum/ecum.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* @verifies SWR-DCU-080 */
void test_EcuM_DecodeResetReason_PriorityOrder(void)
{
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_WATCHDOG, EcuM_DecodeResetReason(0x3Fu));
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_WINDOW_WATCHDOG, EcuM_DecodeResetReason(0x37u));
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_SOFTWARE, EcuM_DecodeResetReason(0x27u));
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_LOW_POWER, EcuM_DecodeResetReason(0x23u));
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_POWER_ON, EcuM_DecodeResetReason(0x03u));
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_PIN, EcuM_DecodeResetReason(0x01u));
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_UNKNOWN, EcuM_DecodeResetReason(0x00u));
}

/* @verifies SWR-DCU-076 */
void test_EcuM_Init_HseClock_StartsWatchdogBeforeDriversAndRefreshesBetweenSteps(void)
{
    Clk_Init_ExpectAndReturn(LS_E_OK);
    Nvic_Init_Expect();
    Clk_GetSysclkHz_ExpectAndReturn(72000000u);
    Stk_Init_ExpectAndReturn(72000000u, LS_E_OK);
    Clk_GetResetFlags_ExpectAndReturn(CLK_RESET_FLAG_POR | CLK_RESET_FLAG_PIN);
    Clk_ClearResetFlags_Expect();
    SafeMon_Init_Expect();
    Wdg_Start_ExpectAndReturn(LS_E_OK);
    EcuMCfg_InitDrivers_Expect();
    Wdg_Refresh_Expect();
    EcuMCfg_InitEcual_Expect();
    Wdg_Refresh_Expect();
    EcuMCfg_InitServices_Expect();
    Wdg_Refresh_Expect();
    EcuMCfg_InitSwcs_Expect();
    Wdg_Refresh_Expect();

    EcuM_Init();

    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_POWER_ON, EcuM_GetResetReason());
}

/* @verifies SWR-DCU-075 */
void test_EcuM_Init_ClockFallback_EntersSafe(void)
{
    Clk_Init_ExpectAndReturn(LS_E_NOT_OK);
    Nvic_Init_Expect();
    Clk_GetSysclkHz_ExpectAndReturn(64000000u);
    Stk_Init_ExpectAndReturn(64000000u, LS_E_OK);
    Clk_GetResetFlags_ExpectAndReturn(CLK_RESET_FLAG_IWDG);
    Clk_ClearResetFlags_Expect();
    SafeMon_Init_Expect();
    SafeMon_EnterSafe_Expect(SAFEMON_CAUSE_CLOCK);
    Wdg_Start_ExpectAndReturn(LS_E_OK);
    EcuMCfg_InitDrivers_Expect();
    Wdg_Refresh_Expect();
    EcuMCfg_InitEcual_Expect();
    Wdg_Refresh_Expect();
    EcuMCfg_InitServices_Expect();
    Wdg_Refresh_Expect();
    EcuMCfg_InitSwcs_Expect();
    Wdg_Refresh_Expect();

    EcuM_Init();

    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_WATCHDOG, EcuM_GetResetReason());
}

/* @verifies SWR-DCU-070 */
void test_EcuM_EarlyInit_SafeOutputsFirst(void)
{
    EcuMCfg_SafeOutputs_Expect();
    EcuM_EarlyInit();
}

void test_EcuM_RequestReset_OutputsOffThenReset(void)
{
    EcuMCfg_SafeOutputs_Expect();
    Nvic_SystemReset_Expect();
    EcuM_RequestReset();
}
