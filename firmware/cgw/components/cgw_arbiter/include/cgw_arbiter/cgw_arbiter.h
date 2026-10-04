/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_arbiter.h
 * @brief Command arbiter: window press and door transaction state machines (LS-SAIC-001
 *        sections 5.1, 5.2, 6.5, 7.5 and 8.7).
 *
 * The arbiter is a pure function of (state, event, time). It runs in the core task, decides
 * admission, keep-alive supervision, STOP latches and door completion, and reports the
 * acknowledgements, results, notices and CAN requests the core has to send. It never drives an
 * actuator: the DCU re-checks every start condition.
 */

#ifndef CGW_ARBITER_H
#define CGW_ARBITER_H

#include <stdbool.h>
#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Window press state. */
    typedef enum
    {
        CGW_ARB_WIN_IDLE = 0, /**< no press */
        CGW_ARB_WIN_MOVING,   /**< press admitted, motion intent active */
        CGW_ARB_WIN_LATCHED   /**< press latched to STOP; only a newer press_id can move again */
    } cgw_arb_win_state_t;

    /** @brief Door transaction state. */
    typedef enum
    {
        CGW_ARB_DOOR_IDLE = 0, /**< no transaction */
        CGW_ARB_DOOR_PENDING   /**< CGW_DoorCmd sent, waiting for DCU_DoorSts */
    } cgw_arb_door_state_t;

    /** @brief Reason of a CGW STOP latch (LS-SAIC-001 section 6.5, second table). */
    typedef enum
    {
        CGW_LATCH_KA_TIMEOUT = 0, /**< keep-alive age above t_cgw_ka_to_ms */
        CGW_LATCH_LINK_QUALITY,   /**< RTT gate or missing Pong */
        CGW_LATCH_DIR_CHANGE,     /**< direction change within the press */
        CGW_LATCH_COMM,           /**< bus-off or DCU lost */
        CGW_LATCH_MODE,           /**< CGW or DCU mode change */
        CGW_LATCH_BACKSTOP,       /**< t_cgw_max_run_backstop_ms reached */
        CGW_LATCH_SESSION         /**< session loss, pre-emption or station disconnect */
    } cgw_latch_reason_t;

    /** @brief System view used by the admission checks; filled by the core for each event. */
    typedef struct
    {
        uint8_t cgw_mode;       /**< Ls_NodeModeType of the CGW */
        uint8_t dcu_mode;       /**< Ls_NodeModeType of the DCU (UNKNOWN when stale) */
        uint8_t lock_state;     /**< Ls_DoorLockStateType, reported with a door timeout */
        bool pairing_busy;      /**< pairing window open or factory reset in progress */
        bool dcu_alive;         /**< DCU_NodeSts fresh */
        bool win_inhibit;       /**< DcuSts_WinInhibit (true when stale) */
        bool lock_inhibit;      /**< DcuSts_LockInhibit (true when stale) */
        bool version_ok;        /**< CAN matrix major versions equal */
        bool bus_off;           /**< CAN controller bus-off */
        bool win_sts_valid;     /**< DCU_WinSts VALID and fresh */
        bool door_sts_valid;    /**< DCU_DoorSts VALID and fresh */
        bool door_rate_limited; /**< DoorSts_RateLimited */
        bool link_ok;           /**< RTT gate of the controller session passes */
    } cgw_arb_env_t;

    /** @brief WindowMove of the controller session. */
    typedef struct
    {
        uint32_t press_id; /**< APP press identifier */
        uint32_t hold_ms;  /**< time since press start reported by the APP */
        uint32_t t_rx_ms;  /**< receipt time at WebSocket handler entry */
        uint8_t dir;       /**< WindowDirection value as received */
    } cgw_arb_move_t;

    /** @brief DoorCommand of the controller session. */
    typedef struct
    {
        uint32_t request_id; /**< APP request identifier */
        uint32_t t_rx_ms;    /**< receipt time */
        uint8_t action;      /**< DoorAction value as received */
    } cgw_arb_door_cmd_t;

    /** @brief Door feedback of a valid DCU_DoorSts. */
    typedef struct
    {
        uint32_t t_rx_ms;    /**< receipt time */
        uint8_t last_req_id; /**< DoorSts_LastReqId */
        uint8_t last_result; /**< DoorSts_LastResult */
        uint8_t lock_state;  /**< DoorSts_LockState */
    } cgw_arb_door_sts_t;

    /** @brief One CommandAck to send. */
    typedef struct
    {
        uint32_t ref_id; /**< press_id or request_id */
        uint8_t result;  /**< Ls_CommandResultType */
        bool valid;      /**< an acknowledgement must be sent */
    } cgw_arb_ack_t;

    /** @brief Actions requested by one arbiter call. Cleared by every call. */
    typedef struct
    {
        cgw_arb_ack_t win_ack;    /**< CommandAck(WINDOW) */
        cgw_arb_ack_t door_ack;   /**< CommandAck(DOOR) */
        uint32_t door_request_id; /**< DoorCommandResult: request_id */
        uint8_t door_result;      /**< DoorCommandResult: result */
        uint8_t door_lock_state;  /**< DoorCommandResult: lock_state */
        bool door_result_valid;   /**< a DoorCommandResult must be sent */
        uint8_t door_can_req_id;  /**< CGW_DoorCmd ReqId */
        uint8_t door_can_request; /**< CGW_DoorCmd Req (Ls_DoorRequestType) */
        bool door_can_valid;      /**< a CGW_DoorCmd transaction must be started */
        uint16_t notice;          /**< non-DTC notice code, 0 for none */
        bool intent_changed;      /**< the window intent changed or was refreshed */
        bool stop_decided;        /**< a STOP was decided (TRACE1) */
    } cgw_arb_out_t;

    /** @brief Arbiter state. Initialise with cgw_arbiter_init(). */
    typedef struct
    {
        struct
        {
            uint32_t press_id;      /**< APP press_id of the current press */
            uint32_t last_press_id; /**< highest press_id seen in the session */
            uint32_t t_ka_ms;       /**< receipt time of the last keep-alive */
            uint32_t t_start_ms;    /**< receipt time of the first WindowMove */
            cgw_arb_win_state_t state;
            uint8_t dir;          /**< Ls_WindowRequestType UP or DOWN */
            uint8_t can_press_id; /**< CAN PressId of the press, 0 before the first press */
        } win;
        struct
        {
            uint8_t last; /**< last allocated CAN PressId */
            uint8_t echo; /**< last WinSts_PressIdEcho */
            bool seeded;  /**< allocator seeded */
        } pid;
        struct
        {
            uint32_t request_id; /**< APP request_id of the transaction */
            uint32_t session;    /**< session generation that started it */
            uint32_t t_sent_ms;  /**< start of the CAN transaction */
            cgw_arb_door_state_t state;
            uint8_t can_req_id;  /**< CAN ReqId */
            uint8_t last_req_id; /**< last allocated CAN ReqId */
            bool seeded;         /**< ReqId allocator seeded */
        } door;
        struct
        {
            uint32_t request_id; /**< request_id of the cached result */
            uint32_t t_done_ms;  /**< completion time */
            uint8_t result;      /**< Ls_CommandResultType */
            uint8_t lock_state;  /**< Ls_DoorLockStateType */
            bool valid;          /**< cache entry present */
        } cache;
        uint32_t session;           /**< generation of the controller session */
        uint32_t last_door_request; /**< StatusUpdate last_door_request_id */
        uint8_t last_door_result;   /**< StatusUpdate last_door_result */
    } cgw_arbiter_t;

    /**
 * @brief Initialise: no press, door IDLE, allocators unseeded.
 *
 * @param[out] a Arbiter.
 */
    void cgw_arbiter_init(cgw_arbiter_t *a);

    /**
 * @brief Start the scope of a newly authenticated controller session: press_id monotonicity, door
 *        request cache and the per-session door status restart.
 *
 * A PENDING door transaction of an earlier session stays PENDING (and blocks new requests) but
 * its result is not sent to the new session.
 *
 * @param[in,out] a Arbiter.
 */
    void cgw_arbiter_session_start(cgw_arbiter_t *a);

    /**
 * @brief Handle a WindowMove of the controller.
 *
 * @param[in,out] a   Arbiter.
 * @param[in]     mv  WindowMove.
 * @param[in]     env System view.
 * @param[out]    out Actions.
 */
    void cgw_arbiter_on_move(cgw_arbiter_t *a, const cgw_arb_move_t *mv, const cgw_arb_env_t *env,
                             cgw_arb_out_t *out);

    /**
 * @brief Handle a WindowStop of the controller: the current press ends by release.
 *
 * @param[in,out] a        Arbiter.
 * @param[in]     press_id press_id of the WindowStop.
 * @param[out]    out      Actions.
 */
    void cgw_arbiter_on_stop(cgw_arbiter_t *a, uint32_t press_id, cgw_arb_out_t *out);

    /**
 * @brief Handle a DoorCommand of the controller.
 *
 * @param[in,out] a   Arbiter.
 * @param[in]     cmd DoorCommand.
 * @param[in]     env System view.
 * @param[out]    out Actions.
 */
    void cgw_arbiter_on_door(cgw_arbiter_t *a, const cgw_arb_door_cmd_t *cmd,
                             const cgw_arb_env_t *env, cgw_arb_out_t *out);

    /**
 * @brief Handle the DCU decision feedback of a valid DCU_WinSts (LS-SAIC-001 section 7.5).
 *
 * @param[in,out] a          Arbiter.
 * @param[in]     echo       WinSts_PressIdEcho.
 * @param[in]     win_result WinSts_WinResult.
 * @param[out]    out        Actions.
 */
    void cgw_arbiter_on_win_sts(cgw_arbiter_t *a, uint8_t echo, uint8_t win_result,
                                cgw_arb_out_t *out);

    /**
 * @brief Handle the door feedback of a valid DCU_DoorSts.
 *
 * @param[in,out] a   Arbiter.
 * @param[in]     sts Door feedback.
 * @param[out]    out Actions.
 */
    void cgw_arbiter_on_door_sts(cgw_arbiter_t *a, const cgw_arb_door_sts_t *sts,
                                 cgw_arb_out_t *out);

    /**
 * @brief Latch STOP for the active press (no effect without an active press).
 *
 * @param[in,out] a      Arbiter.
 * @param[in]     reason Latch reason; selects the second CommandAck and the notice.
 * @param[out]    out    Actions.
 */
    void cgw_arbiter_latch(cgw_arbiter_t *a, cgw_latch_reason_t reason, cgw_arb_out_t *out);

    /**
 * @brief Periodic supervision: keep-alive age, backstop, RTT gate, door result timeout.
 *
 * @param[in,out] a      Arbiter.
 * @param[in]     now_ms Current time.
 * @param[in]     env    System view (link_ok and lock_state are used).
 * @param[out]    out    Actions.
 */
    void cgw_arbiter_tick(cgw_arbiter_t *a, uint32_t now_ms, const cgw_arb_env_t *env,
                          cgw_arb_out_t *out);

    /**
 * @brief Current window intent for can_io.
 *
 * @param[in]  a      Arbiter.
 * @param[out] intent Intent.
 */
    void cgw_arbiter_intent(const cgw_arbiter_t *a, cgw_win_intent_t *intent);

    /**
 * @brief Test whether a press is active (moving or latched but not released).
 *
 * @param[in] a Arbiter.
 * @return true while a press exists.
 */
    bool cgw_arbiter_press_active(const cgw_arbiter_t *a);

    /**
 * @brief Map a WindowStopReason to the CommandResult of LS-SAIC-001 section 6.5.
 *
 * @param[in] stop_reason Ls_WindowStopReasonType.
 * @return Ls_CommandResultType; UNSPECIFIED for NONE and unknown values.
 */
    uint8_t cgw_arbiter_stop_reason_result(uint8_t stop_reason);

#ifdef __cplusplus
}
#endif

#endif /* CGW_ARBITER_H */
