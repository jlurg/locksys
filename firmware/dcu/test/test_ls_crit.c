/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "unity.h"

#include "platform/ls_compiler.h"
#include "platform/ls_crit.h"
#include "stm32f1_fakes.h"

void setUp(void)
{
    LsFake_Reset();
}

void tearDown(void)
{
}

/* @verifies SWR-DCU-015 */
void test_LsCrit_Enter_FromUnmasked_RaisesToTaskLevelAndRestores(void)
{
    const LsCrit_StateType state = LsCrit_Enter();

    TEST_ASSERT_EQUAL_UINT32(0u, state);
    TEST_ASSERT_EQUAL_HEX32(LS_CRIT_BASEPRI_TASK, LsHost_Basepri);
    LsCrit_Exit(state);
    TEST_ASSERT_EQUAL_HEX32(0u, LsHost_Basepri);
}

/* @verifies SWR-DCU-015 */
void test_LsCrit_EnterReflex_InsideTaskSection_RaisesAndRestoresTaskLevel(void)
{
    const LsCrit_StateType outer = LsCrit_Enter();
    const LsCrit_StateType inner = LsCrit_EnterReflex();

    TEST_ASSERT_EQUAL_HEX32(LS_CRIT_BASEPRI_TASK, inner);
    TEST_ASSERT_EQUAL_HEX32(LS_CRIT_BASEPRI_REFLEX, LsHost_Basepri);
    LsCrit_Exit(inner);
    TEST_ASSERT_EQUAL_HEX32(LS_CRIT_BASEPRI_TASK, LsHost_Basepri);
    LsCrit_Exit(outer);
    TEST_ASSERT_EQUAL_HEX32(0u, LsHost_Basepri);
}

/* @verifies SWR-DCU-015 */
void test_LsCrit_Enter_InsideReflexSection_NeverLowersMasking(void)
{
    const LsCrit_StateType outer = LsCrit_EnterReflex();
    const LsCrit_StateType inner = LsCrit_Enter();

    TEST_ASSERT_EQUAL_HEX32(LS_CRIT_BASEPRI_REFLEX, inner);
    TEST_ASSERT_EQUAL_HEX32(LS_CRIT_BASEPRI_REFLEX, LsHost_Basepri);
    LsCrit_Exit(inner);
    TEST_ASSERT_EQUAL_HEX32(LS_CRIT_BASEPRI_REFLEX, LsHost_Basepri);
    LsCrit_Exit(outer);
    TEST_ASSERT_EQUAL_HEX32(0u, LsHost_Basepri);
}

void test_LsCrit_Enter_WithHigherMaskingSet_KeepsIt(void)
{
    LsHost_Basepri = 0x10u;
    TEST_ASSERT_EQUAL_HEX32(0x10u, LsCrit_Enter());
    TEST_ASSERT_EQUAL_HEX32(0x10u, LsHost_Basepri);
}

void test_LsCrit_Enter_WithLowerMaskingSet_Raises(void)
{
    LsHost_Basepri = 0x50u;
    TEST_ASSERT_EQUAL_HEX32(0x50u, LsCrit_Enter());
    TEST_ASSERT_EQUAL_HEX32(LS_CRIT_BASEPRI_TASK, LsHost_Basepri);
}
