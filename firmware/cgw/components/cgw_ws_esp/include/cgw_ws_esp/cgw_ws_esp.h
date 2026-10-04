/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_ws_esp.h
 * @brief WebSocket adapter (LS-SAIC-001 section 8.2): HTTP server with the single endpoint
 *        /ws/v1, exact subprotocol check, binary frames up to 256 bytes, connection table with
 *        the single unauthenticated slot, and the TX slot pool drained by one httpd work item.
 *
 * Implements cgw_link_port.h. Received frames and connection events are handed to the
 * composition root through a sink callback that runs in the httpd task.
 */

#ifndef CGW_WS_ESP_H
#define CGW_WS_ESP_H

#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Connection events reported to the sink. */
    typedef enum
    {
        CGW_WS_EV_TCP_OPEN = 0, /**< TCP accepted */
        CGW_WS_EV_WS_OPEN,      /**< WebSocket upgrade with subprotocol locksys.v1 */
        CGW_WS_EV_CLOSE,        /**< connection closed */
        CGW_WS_EV_FRAME         /**< binary message received */
    } cgw_ws_ev_t;

    /**
 * @brief Event sink; runs in the httpd task and must only copy and post.
 *
 * @param[in] ev   Event.
 * @param[in] conn Connection.
 * @param[in] t_ms Handler entry time (receipt time of a frame).
 * @param[in] data Message bytes for CGW_WS_EV_FRAME, else NULL.
 * @param[in] len  Message length.
 * @return CGW_OK when posted; CGW_E_FULL is counted as a drop.
 */
    typedef cgw_rc_t (*cgw_ws_sink_t)(cgw_ws_ev_t ev, cgw_conn_t conn, uint32_t t_ms,
                                      const uint8_t *data, uint16_t len);

    /** @brief Server configuration. */
    typedef struct
    {
        cgw_ws_sink_t sink;  /**< event sink */
        uint32_t stack_size; /**< httpd task stack in bytes */
        uint16_t port;       /**< TCP port (80) */
        uint8_t priority;    /**< httpd task priority */
        uint8_t core;        /**< httpd task CPU */
    } cgw_ws_cfg_t;

    /** @brief Counters of the adapter. */
    typedef struct
    {
        uint32_t refused;     /**< connections refused by the unauthenticated-slot gate */
        uint32_t rx_dropped;  /**< frames not posted (queue full) */
        uint32_t tx_full;     /**< sends refused (no slot or ring entry) */
        uint32_t drain_retry; /**< drain work items that could not be queued */
        uint32_t tx_errors;   /**< asynchronous send errors */
    } cgw_ws_stats_t;

    /**
 * @brief Start the HTTP server and register /ws/v1.
 *
 * @param[in] cfg Configuration.
 * @retval CGW_OK   Server running.
 * @retval CGW_E_IO Server could not be started.
 */
    cgw_rc_t cgw_ws_start(const cgw_ws_cfg_t *cfg);

    /**
 * @brief Copy the adapter counters.
 *
 * @param[out] out Counters.
 */
    void cgw_ws_get_stats(cgw_ws_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CGW_WS_ESP_H */
