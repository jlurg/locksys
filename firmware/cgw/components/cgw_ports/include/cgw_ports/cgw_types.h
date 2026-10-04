/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_types.h
 * @brief Types and constants shared by the CGW core components, the ports and the adapters.
 */

#ifndef CGW_TYPES_H
#define CGW_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Result code of the CGW APIs. */
    typedef enum
    {
        CGW_OK = 0,       /**< Operation done. */
        CGW_E_ARG,        /**< Invalid argument; nothing changed. */
        CGW_E_STATE,      /**< Not allowed in the current state. */
        CGW_E_FULL,       /**< No free buffer, slot or queue entry. */
        CGW_E_NOT_FOUND,  /**< Object not present (for example no stored pairing). */
        CGW_E_NO_ENTROPY, /**< True random numbers are not available yet. */
        CGW_E_CRYPTO,     /**< Cryptographic operation or verification failed. */
        CGW_E_IO,         /**< Driver, flash or network error. */
        CGW_E_PROTO,      /**< Malformed or out-of-range protocol data. */
        CGW_E_UNSUPPORTED /**< Function not available in this build. */
    } cgw_rc_t;

    /**
 * @brief Connection identifier: (socket descriptor << 8) | generation.
 *
 * The generation changes on every TCP accept, so a stale identifier never matches a reused
 * socket descriptor. CGW_CONN_NONE is never a valid identifier.
 */
    typedef uint32_t cgw_conn_t;

/** @brief Invalid connection identifier. */
#define CGW_CONN_NONE ((cgw_conn_t)0u)

/** @name Protocol sizes (LS-SAIC-001 sections 8.2 and 8.3) @{ */
#define CGW_WS_FRAME_MAX   (256u) /**< largest WebSocket message, ws_frame_max_bytes */
#define CGW_SESSIONS_MAX   (3u)   /**< concurrent WebSocket connections, n_ws_sockets_max */
#define CGW_KEY_LEN        (32u)  /**< K_pair and K_sess length */
#define CGW_HMAC_LEN       (32u)  /**< HMAC-SHA256 output length */
#define CGW_TAG_LEN        (16u)  /**< truncated frame tag length */
#define CGW_NONCE_LEN      (16u)  /**< server and client nonce length */
#define CGW_CLIENT_ID_LEN  (16u)  /**< APP client identifier length */
#define CGW_DEVICE_ID_LEN  (8u)   /**< CGW device identifier length */
#define CGW_PASSPHRASE_LEN (20u)  /**< SoftAP passphrase length (base32 characters) */
/** @} */

/** @name WebSocket close codes (LS-SAIC-001 section 8.4) @{ */
#define CGW_CLOSE_NONE              (0u)    /**< close the TCP connection without a close frame */
#define CGW_CLOSE_NORMAL            (1000u) /**< normal closure */
#define CGW_CLOSE_UNSUPPORTED_DATA  (1003u) /**< text, continuation or fragmented frame */
#define CGW_CLOSE_POLICY            (1008u) /**< integrity violation or rate limit */
#define CGW_CLOSE_TOO_BIG           (1009u) /**< message larger than CGW_WS_FRAME_MAX */
#define CGW_CLOSE_INTERNAL          (1011u) /**< internal error (key derivation failure) */
#define CGW_CLOSE_VERSION           (4001u) /**< subprotocol or protocol major mismatch */
#define CGW_CLOSE_AUTH_FAILED       (4002u) /**< authentication failed */
#define CGW_CLOSE_BUSY              (4003u) /**< another controller session is alive */
#define CGW_CLOSE_SESSION_TIMEOUT   (4004u) /**< session timeout or pre-emption */
#define CGW_CLOSE_PAIRING_REQUIRED  (4005u) /**< no pairing, or the pairing was replaced */
#define CGW_CLOSE_HANDSHAKE_TIMEOUT (4006u) /**< handshake timeout or authentication throttled */
    /** @} */

    /** @brief Priority class of an outgoing WebSocket frame. */
    typedef enum
    {
        CGW_PRIO_STATUS = 0, /**< StatusUpdate push; refused when the TX pool runs low */
        CGW_PRIO_CONTROL /**< handshake, acknowledgements, results, Ping/Pong, Notice, SessionClose */
    } cgw_prio_t;

    /** @brief Window motion intent passed from the core to can_io. */
    typedef struct
    {
        uint32_t t_ka_ms;     /**< receipt time of the last keep-alive of the press */
        uint8_t dir;          /**< Ls_WindowRequestType: STOP, UP or DOWN */
        uint8_t can_press_id; /**< CAN PressId of the press; 0 when no press exists */
        bool active;          /**< a press is active */
        bool latched;         /**< the press is latched to STOP */
    } cgw_win_intent_t;

    /** @brief CGW node information transmitted in CGW_NodeSts. */
    typedef struct
    {
        uint8_t mode;          /**< Ls_NodeModeType */
        uint8_t app_link;      /**< Ls_AppLinkStateType */
        uint8_t wifi_clients;  /**< associated stations */
        uint8_t reset_reason;  /**< Ls_ResetReasonType */
        uint8_t dtc_count;     /**< confirmed CGW DTCs */
        uint8_t heap_free_pct; /**< free internal heap in percent of the total */
    } cgw_node_info_t;

    /** @brief Operator indication shown on the status LED. */
    typedef enum
    {
        CGW_IND_OFF = 0,   /**< LED off */
        CGW_IND_STARTING,  /**< INIT mode */
        CGW_IND_READY,     /**< NORMAL mode, no APP session */
        CGW_IND_CONNECTED, /**< authenticated APP session */
        CGW_IND_PAIRING,   /**< pairing window open */
        CGW_IND_DEGRADED,  /**< DEGRADED mode */
        CGW_IND_FAULT      /**< SAFE mode */
    } cgw_indication_t;

    /**
 * @brief Wrap-safe check whether @p now_ms is at or after @p deadline_ms.
 *
 * @param[in] now_ms      Current time in ms.
 * @param[in] deadline_ms Deadline in ms; must lie within 2^31 - 1 ms of @p now_ms.
 * @return true when the deadline has been reached.
 */
    static inline bool cgw_time_reached(uint32_t now_ms, uint32_t deadline_ms)
    {
        return (uint32_t)(now_ms - deadline_ms) < 0x80000000u;
    }

#ifdef __cplusplus
}
#endif

#endif /* CGW_TYPES_H */
