/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_evset.h
 * @brief Prioritised event bitset with a bounded drain.
 *
 * One bit per event type; pending events are delivered lowest event number first, so the
 * numbering of the events is their priority. Posting a pending event again has no effect.
 * A drain cycle delivers at most the configured number of events; the caller treats
 * LS_EVSET_E_LIMIT as a design error (for example a livelock between events).
 * Not interrupt safe: post and drain from the same task context.
 */

#ifndef LS_EVSET_H
#define LS_EVSET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Number of event types of one set (event numbers 0 .. LS_EVSET_MAX_EVENTS - 1). */
#define LS_EVSET_MAX_EVENTS (32u)

    /** @brief Event number; lower numbers have higher priority. */
    typedef uint8_t LsEvSet_EventType;

    /** @brief Result of the event set operations. */
    typedef uint8_t LsEvSet_ResultType;
/** @brief Event posted or delivered. */
#define LS_EVSET_OK ((LsEvSet_ResultType)0u)
/** @brief No event pending. */
#define LS_EVSET_EMPTY ((LsEvSet_ResultType)1u)
/** @brief Drain budget exhausted while events are still pending; they stay pending. */
#define LS_EVSET_E_LIMIT ((LsEvSet_ResultType)2u)
/** @brief Invalid argument; nothing changed. */
#define LS_EVSET_E_PARAM ((LsEvSet_ResultType)3u)

    /** @brief Event set. Initialise with LsEvSet_Init(); do not access the fields directly. */
    typedef struct
    {
        uint32_t pending; /**< bit n set: event n pending */
        uint8_t maxDrain; /**< events delivered per drain cycle at most */
        uint8_t drained;  /**< events delivered in the current drain cycle */
    } LsEvSet_Type;

    /**
 * @brief Initialise an empty event set.
 *
 * @param set      Event set.
 * @param maxDrain Events delivered per drain cycle at most (1 .. 255).
 * @return LS_EVSET_OK, or LS_EVSET_E_PARAM for a NULL set or a zero budget.
 */
    LsEvSet_ResultType LsEvSet_Init(LsEvSet_Type *set, uint8_t maxDrain);

    /**
 * @brief Mark an event as pending.
 *
 * @param set   Event set.
 * @param event Event number (< LS_EVSET_MAX_EVENTS).
 * @return LS_EVSET_OK, or LS_EVSET_E_PARAM for a NULL set or an invalid event number.
 */
    LsEvSet_ResultType LsEvSet_Post(LsEvSet_Type *set, LsEvSet_EventType event);

    /**
 * @brief Withdraw a pending event.
 *
 * @param set   Event set.
 * @param event Event number (< LS_EVSET_MAX_EVENTS).
 * @return LS_EVSET_OK, or LS_EVSET_E_PARAM for a NULL set or an invalid event number.
 */
    LsEvSet_ResultType LsEvSet_Cancel(LsEvSet_Type *set, LsEvSet_EventType event);

    /**
 * @brief Test whether an event is pending.
 *
 * @param set   Event set.
 * @param event Event number.
 * @return true when the event is pending; false for a NULL set or an invalid event number.
 */
    bool LsEvSet_IsPending(const LsEvSet_Type *set, LsEvSet_EventType event);

    /**
 * @brief Test whether no event is pending.
 *
 * @param set Event set.
 * @return true when no event is pending or @p set is NULL.
 */
    bool LsEvSet_IsEmpty(const LsEvSet_Type *set);

    /**
 * @brief Withdraw every pending event.
 *
 * @param set Event set; NULL is ignored.
 */
    void LsEvSet_Clear(LsEvSet_Type *set);

    /**
 * @brief Start a drain cycle: reset the number of delivered events to zero.
 *
 * @param set Event set; NULL is ignored.
 */
    void LsEvSet_BeginDrain(LsEvSet_Type *set);

    /**
 * @brief Deliver the pending event with the lowest number and clear it.
 *
 * @param set   Event set.
 * @param event Receives the delivered event number (written only on LS_EVSET_OK).
 * @return LS_EVSET_OK, LS_EVSET_EMPTY, LS_EVSET_E_LIMIT when the drain budget of the cycle is
 *         spent and events remain pending, or LS_EVSET_E_PARAM for NULL arguments.
 */
    LsEvSet_ResultType LsEvSet_Next(LsEvSet_Type *set, LsEvSet_EventType *event);

#ifdef __cplusplus
}
#endif

#endif /* LS_EVSET_H */
