/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_vstate.h
 * @brief Vehicle-state cache: last valid DCU frames with freshness, StatusUpdate builder
 *        (LS-SAIC-001 section 8.8), status push policy (section 5.5) and CAN matrix version check
 *        (section 7.5).
 *
 * Only frames that cgw_com delivered (E2E OK and receiver VALID) enter the cache. Freshness is
 * cleared by the RX timeouts that cgw_com reports.
 */

#ifndef CGW_VSTATE_H
#define CGW_VSTATE_H

#include <stdbool.h>
#include <stdint.h>

#include "cgw_ports/cgw_types.h"
#include "locksys_app.pb.h"
#include "locksys_cgw.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Cached DCU messages (same order as cgw_com_rx_msg_t). */
    typedef enum
    {
        CGW_VS_WIN_STS = 0, /**< DCU_WinSts */
        CGW_VS_WIN_MOTION,  /**< DCU_WinMotion */
        CGW_VS_DOOR_STS,    /**< DCU_DoorSts */
        CGW_VS_TEMP_STS,    /**< DCU_TempSts */
        CGW_VS_NODE_STS,    /**< DCU_NodeSts */
        CGW_VS_VERSION,     /**< DCU_Version */
        CGW_VS_MSG_COUNT    /**< number of cached messages */
    } cgw_vs_msg_t;

    /** @brief Vehicle-state cache. Initialise with cgw_vstate_init(). */
    typedef struct
    {
        struct locksys_cgw_dcu_win_sts_t win;     /**< last valid DCU_WinSts */
        struct locksys_cgw_dcu_win_motion_t mot;  /**< last valid DCU_WinMotion */
        struct locksys_cgw_dcu_door_sts_t door;   /**< last valid DCU_DoorSts */
        struct locksys_cgw_dcu_temp_sts_t temp;   /**< last valid DCU_TempSts */
        struct locksys_cgw_dcu_node_sts_t node;   /**< last valid DCU_NodeSts */
        struct locksys_cgw_dcu_version_t version; /**< last DCU_Version */
        uint32_t t_rx_ms[CGW_VS_MSG_COUNT];       /**< receipt time of the cached frame */
        bool fresh[CGW_VS_MSG_COUNT];             /**< received and not timed out */
        bool seen[CGW_VS_MSG_COUNT];              /**< received at least once */
        uint8_t ver_bad;                          /**< consecutive NodeSts with another major */
        uint8_t ver_good;                         /**< consecutive NodeSts with the same major */
        bool version_fault;                       /**< n_ver_debounce mismatches (U1B03) */
    } cgw_vstate_t;

    /** @brief CGW-side inputs of a StatusUpdate. */
    typedef struct
    {
        uint32_t seq;                  /**< push counter of the session */
        uint32_t last_door_request_id; /**< door transaction of the session */
        uint8_t last_door_result;      /**< Ls_CommandResultType */
        uint8_t cgw_mode;              /**< Ls_NodeModeType */
        uint8_t cgw_dtc_count;         /**< confirmed CGW DTCs */
        bool pairing_active;           /**< pairing window open */
    } cgw_vs_status_ctx_t;

    /** @brief Push scheduler of one session. */
    typedef struct
    {
        locksys_app_v1_StatusUpdate last; /**< last StatusUpdate sent */
        uint32_t t_last_ms;               /**< time of the last push */
        bool sent_any;                    /**< at least one push in this session */
        bool requested;                   /**< StatusRequest received */
    } cgw_vs_push_t;

    /**
 * @brief Initialise the cache: nothing received, version unknown.
 *
 * @param[out] vs Cache.
 */
    void cgw_vstate_init(cgw_vstate_t *vs);

    /**
 * @brief Store a delivered DCU frame.
 *
 * @param[in,out] vs     Cache.
 * @param[in]     msg    Message.
 * @param[in]     data   Frame bytes (8).
 * @param[in]     now_ms Receipt time.
 * @retval CGW_OK      Stored.
 * @retval CGW_E_PROTO Unpack failed.
 * @retval CGW_E_ARG   Invalid argument.
 */
    cgw_rc_t cgw_vstate_on_rx(cgw_vstate_t *vs, cgw_vs_msg_t msg, const uint8_t *data,
                              uint32_t now_ms);

    /**
 * @brief Mark a message stale after its RX timeout.
 *
 * @param[in,out] vs  Cache.
 * @param[in]     msg Message.
 */
    void cgw_vstate_on_timeout(cgw_vstate_t *vs, cgw_vs_msg_t msg);

    /**
 * @brief Test whether a message is fresh (received and not timed out).
 *
 * @param[in] vs  Cache.
 * @param[in] msg Message.
 * @return true when fresh.
 */
    bool cgw_vstate_is_fresh(const cgw_vstate_t *vs, cgw_vs_msg_t msg);

    /**
 * @brief Test whether the CAN matrix versions are compatible.
 *
 * @param[in] vs Cache.
 * @return true after the first DCU_NodeSts and while no version fault is active.
 */
    bool cgw_vstate_version_ok(const cgw_vstate_t *vs);

    /**
 * @brief Build a StatusUpdate with the source and stale rules of LS-SAIC-001 section 8.8.
 *
 * @param[in]  vs     Cache.
 * @param[in]  ctx    CGW-side inputs.
 * @param[in]  now_ms Send time (for status_age_ms).
 * @param[out] out    StatusUpdate.
 */
    void cgw_vstate_build_status(const cgw_vstate_t *vs, const cgw_vs_status_ctx_t *ctx,
                                 uint32_t now_ms, locksys_app_v1_StatusUpdate *out);

    /**
 * @brief Reset the push scheduler at the start of a session.
 *
 * @param[out] push Scheduler.
 */
    void cgw_vstate_push_reset(cgw_vs_push_t *push);

    /**
 * @brief Decide whether a candidate StatusUpdate is pushed now (LS-SAIC-001 section 5.5).
 *
 * @param[in] push      Scheduler.
 * @param[in] candidate StatusUpdate built for this tick.
 * @param[in] now_ms    Current time.
 * @param[in] motion    A press is active or the window is moving.
 * @return true when the candidate must be sent.
 */
    bool cgw_vstate_push_due(const cgw_vs_push_t *push,
                             const locksys_app_v1_StatusUpdate *candidate, uint32_t now_ms,
                             bool motion);

    /**
 * @brief Record a sent StatusUpdate.
 *
 * @param[in,out] push   Scheduler.
 * @param[in]     sent   StatusUpdate sent.
 * @param[in]     now_ms Send time.
 */
    void cgw_vstate_push_commit(cgw_vs_push_t *push, const locksys_app_v1_StatusUpdate *sent,
                                uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* CGW_VSTATE_H */
