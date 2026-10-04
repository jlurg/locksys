/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_trace_port.h
 * @brief Trace port: measurement outputs TRACE0 .. TRACE3 (LS-SAIC-001 section 3.5).
 *
 * Implemented by cgw_platform_esp. Present in all build types, so release testing uses the
 * shipped image.
 */

#ifndef CGW_TRACE_PORT_H
#define CGW_TRACE_PORT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief TRACE0 event; each event is a pulse of a distinct width. */
    typedef enum
    {
        CGW_TRACE_EV_WINDOW_MOVE = 0, /**< 5 us: WindowMove accepted as keep-alive */
        CGW_TRACE_EV_WINDOW_STOP,     /**< 20 us: WindowStop received */
        CGW_TRACE_EV_DOOR_COMMAND,    /**< 50 us: DoorCommand received */
        CGW_TRACE_EV_SESSION_AUTH,    /**< 100 us: session authenticated */
        CGW_TRACE_EV_SESSION_CLOSED,  /**< 200 us: session closed */
        CGW_TRACE_EV_AP_START,        /**< 500 us: WIFI_EVENT_AP_START */
        CGW_TRACE_EV_COUNT            /**< number of events */
    } cgw_trace_event_t;

    /** @brief Level and pulse outputs TRACE1 .. TRACE3. */
    typedef enum
    {
        CGW_TRACE_STOP_PENDING =
            0,               /**< TRACE1: set when the core decides STOP, cleared at enqueue */
        CGW_TRACE_WINCMD_TX, /**< TRACE2: pulse on every CGW_WinCmd enqueue */
        CGW_TRACE_CORE_BUSY, /**< TRACE3: high while the core dispatches */
        CGW_TRACE_PIN_COUNT  /**< number of level outputs */
    } cgw_trace_pin_t;

    /**
 * @brief Emit the width-coded TRACE0 pulse of an event.
 *
 * @param[in] ev Event.
 * @note Callable from tasks; not from interrupt handlers.
 */
    void cgw_trace_event(cgw_trace_event_t ev);

    /**
 * @brief Drive a level output.
 *
 * @param[in] pin   Output.
 * @param[in] level Level.
 */
    void cgw_trace_set(cgw_trace_pin_t pin, bool level);

#ifdef __cplusplus
}
#endif

#endif /* CGW_TRACE_PORT_H */
