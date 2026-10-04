/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_crit.c
 * @brief Nestable critical sections on BASEPRI.
 */

#include "platform/ls_crit.h"

#include "platform/ls_compiler.h"

static LsCrit_StateType LsCrit_Raise(uint32_t level)
{
    const uint32_t previous = LS_GET_BASEPRI();

    /* BASEPRI 0 masks nothing; a lower non-zero value masks more, so never lower it. */
    if ((previous == 0u) || (previous > level))
    {
        LS_SET_BASEPRI(level);
        LS_ISB();
    }
    return previous;
}

/* @satisfies SWR-DCU-015 */
LsCrit_StateType LsCrit_Enter(void)
{
    return LsCrit_Raise(LS_CRIT_BASEPRI_TASK);
}

LsCrit_StateType LsCrit_EnterReflex(void)
{
    return LsCrit_Raise(LS_CRIT_BASEPRI_REFLEX);
}

void LsCrit_Exit(LsCrit_StateType state)
{
    LS_SET_BASEPRI(state);
    LS_ISB();
}
