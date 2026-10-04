/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_core.h
 * @brief Core active object of the CGW: event catalogue, run-to-completion dispatch and the
 *        10 ms time event that drive the session, pairing, arbiter, vehicle-state and health
 *        modules (LS-CGW-SAD-001 sections 4 and 5).
 *
 * Portable: outputs leave through the link, can_io, console, trace, key store and system ports.
 * The core task owns one cgw_core_t and is the only caller.
 */

#ifndef CGW_CORE_H
#define CGW_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include "cgw_arbiter/cgw_arbiter.h"
#include "cgw_health/cgw_health.h"
#include "cgw_pairing/cgw_pairing.h"
#include "cgw_ports/cgw_keystore_port.h"
#include "cgw_ports/cgw_types.h"
#include "cgw_session/cgw_session.h"
#include "cgw_vstate/cgw_vstate.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Core event types (LS-CGW-SAD-001 section 4.1). */
    typedef enum
    {
        CGW_EV_NONE = 0,   /**< no event */
        CGW_EV_TICK,       /**< 10 ms time event; aux = free internal heap in percent */
        CGW_EV_TCP_OPEN,   /**< TCP accepted on conn */
        CGW_EV_WS_OPEN,    /**< WebSocket upgrade of conn completed */
        CGW_EV_WS_CLOSE,   /**< conn closed */
        CGW_EV_WS_FRAME,   /**< binary message of conn in data[0 .. len-1]; t_ms = handler entry */
        CGW_EV_AP_STARTED, /**< WIFI_EVENT_AP_START */
        CGW_EV_AP_FAILED,  /**< SoftAP did not start after the retries */
        CGW_EV_STA_JOIN,   /**< station associated; aux = station count */
        CGW_EV_STA_LEAVE, /**< station left; arg = its IPv4 (network order, 0 = unknown); aux = count */
        CGW_EV_CAN_RX,        /**< delivered DCU frame; arg = cgw_vs_msg_t; data[0 .. 7] */
        CGW_EV_RX_TIMEOUT,    /**< RX timeout; arg = cgw_vs_msg_t */
        CGW_EV_CAN_BUS,       /**< controller state; arg = cgw_bus_state_t */
        CGW_EV_BUSOFF_HEALED, /**< t_busoff_heal_ms without bus-off */
        CGW_EV_E2E_ERROR,     /**< E2E error; arg = LsE2e_CheckStatusType */
        CGW_EV_PAIR_REQ,      /**< BOOT held for t_pair_btn_hold_ms */
        CGW_EV_FACTORY_RESET, /**< BOOT held for t_factory_reset_hold_ms */
        CGW_EV_DTC            /**< test result from an adapter; arg = cgw_dtc_t, aux = 1 failed */
    } cgw_core_ev_type_t;

    /** @brief One core event (queue item). */
    typedef struct
    {
        uint32_t t_ms;                  /**< event time; WS_FRAME: WebSocket handler entry time */
        cgw_conn_t conn;                /**< connection of WebSocket events */
        uint32_t arg;                   /**< type-specific argument */
        uint16_t len;                   /**< WS_FRAME payload length */
        uint8_t type;                   /**< cgw_core_ev_type_t */
        uint8_t aux;                    /**< type-specific small argument */
        uint8_t data[CGW_WS_FRAME_MAX]; /**< payload */
    } cgw_core_event_t;

    /** @brief Identity and start-up information of the CGW. */
    typedef struct
    {
        uint8_t device_id[CGW_DEVICE_ID_LEN];     /**< first 8 bytes of SHA-256 over the base MAC */
        uint8_t ap_mac[CGW_MAC_LEN];              /**< SoftAP MAC (BSSID) */
        char fw_version[24];                      /**< application version string */
        char ssid[CGW_SSID_SIZE];                 /**< SoftAP SSID */
        char passphrase[CGW_PASSPHRASE_LEN + 1u]; /**< SoftAP passphrase */
        cgw_reset_src_t reset_src;                /**< reset source of this start */
        uint8_t wdt_resets;   /**< watchdog and panic resets within the window */
        bool wifi_transition; /**< DEV transition mode active */
    } cgw_core_identity_t;

    /** @brief Counters of the core. */
    typedef struct
    {
        uint32_t unknown_body;  /**< unknown Body members ignored */
        uint32_t violations;    /**< protocol violations (close 1008) */
        uint32_t send_failures; /**< frames not queued (TX pool full) */
    } cgw_core_stats_t;

    /** @brief Core state. Initialise with cgw_core_init(). */
    typedef struct
    {
        cgw_session_table_t ses;  /**< sessions */
        cgw_arbiter_t arb;        /**< command arbiter */
        cgw_vstate_t vs;          /**< vehicle state */
        cgw_vs_push_t push;       /**< status push of the controller session */
        cgw_health_t health;      /**< modes and DTCs */
        cgw_pairing_t pair;       /**< pairing window */
        cgw_pairing_record_t rec; /**< committed pairing */
        cgw_core_identity_t id;   /**< identity */
        cgw_core_stats_t stats;   /**< counters */
        cgw_win_intent_t intent;  /**< last intent posted to can_io */
        cgw_node_info_t node;     /**< last node information posted to can_io */
        uint32_t status_seq;      /**< StatusUpdate seq of the controller session */
        uint8_t reset_reason;     /**< Ls_ResetReasonType of this start */
        uint8_t dcu_mode_prev;    /**< DCU mode at the last evaluation */
        uint8_t wifi_clients;     /**< associated stations */
        uint8_t heap_free_pct;    /**< free internal heap in percent */
        uint8_t bus;              /**< cgw_bus_state_t */
        uint8_t indication;       /**< cgw_indication_t shown */
        bool paired;              /**< rec holds a committed pairing */
        bool ap_started;          /**< SoftAP running */
        bool can_enabled;         /**< TWAI node enabled */
        bool intent_dirty;        /**< intent not yet accepted by can_io */
        bool node_dirty;          /**< node information not yet accepted by can_io */
        bool factory_reset;       /**< factory reset in progress */
    } cgw_core_t;

    /**
 * @brief Initialise the core: all modules reset, CGW mode INIT, intent STOP.
 *
 * @param[out] core   Core state.
 * @param[in]  id     Identity.
 * @param[in]  rec    Committed pairing, or NULL when the CGW is unpaired; copied.
 * @param[in]  now_ms Current time.
 */
    void cgw_core_init(cgw_core_t *core, const cgw_core_identity_t *id,
                       const cgw_pairing_record_t *rec, uint32_t now_ms);

    /**
 * @brief Dispatch one event to completion.
 *
 * @param[in,out] core Core state.
 * @param[in]     ev   Event.
 */
    void cgw_core_dispatch(cgw_core_t *core, const cgw_core_event_t *ev);

    /**
 * @brief Current CGW mode.
 *
 * @param[in] core Core state.
 * @return Ls_NodeModeType.
 */
    uint8_t cgw_core_mode(const cgw_core_t *core);

#ifdef __cplusplus
}
#endif

#endif /* CGW_CORE_H */
