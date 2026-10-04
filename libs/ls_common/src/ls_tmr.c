/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_tmr.c
 * @brief Timestamp timer pool.
 */

#include "ls_common/ls_tmr.h"

#include <stddef.h>

/* Slot bound to an event, or NULL. */
static LsTmr_SlotType *LsTmr_Find(const LsTmr_PoolType *pool, LsEvSet_EventType eventId)
{
    LsTmr_SlotType *found = NULL;

    if ((pool != NULL) && (pool->slots != NULL))
    {
        for (uint8_t i = 0u; i < pool->numSlots; i++)
        {
            if (pool->slots[i].eventId == eventId)
            {
                found = &pool->slots[i];
            }
        }
    }
    return found;
}

/* True when the event list holds distinct values below LS_EVSET_MAX_EVENTS. */
static bool LsTmr_EventsValid(const LsEvSet_EventType *eventIds, uint8_t numSlots)
{
    uint32_t seen = 0u;
    bool valid = true;

    for (uint8_t i = 0u; i < numSlots; i++)
    {
        uint32_t bit = (uint32_t)1u << (eventIds[i] & 31u);

        if ((eventIds[i] >= LS_EVSET_MAX_EVENTS) || ((seen & bit) != 0u))
        {
            valid = false;
        }
        seen |= bit;
    }
    return valid;
}

LsTmr_ResultType LsTmr_Init(LsTmr_PoolType *pool, LsTmr_SlotType *slots,
                            const LsEvSet_EventType *eventIds, uint8_t numSlots)
{
    LsTmr_ResultType result = LS_TMR_E_PARAM;

    if (pool != NULL)
    {
        pool->slots = NULL;
        pool->numSlots = 0u;
        if ((slots != NULL) && (eventIds != NULL) && (numSlots > 0u) &&
            (numSlots <= LS_EVSET_MAX_EVENTS) && LsTmr_EventsValid(eventIds, numSlots))
        {
            for (uint8_t i = 0u; i < numSlots; i++)
            {
                slots[i].startMs = 0u;
                slots[i].durationMs = 0u;
                slots[i].eventId = eventIds[i];
                slots[i].active = false;
            }
            pool->slots = slots;
            pool->numSlots = numSlots;
            result = LS_TMR_OK;
        }
    }
    return result;
}

LsTmr_ResultType LsTmr_Start(LsTmr_PoolType *pool, LsEvSet_EventType eventId, uint32_t nowMs,
                             uint32_t durationMs)
{
    LsTmr_ResultType result = LS_TMR_E_PARAM;
    LsTmr_SlotType *slot = LsTmr_Find(pool, eventId);

    if ((slot != NULL) && (durationMs <= LS_TMR_MAX_DURATION_MS))
    {
        slot->startMs = nowMs;
        slot->durationMs = durationMs;
        slot->active = true;
        result = LS_TMR_OK;
    }
    return result;
}

LsTmr_ResultType LsTmr_Stop(LsTmr_PoolType *pool, LsEvSet_EventType eventId)
{
    LsTmr_ResultType result = LS_TMR_E_PARAM;
    LsTmr_SlotType *slot = LsTmr_Find(pool, eventId);

    if (slot != NULL)
    {
        slot->active = false;
        result = LS_TMR_OK;
    }
    return result;
}

bool LsTmr_IsActive(const LsTmr_PoolType *pool, LsEvSet_EventType eventId)
{
    const LsTmr_SlotType *slot = LsTmr_Find(pool, eventId);

    return (slot != NULL) && slot->active;
}

uint32_t LsTmr_Elapsed(uint32_t nowMs, uint32_t sinceMs)
{
    return nowMs - sinceMs;
}

uint8_t LsTmr_Poll(LsTmr_PoolType *pool, uint32_t nowMs, LsEvSet_Type *events)
{
    uint8_t expired = 0u;

    if ((pool != NULL) && (pool->slots != NULL) && (events != NULL))
    {
        for (uint8_t i = 0u; i < pool->numSlots; i++)
        {
            LsTmr_SlotType *slot = &pool->slots[i];

            if (slot->active && (LsTmr_Elapsed(nowMs, slot->startMs) >= slot->durationMs))
            {
                slot->active = false;
                (void)LsEvSet_Post(events, slot->eventId);
                expired++;
            }
        }
    }
    return expired;
}
