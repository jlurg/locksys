/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_link_port.h
 * @brief Link port: outgoing WebSocket frames and connection control towards the APP.
 *
 * Implemented by cgw_ws_esp. The core task is the only caller. Frames are copied into a static
 * TX slot and handed to the HTTP server task through a single-producer single-consumer ring;
 * the core never writes to a socket directly.
 */

#ifndef CGW_LINK_PORT_H
#define CGW_LINK_PORT_H

#include <stddef.h>
#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief Queue one binary WebSocket message.
 *
 * @param[in] conn Connection.
 * @param[in] data Encoded Frame.
 * @param[in] len  Length, 1 .. CGW_WS_FRAME_MAX.
 * @param[in] prio Priority class; CGW_PRIO_STATUS is refused when the TX pool runs low.
 * @retval CGW_OK    Message queued; it is sent unless the connection closes first.
 * @retval CGW_E_FULL No TX slot or ring entry available; nothing queued.
 * @retval CGW_E_ARG Invalid argument.
 */
    cgw_rc_t cgw_link_send(cgw_conn_t conn, const uint8_t *data, size_t len, cgw_prio_t prio);

    /**
 * @brief Close a connection after the frames already queued for it.
 *
 * @param[in] conn Connection.
 * @param[in] code WebSocket close code, or CGW_CLOSE_NONE to close the TCP connection without
 *                 a close frame (connection not upgraded).
 */
    void cgw_link_close(cgw_conn_t conn, uint16_t code);

    /**
 * @brief Mark a connection as authenticated (it no longer occupies the single
 *        unauthenticated connection slot).
 *
 * @param[in] conn Connection.
 */
    void cgw_link_set_authenticated(cgw_conn_t conn);

    /**
 * @brief Find the WebSocket connection of a peer address.
 *
 * @param[in] ipv4 Peer IPv4 address in network byte order.
 * @return Connection, or CGW_CONN_NONE when no connection of that peer exists.
 */
    cgw_conn_t cgw_link_find_by_peer(uint32_t ipv4);

    /**
 * @brief Retry a hand-over of queued frames that the HTTP server could not accept.
 *
 * @note Called by the core on every tick.
 */
    void cgw_link_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* CGW_LINK_PORT_H */
