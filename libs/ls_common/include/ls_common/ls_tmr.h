/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_tmr.h
 * @brief Timestamp timer pool that posts expired timers into an event set.
 *
 * Each slot is bound to one event at initialisation. A started timer stores its start
 * timestamp; LsTmr_Poll() posts the bound event once the elapsed time reaches the
 * duration ("elapsed >= duration"), so a timer never expires early, whatever the polling
 * jitter. Timestamps are free-running 32-bit millisecond counters; differences are taken
 * modulo 2^32, so durations up to LS_TMR_MAX_DURATION_MS are exact across the wrap.
 * Not interrupt safe: start, stop and poll from the same task context.
 */

#ifndef LS_TMR_H
#define LS_TMR_H

#include <stdbool.h>
#include <stdint.h>

#include "ls_common/ls_evset.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Longest supported timer duration in ms (2^31 - 1). */
#define LS_TMR_MAX_DURATION_MS (0x7FFFFFFFu)

    /** @brief Result of the timer pool operations. */
    typedef uint8_t LsTmr_ResultType;
/** @brief Operation done. */
#define LS_TMR_OK ((LsTmr_ResultType)0u)
/** @brief Invalid argument or unknown event; nothing changed. */
#define LS_TMR_E_PARAM ((LsTmr_ResultType)1u)

    /** @brief One timer slot. Owned by the pool; do not access the fields directly. */
    typedef struct
    {
        uint32_t startMs;          /**< timestamp of the last start */
        uint32_t durationMs;       /**< duration of the running timer */
        LsEvSet_EventType eventId; /**< event posted on expiry */
        bool active;               /**< timer running */
    } LsTmr_SlotType;

    /** @brief Timer pool over caller-provided slots. */
    typedef struct
    {
        LsTmr_SlotType *slots; /**< slot storage, numSlots entries */
        uint8_t numSlots;      /**< number of slots */
    } LsTmr_PoolType;

    /**
 * @brief Initialise a pool and bind slot i to event eventIds[i]; every timer is stopped.
 *
 * @param pool     Timer pool.
 * @param slots    Slot storage of @p numSlots entries, owned by the caller for the pool lifetime.
 * @param eventIds Event of each slot; distinct values below LS_EVSET_MAX_EVENTS.
 * @param numSlots Number of slots (1 .. LS_EVSET_MAX_EVENTS).
 * @return LS_TMR_OK, or LS_TMR_E_PARAM (NULL argument, bad count, invalid or repeated event);
 *         the pool is unusable (no slots) after a failed initialisation.
 */
    LsTmr_ResultType LsTmr_Init(LsTmr_PoolType *pool, LsTmr_SlotType *slots,
                                const LsEvSet_EventType *eventIds, uint8_t numSlots);

    /**
 * @brief Start or restart the timer bound to an event.
 *
 * @param pool       Timer pool.
 * @param eventId    Event of the timer.
 * @param nowMs      Current timestamp in ms.
 * @param durationMs Duration in ms (0 .. LS_TMR_MAX_DURATION_MS; 0 expires at the next poll).
 * @return LS_TMR_OK, or LS_TMR_E_PARAM for an unknown event, a NULL pool or a too long duration.
 */
    LsTmr_ResultType LsTmr_Start(LsTmr_PoolType *pool, LsEvSet_EventType eventId, uint32_t nowMs,
                                 uint32_t durationMs);

    /**
 * @brief Stop the timer bound to an event (no effect when it is not running).
 *
 * @param pool    Timer pool.
 * @param eventId Event of the timer.
 * @return LS_TMR_OK, or LS_TMR_E_PARAM for an unknown event or a NULL pool.
 */
    LsTmr_ResultType LsTmr_Stop(LsTmr_PoolType *pool, LsEvSet_EventType eventId);

    /**
 * @brief Test whether the timer bound to an event is running.
 *
 * @param pool    Timer pool.
 * @param eventId Event of the timer.
 * @return true when running; false when stopped, expired, unknown or @p pool is NULL.
 */
    bool LsTmr_IsActive(const LsTmr_PoolType *pool, LsEvSet_EventType eventId);

    /**
 * @brief Time elapsed between two timestamps, modulo 2^32.
 *
 * @param nowMs   Current timestamp in ms.
 * @param sinceMs Earlier timestamp in ms.
 * @return nowMs - sinceMs modulo 2^32.
 */
    uint32_t LsTmr_Elapsed(uint32_t nowMs, uint32_t sinceMs);

    /**
 * @brief Expire every running timer whose duration has elapsed and post its event.
 *
 * @param pool   Timer pool.
 * @param nowMs  Current timestamp in ms.
 * @param events Event set that receives the expiry events.
 * @return Number of timers that expired; 0 when an argument is NULL.
 */
    uint8_t LsTmr_Poll(LsTmr_PoolType *pool, uint32_t nowMs, LsEvSet_Type *events);

#ifdef __cplusplus
}
#endif

#endif /* LS_TMR_H */
