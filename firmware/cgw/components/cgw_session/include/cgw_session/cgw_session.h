/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_session.h
 * @brief APP session table: handshake, proofs, session key, frame tags, counters, rate limit,
 *        single-controller arbitration, authentication throttling and close decisions.
 *
 * Implements LS-SAIC-001 sections 8.2 to 8.4. All functions run in the core task. Times are
 * milliseconds of the clock port; the receive time of a frame is the WebSocket handler entry
 * time (t_rx_ms), never the processing time.
 */

#ifndef CGW_SESSION_H
#define CGW_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cgw_ports/cgw_crypto_port.h"
#include "cgw_ports/cgw_types.h"
#include "cgw_proto/cgw_proto.h"
#include "ls_params_gen.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief State of a session table entry. */
    typedef enum
    {
        CGW_SES_FREE = 0,   /**< entry unused */
        CGW_SES_TCP,        /**< TCP accepted, WebSocket upgrade pending */
        CGW_SES_OPEN,       /**< WebSocket open, ServerHello not sent */
        CGW_SES_HELLO_SENT, /**< ServerHello sent, ClientAuth pending */
        CGW_SES_AUTH,       /**< authenticated: the controller session */
        CGW_SES_CLOSING     /**< close requested, waiting for the close event */
    } cgw_ses_state_t;

    /** @brief Receipt times of the last accepted APP frames (rolling rate-limit window). */
    typedef struct
    {
        uint32_t t_ms[LS_N_RATE_LIMIT_FRAMES]; /**< ring of receipt times */
        uint8_t next;                          /**< next write index (oldest entry when full) */
        uint8_t count;                         /**< valid entries */
    } cgw_rate_window_t;

    /** @brief CGW-originated Ping and the last RTT sample of a session. */
    typedef struct
    {
        uint32_t t_sent_ms;   /**< send time (and timestamp) of the last Ping */
        uint32_t t_sample_ms; /**< receipt time of the Pong of the last RTT sample */
        uint32_t rtt_ms;      /**< last RTT sample */
        bool sent_any;        /**< at least one Ping was sent in this session */
        bool pending;         /**< the last Ping is not answered yet */
        bool sampled;         /**< at least one RTT sample exists */
        bool missed;          /**< a Pong was missing for t_pong_to_ms since the last sample */
    } cgw_ses_ping_t;

    /** @brief One session table entry. */
    typedef struct
    {
        cgw_conn_t conn;                      /**< connection */
        uint32_t t_accept_ms;                 /**< TCP accept time */
        uint32_t t_last_rx_ms;                /**< receipt time of the last valid frame */
        uint32_t rx_counter;                  /**< last accepted APP -> CGW counter */
        uint32_t tx_counter;                  /**< last used CGW -> APP counter */
        uint32_t session_id;                  /**< random session identifier */
        cgw_key_t k_sess;                     /**< session key handle */
        cgw_rate_window_t rate;               /**< rate-limit window */
        cgw_ses_ping_t ping;                  /**< Ping and RTT state */
        uint8_t server_nonce[CGW_NONCE_LEN];  /**< nonce of the ServerHello */
        uint8_t client_id[CGW_CLIENT_ID_LEN]; /**< client identifier after authentication */
        cgw_ses_state_t state;                /**< entry state */
        bool pending_key;                     /**< authenticated with the pending pairing key */
    } cgw_session_t;

    /** @brief Security event counters. */
    typedef struct
    {
        uint32_t
            auth_failures;    /**< failed proofs, handshake timeouts, pre-authentication garbage */
        uint32_t replays;     /**< zero or non-increasing counters */
        uint32_t bad_tags;    /**< tag verification failures */
        uint32_t rate_closes; /**< rate-limit closes */
        uint32_t refused;     /**< connections refused because the table was full */
        uint32_t throttled;   /**< authentication attempts answered with REJECTED_RATE_LIMIT */
    } cgw_session_stats_t;

    /** @brief Session table. Initialise with cgw_session_init(). */
    typedef struct
    {
        cgw_session_t entries[CGW_SESSIONS_MAX]; /**< entries */
        cgw_session_stats_t stats;               /**< security event counters */
        uint32_t t_last_failure_ms;              /**< time of the last authentication failure */
        uint8_t failures;                        /**< failures counted for throttling */
    } cgw_session_table_t;

    /** @brief Session decision for a received frame. */
    typedef enum
    {
        CGW_SES_DROP = 0,  /**< ignore the frame */
        CGW_SES_DELIVER,   /**< authenticated frame accepted; process the body */
        CGW_SES_HANDSHAKE, /**< handshake frame (counter 0, no tag); expect ClientAuth */
        CGW_SES_CLOSE      /**< close the connection with close_code */
    } cgw_ses_action_t;

    /** @brief Session decision with its close code. */
    typedef struct
    {
        cgw_ses_action_t action; /**< decision */
        uint16_t close_code;     /**< WebSocket close code for CGW_SES_CLOSE */
        bool controller_lost;    /**< the controller session ends: latch STOP */
    } cgw_ses_verdict_t;

    /** @brief Close request produced by cgw_session_tick(). */
    typedef struct
    {
        cgw_conn_t conn;         /**< connection */
        uint16_t close_code;     /**< close code; CGW_CLOSE_NONE closes the TCP connection only */
        bool controller_lost;    /**< the controller session ends: latch STOP */
        bool send_session_close; /**< send SessionClose(reason) before closing */
    } cgw_ses_close_t;

    /** @brief Close requests of one tick. */
    typedef struct
    {
        cgw_ses_close_t closes[CGW_SESSIONS_MAX]; /**< close requests */
        uint8_t count;                            /**< number of requests */
    } cgw_ses_tick_result_t;

    /** @brief Identity data for the ServerHello. */
    typedef struct
    {
        const uint8_t *device_id; /**< CGW_DEVICE_ID_LEN bytes */
        const char *fw_version;   /**< firmware version string */
        bool pairing_window_open; /**< pairing window state */
    } cgw_ses_identity_t;

    /** @brief Keys and identity used to verify a ClientAuth. */
    typedef struct
    {
        const uint8_t *device_id;     /**< CGW_DEVICE_ID_LEN bytes */
        const uint8_t *pending_kpair; /**< pending pairing key (CGW_KEY_LEN) or NULL */
        const uint8_t *current_kpair; /**< committed pairing key (CGW_KEY_LEN) or NULL */
    } cgw_ses_auth_input_t;

    /** @brief Outcome of a ClientAuth verification. */
    typedef struct
    {
        uint32_t session_id;   /**< session identifier for AuthResult(OK) */
        cgw_conn_t preempted;  /**< controller closed with 4004 because of this authentication */
        uint16_t close_code;   /**< close code after a failed AuthResult; 0 on success */
        uint8_t result;        /**< Ls_CommandResultType; UNSPECIFIED: close without AuthResult */
        bool used_pending_key; /**< authenticated with the pending pairing key */
        uint8_t server_proof[CGW_HMAC_LEN]; /**< server proof for AuthResult(OK) */
    } cgw_ses_auth_t;

    /**
 * @brief Initialise the session table: all entries free, counters zero.
 *
 * @param[out] t Session table.
 */
    void cgw_session_init(cgw_session_table_t *t);

    /**
 * @brief Register an accepted TCP connection; starts the handshake timer.
 *
 * @param[in,out] t           Session table.
 * @param[in]     conn        Connection.
 * @param[in]     t_accept_ms TCP accept time.
 * @retval CGW_OK     Entry allocated.
 * @retval CGW_E_FULL No free entry (counted as refused).
 * @retval CGW_E_ARG  Invalid argument or connection already known.
 */
    cgw_rc_t cgw_session_on_tcp_open(cgw_session_table_t *t, cgw_conn_t conn, uint32_t t_accept_ms);

    /**
 * @brief Register the WebSocket upgrade of a connection.
 *
 * @param[in,out] t    Session table.
 * @param[in]     conn Connection.
 * @retval CGW_OK      Entry is OPEN.
 * @retval CGW_E_STATE Unknown connection or not in TCP state.
 */
    cgw_rc_t cgw_session_on_ws_open(cgw_session_table_t *t, cgw_conn_t conn);

    /**
 * @brief Release the entry of a closed connection; destroys the session key.
 *
 * @param[in,out] t    Session table.
 * @param[in]     conn Connection.
 * @return true when the closed connection was the authenticated controller (latch STOP).
 */
    bool cgw_session_on_close(cgw_session_table_t *t, cgw_conn_t conn);

    /**
 * @brief Mark a connection as closing; further frames of it are dropped.
 *
 * @param[in,out] t    Session table.
 * @param[in]     conn Connection.
 */
    void cgw_session_mark_closing(cgw_session_table_t *t, cgw_conn_t conn);

    /**
 * @brief Find the entry of a connection.
 *
 * @param[in] t    Session table.
 * @param[in] conn Connection.
 * @return Entry, or NULL when the connection is unknown or the entry is free.
 */
    const cgw_session_t *cgw_session_find(const cgw_session_table_t *t, cgw_conn_t conn);

    /**
 * @brief Return the authenticated controller connection.
 *
 * @param[in] t Session table.
 * @return Connection, or CGW_CONN_NONE.
 */
    cgw_conn_t cgw_session_controller(const cgw_session_table_t *t);

    /**
 * @brief Check a received counter against the session state.
 *
 * @param[in] s       Authenticated session.
 * @param[in] counter Received counter.
 * @return true when @p counter is not 0 and strictly greater than the last accepted counter.
 */
    bool cgw_session_counter_is_valid(const cgw_session_t *s, uint32_t counter);

    /**
 * @brief Admit a received frame to the rolling rate-limit window.
 *
 * @param[in,out] w       Window.
 * @param[in]     t_rx_ms Receipt time of the frame.
 * @return false when the frame would be the (n_rate_limit_frames + 1)-th frame within
 *         t_rate_window_ms; the window is unchanged then.
 */
    bool cgw_session_rate_admit(cgw_rate_window_t *w, uint32_t t_rx_ms);

    /**
 * @brief Apply rate limit, counter rules and tag verification to a received frame.
 *
 * Handshake frames (counter 0, no tag) of a session waiting for ClientAuth give
 * CGW_SES_HANDSHAKE. A frame of an authenticated session is delivered only when its counter is
 * valid and its tag verifies over the raw body; the counter and the last receive time are then
 * updated.
 *
 * @param[in,out] t       Session table.
 * @param[in]     conn    Connection.
 * @param[in]     frame   Decoded frame.
 * @param[in]     t_rx_ms Receipt time of the frame.
 * @return Verdict; CGW_SES_CLOSE carries 1008 for every violation.
 */
    cgw_ses_verdict_t cgw_session_on_frame(cgw_session_table_t *t, cgw_conn_t conn,
                                           const cgw_proto_frame_t *frame, uint32_t t_rx_ms);

    /**
 * @brief Handle a protocol violation found after cgw_session_on_frame(): malformed Frame or
 *        Body, wrong-direction member, invalid enumeration, out-of-sequence handshake message.
 *
 * @param[in,out] t      Session table.
 * @param[in]     conn   Connection.
 * @param[in]     now_ms Current time.
 * @return Verdict CGW_SES_CLOSE with 1008, or CGW_SES_DROP for an unknown connection.
 */
    cgw_ses_verdict_t cgw_session_on_violation(cgw_session_table_t *t, cgw_conn_t conn,
                                               uint32_t now_ms);

    /**
 * @brief Prepare the ServerHello of an open connection: new server nonce, state HELLO_SENT.
 *
 * @param[in,out] t     Session table.
 * @param[in]     conn  Connection in state OPEN.
 * @param[in]     id    Identity data.
 * @param[out]    hello ServerHello to send with counter 0 and no tag.
 * @retval CGW_OK           ServerHello prepared.
 * @retval CGW_E_STATE      Unknown connection or wrong state.
 * @retval CGW_E_NO_ENTROPY No random numbers yet.
 * @retval CGW_E_ARG        Invalid argument.
 */
    cgw_rc_t cgw_session_build_hello(cgw_session_table_t *t, cgw_conn_t conn,
                                     const cgw_ses_identity_t *id,
                                     locksys_app_v1_ServerHello *hello);

    /**
 * @brief Verify a ClientAuth and, on success, authenticate the session.
 *
 * Order: protocol version, throttling, key availability, proof (pending key first, then the
 * current key), single-controller arbitration, session key derivation, server proof.
 *
 * @param[in,out] t      Session table.
 * @param[in]     conn   Connection in state HELLO_SENT.
 * @param[in]     ca     Decoded ClientAuth.
 * @param[in]     in     Keys and device identity.
 * @param[in]     now_ms Receipt time of the ClientAuth.
 * @return Outcome. result OK: send AuthResult(OK) with counter 1 and a tag. Other results:
 *         send AuthResult(result) with counter 0 and no tag, then close with close_code.
 *         UNSPECIFIED: close with close_code without an AuthResult.
 */
    cgw_ses_auth_t cgw_session_on_client_auth(cgw_session_table_t *t, cgw_conn_t conn,
                                              const locksys_app_v1_ClientAuth *ca,
                                              const cgw_ses_auth_input_t *in, uint32_t now_ms);

    /**
 * @brief Seal an outgoing body of an authenticated session: next counter and tag.
 *
 * @param[in,out] t        Session table.
 * @param[in]     conn     Authenticated connection.
 * @param[in]     body     Encoded Body.
 * @param[in]     body_len Body length, 1 .. CGW_PROTO_BODY_MAX.
 * @param[out]    out      Frame to encode.
 * @retval CGW_OK       Frame sealed.
 * @retval CGW_E_STATE  Not authenticated, or the counter space is exhausted.
 * @retval CGW_E_CRYPTO Tag computation failed.
 * @retval CGW_E_ARG    Invalid argument.
 */
    cgw_rc_t cgw_session_seal(cgw_session_table_t *t, cgw_conn_t conn, const uint8_t *body,
                              size_t body_len, cgw_proto_frame_t *out);

    /**
 * @brief Evaluate the handshake and session timeouts.
 *
 * Handshake: no authentication within t_cgw_handshake_to_ms of TCP accept closes with 4006
 * when upgraded, otherwise the TCP connection is closed; counted as an authentication failure.
 * Session: no valid frame within t_session_to_ms closes the controller with SessionClose and
 * 4004. Closed entries become CLOSING.
 *
 * @param[in,out] t      Session table.
 * @param[in]     now_ms Current time.
 * @param[out]    out    Close requests.
 */
    void cgw_session_tick(cgw_session_table_t *t, uint32_t now_ms, cgw_ses_tick_result_t *out);

    /**
 * @brief Decide whether the CGW sends a Ping to the controller now.
 *
 * The first Ping follows AuthResult(OK) at once, then every t_ping_motion_ms while a press is
 * active and every t_ping_idle_ms otherwise; no new Ping while one is outstanding.
 *
 * @param[in,out] t      Session table.
 * @param[in]     now_ms Current time; used as the Ping timestamp.
 * @param[in]     moving A press is active.
 * @return true when a Ping with timestamp @p now_ms must be sent to the controller.
 */
    bool cgw_session_ping_due(cgw_session_table_t *t, uint32_t now_ms, bool moving);

    /**
 * @brief Record a Pong of the controller; a matching echo produces an RTT sample.
 *
 * @param[in,out] t       Session table.
 * @param[in]     conn    Connection.
 * @param[in]     echo_ms Echoed timestamp.
 * @param[in]     t_rx_ms Receipt time of the Pong.
 */
    void cgw_session_on_pong(cgw_session_table_t *t, cgw_conn_t conn, uint32_t echo_ms,
                             uint32_t t_rx_ms);

    /**
 * @brief Test the link-quality gate of the controller.
 *
 * @param[in] t      Session table.
 * @param[in] now_ms Current time.
 * @return true when the last RTT is at most t_rtt_max_ms, was sampled within
 *         t_rtt_sample_max_age_ms and no Ping is outstanding for t_pong_to_ms or longer.
 */
    bool cgw_session_link_ok(const cgw_session_table_t *t, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* CGW_SESSION_H */
