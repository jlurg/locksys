/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_evset.c
 * @brief Prioritised event bitset with a bounded drain.
 */

#include "ls_common/ls_evset.h"

#include <stddef.h>

/* Bit index of a 32-bit power of two: s_bitIndex[(bit * 0x077CB531) >> 27] (de Bruijn). */
static const LsEvSet_EventType s_bitIndex[32] = {
    0u,  1u,  28u, 2u,  29u, 14u, 24u, 3u, 30u, 22u, 20u, 15u, 25u, 17u, 4u,  8u,
    31u, 27u, 13u, 23u, 21u, 19u, 16u, 7u, 26u, 12u, 18u, 6u,  11u, 5u,  10u, 9u,
};

static uint32_t LsEvSet_Bit(LsEvSet_EventType event)
{
    return (uint32_t)1u << event;
}

/* Number of the lowest set bit; value must not be 0. */
static LsEvSet_EventType LsEvSet_LowestBit(uint32_t value)
{
    uint32_t lowest = value & (~value + 1u);

    return s_bitIndex[(lowest * 0x077CB531u) >> 27];
}

LsEvSet_ResultType LsEvSet_Init(LsEvSet_Type *set, uint8_t maxDrain)
{
    LsEvSet_ResultType result = LS_EVSET_E_PARAM;

    if ((set != NULL) && (maxDrain > 0u))
    {
        set->pending = 0u;
        set->maxDrain = maxDrain;
        set->drained = 0u;
        result = LS_EVSET_OK;
    }
    return result;
}

LsEvSet_ResultType LsEvSet_Post(LsEvSet_Type *set, LsEvSet_EventType event)
{
    LsEvSet_ResultType result = LS_EVSET_E_PARAM;

    if ((set != NULL) && (event < LS_EVSET_MAX_EVENTS))
    {
        set->pending |= LsEvSet_Bit(event);
        result = LS_EVSET_OK;
    }
    return result;
}

LsEvSet_ResultType LsEvSet_Cancel(LsEvSet_Type *set, LsEvSet_EventType event)
{
    LsEvSet_ResultType result = LS_EVSET_E_PARAM;

    if ((set != NULL) && (event < LS_EVSET_MAX_EVENTS))
    {
        set->pending &= ~LsEvSet_Bit(event);
        result = LS_EVSET_OK;
    }
    return result;
}

bool LsEvSet_IsPending(const LsEvSet_Type *set, LsEvSet_EventType event)
{
    bool pending = false;

    if ((set != NULL) && (event < LS_EVSET_MAX_EVENTS))
    {
        pending = (set->pending & LsEvSet_Bit(event)) != 0u;
    }
    return pending;
}

bool LsEvSet_IsEmpty(const LsEvSet_Type *set)
{
    return (set == NULL) || (set->pending == 0u);
}

void LsEvSet_Clear(LsEvSet_Type *set)
{
    if (set != NULL)
    {
        set->pending = 0u;
    }
}

void LsEvSet_BeginDrain(LsEvSet_Type *set)
{
    if (set != NULL)
    {
        set->drained = 0u;
    }
}

LsEvSet_ResultType LsEvSet_Next(LsEvSet_Type *set, LsEvSet_EventType *event)
{
    LsEvSet_ResultType result = LS_EVSET_E_PARAM;

    if ((set != NULL) && (event != NULL))
    {
        if (set->pending == 0u)
        {
            result = LS_EVSET_EMPTY;
        }
        else if (set->drained >= set->maxDrain)
        {
            result = LS_EVSET_E_LIMIT;
        }
        else
        {
            LsEvSet_EventType lowest = LsEvSet_LowestBit(set->pending);

            set->pending &= ~LsEvSet_Bit(lowest);
            set->drained++;
            *event = lowest;
            result = LS_EVSET_OK;
        }
    }
    return result;
}
