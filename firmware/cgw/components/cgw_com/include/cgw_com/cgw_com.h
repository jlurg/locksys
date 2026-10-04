/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_com.h
 * @brief CAN communication of the CGW (can_io task logic): TX schedule, E2E protection and check,
 *        independent Req/HoldAge computation, RX timeouts and bus-off policy (LS-SAIC-001
 *        sections 7.2 to 7.6).
 *
 * Portable: frames leave through the CAN port (cgw_can_port.h); time is passed by the caller.
 * The can_io task is the only user of this module.
 */

#ifndef CGW_COM_H
#define CGW_COM_H

#include <stdbool.h>
#include <stdint.h>

#include "cgw_ports/cgw_can_port.h"
#include "cgw_ports/cgw_types.h"
#include "ls_e2e/ls_e2e.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Received DCU messages. */
    typedef enum
    {
        CGW_COM_RX_WIN_STS = 0, /**< DCU_WinSts 0x200 */
        CGW_COM_RX_WIN_MOTION,  /**< DCU_WinMotion 0x201 */
        CGW_COM_RX_DOOR_STS,    /**< DCU_DoorSts 0x210 */
        CGW_COM_RX_TEMP_STS,    /**< DCU_TempSts 0x220 */
        CGW_COM_RX_NODE_STS,    /**< DCU_NodeSts 0x510 */
        CGW_COM_RX_VERSION,     /**< DCU_Version 0x590 (no E2E, cached only) */
        CGW_COM_RX_COUNT        /**< number of received messages */
    } cgw_com_rx_msg_t;

    /** @brief Identity transmitted in CGW_Version. */
    typedef struct
    {
        uint32_t git_hash;  /**< 28-bit short commit hash */
        uint8_t sw_major;   /**< SemVer major */
        uint8_t sw_minor;   /**< SemVer minor */
        uint8_t sw_patch;   /**< SemVer patch */
        uint8_t build_type; /**< Ls_BuildTypeType */
        bool dirty;         /**< built from a modified tree */
    } cgw_com_version_t;

    /** @brief Receiver state of one DCU message. */
    typedef struct
    {
        LsE2e_RxStateType e2e; /**< E2E receiver state */
        uint32_t t_last_ms;    /**< receipt time of the last OK frame */
        bool timed_out;        /**< RX timeout reported and not cleared by a new OK frame */
    } cgw_com_rx_t;

    /** @brief Result of cgw_com_on_rx(). */
    typedef struct
    {
        cgw_com_rx_msg_t
            msg; /**< message; CGW_COM_RX_COUNT for an identifier not in the whitelist */
        LsE2e_CheckStatusType status; /**< E2E status of the frame */
        bool deliver;                 /**< frame OK and receiver VALID: pass the data to the core */
    } cgw_com_rx_result_t;

    /** @brief Events reported by cgw_com_tick(). */
    typedef struct
    {
        uint8_t timeouts;   /**< bit n set: message n (cgw_com_rx_msg_t) timed out in this tick */
        bool busoff_healed; /**< t_busoff_heal_ms without bus-off since the last recovery */
    } cgw_com_tick_out_t;

    /** @brief Counters for health reporting. */
    typedef struct
    {
        uint32_t tx_skipped; /**< cycles skipped because both slots were busy */
        uint32_t unknown_id; /**< frames outside the whitelist */
        uint32_t reclaimed;  /**< slots reclaimed after recovery */
        uint32_t busoff;     /**< bus-off entries */
    } cgw_com_stats_t;

    /** @brief can_io communication state. Initialise with cgw_com_init(). */
    typedef struct
    {
        cgw_win_intent_t intent;           /**< last intent from the core */
        cgw_node_info_t node;              /**< CGW_NodeSts content */
        cgw_com_version_t version;         /**< CGW_Version content */
        LsE2e_TxStateType win_tx;          /**< CGW_WinCmd sender state */
        LsE2e_TxStateType door_tx;         /**< CGW_DoorCmd sender state */
        LsE2e_TxStateType node_tx;         /**< CGW_NodeSts sender state */
        cgw_com_rx_t rx[CGW_COM_RX_COUNT]; /**< receivers */
        cgw_com_stats_t stats;             /**< counters */
        uint32_t win_next_ms;              /**< next cyclic CGW_WinCmd */
        uint32_t win_last_ms;              /**< last CGW_WinCmd enqueue */
        uint32_t door_next_ms;             /**< next CGW_DoorCmd repetition */
        uint32_t node_next_ms;             /**< next CGW_NodeSts */
        uint32_t ver_next_ms;              /**< next CGW_Version */
        uint32_t bus_next_ms;              /**< next recovery attempt */
        uint32_t bus_active_ms;            /**< time the bus became error-active after a bus-off */
        uint32_t reclaim_ms;               /**< reclaim time after recovery */
        cgw_bus_state_t bus;               /**< controller state */
        uint8_t win_last_req;              /**< Req of the last CGW_WinCmd */
        uint8_t door_req_id;               /**< ReqId of the door transaction */
        uint8_t door_request;              /**< Req of the door transaction */
        uint8_t door_left;                 /**< transmissions left */
        uint8_t bus_attempts;              /**< recovery attempts since the last heal */
        bool win_sent_any;                 /**< at least one CGW_WinCmd enqueued */
        bool reclaim_pending;              /**< reclaim scheduled */
        bool healing;                      /**< bus-off DTC active, heal timer running */
    } cgw_com_t;

    /**
 * @brief Initialise: intent STOP, first CGW_WinCmd due at once, receivers INIT, RX timeouts
 *        measured from @p now_ms.
 *
 * @param[out] com     State.
 * @param[in]  version CGW_Version content.
 * @param[in]  now_ms  Current time (TWAI node enable).
 */
    void cgw_com_init(cgw_com_t *com, const cgw_com_version_t *version, uint32_t now_ms);

    /**
 * @brief Take a new window intent; a Req change is sent after the minimum gap.
 *
 * @param[in,out] com    State.
 * @param[in]     intent Intent.
 */
    void cgw_com_set_intent(cgw_com_t *com, const cgw_win_intent_t *intent);

    /**
 * @brief Start a CGW_DoorCmd transaction: transmissions at 0, 20 and 40 ms.
 *
 * @param[in,out] com     State.
 * @param[in]     req_id  CAN ReqId 1 .. 255.
 * @param[in]     request Ls_DoorRequestType LOCK or UNLOCK.
 * @param[in]     now_ms  Current time.
 */
    void cgw_com_request_door(cgw_com_t *com, uint8_t req_id, uint8_t request, uint32_t now_ms);

    /**
 * @brief Update the content of CGW_NodeSts.
 *
 * @param[in,out] com  State.
 * @param[in]     info Node information.
 */
    void cgw_com_set_node_info(cgw_com_t *com, const cgw_node_info_t *info);

    /**
 * @brief Compute the CGW_WinCmd payload for the current intent at @p now_ms.
 *
 * Req is the intent direction only while the intent is active, not latched and the keep-alive
 * age is at most t_cgw_ka_to_ms; HoldAge = min(255, age / 10 ms), raw 255 without a press or
 * after a latch (LS-SAIC-001 section 7.4, SM-16).
 *
 * @param[in]  com    State.
 * @param[in]  now_ms Send time.
 * @param[out] req    Ls_WindowRequestType.
 * @param[out] hold   HoldAge raw value.
 */
    void cgw_com_win_cmd_values(const cgw_com_t *com, uint32_t now_ms, uint8_t *req, uint8_t *hold);

    /**
 * @brief Transmit due frames, supervise RX timeouts and drive the bus-off recovery.
 *
 * @param[in,out] com    State.
 * @param[in]     now_ms Current time.
 * @param[out]    out    Events of this tick.
 * @return Milliseconds until the next deadline (1 .. CGW_WinCmd cycle).
 */
    uint32_t cgw_com_tick(cgw_com_t *com, uint32_t now_ms, cgw_com_tick_out_t *out);

    /**
 * @brief Check one received frame in arrival order.
 *
 * @param[in,out] com    State.
 * @param[in]     id     Standard identifier.
 * @param[in]     data   Frame bytes.
 * @param[in]     dlc    Frame length.
 * @param[in]     now_ms Receipt time.
 * @return Check result.
 */
    cgw_com_rx_result_t cgw_com_on_rx(cgw_com_t *com, uint16_t id, const uint8_t *data, uint8_t dlc,
                                      uint32_t now_ms);

    /**
 * @brief Handle a controller state change.
 *
 * BUS_OFF: scheduling stops, the busy CGW_WinCmd slots are rewritten to STOP frames (HoldAge 255,
 * new alive counters) and recovery is scheduled. ACTIVE after BUS_OFF: scheduling resumes and the
 * slot reclaim is scheduled 50 ms later.
 *
 * @param[in,out] com    State.
 * @param[in]     state  New state.
 * @param[in]     now_ms Current time.
 */
    void cgw_com_on_bus_state(cgw_com_t *com, cgw_bus_state_t state, uint32_t now_ms);

    /**
 * @brief Map a standard identifier to a received message.
 *
 * @param[in] id Identifier.
 * @return Message, or CGW_COM_RX_COUNT when not in the whitelist.
 */
    cgw_com_rx_msg_t cgw_com_rx_msg_of(uint16_t id);

#ifdef __cplusplus
}
#endif

#endif /* CGW_COM_H */
