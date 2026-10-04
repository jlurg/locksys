/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_com/cgw_com.h"

#include <string.h>

#include "cgw_ports/cgw_trace_port.h"
#include "locksys_cgw.h"
#include "ls_can_matrix_gen.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"

#define COM_HOLD_AGE_NONE (255u)
#define COM_HOLD_AGE_STEP (10u) /* HoldAge factor in ms */
#define COM_NODE_PHASE_MS (7u)
#define COM_VER_PHASE_MS  (13u)
#define COM_RECLAIM_MS    (50u)
#define COM_WIN_SLOTS     (2u)
#define COM_TIMEOUT_NONE  (0u)

#define COM_E2E_CFG(name, mode_)             \
    {(uint16_t)LS_CAN_##name##_E2E_DATA_ID,  \
     (uint8_t)LS_CAN_##name##_DLC,           \
     (mode_),                                \
     (uint8_t)LS_CAN_##name##_E2E_MAX_DELTA, \
     (uint8_t)LS_N_E2E_OK_VALID,             \
     (uint8_t)LS_N_E2E_ERR_INVALID}

static const LsE2e_ConfigType k_cfg_win_cmd = COM_E2E_CFG(CGW_WIN_CMD, LS_E2E_MODE_CYCLIC);
static const LsE2e_ConfigType k_cfg_door_cmd = COM_E2E_CFG(CGW_DOOR_CMD, LS_E2E_MODE_EVENT);
static const LsE2e_ConfigType k_cfg_node_sts = COM_E2E_CFG(CGW_NODE_STS, LS_E2E_MODE_CYCLIC);

/* Receivers, indexed by cgw_com_rx_msg_t; DCU_Version has no E2E. */
typedef struct
{
    LsE2e_ConfigType e2e;
    uint32_t timeout_ms;
    uint16_t id;
} com_rx_def_t;

static const com_rx_def_t k_rx[CGW_COM_RX_COUNT] = {
    {COM_E2E_CFG(DCU_WIN_STS, LS_E2E_MODE_CYCLIC), LS_CAN_DCU_WIN_STS_RX_TIMEOUT_MS,
     (uint16_t)LS_CAN_DCU_WIN_STS_ID},
    {COM_E2E_CFG(DCU_WIN_MOTION, LS_E2E_MODE_CYCLIC), LS_CAN_DCU_WIN_MOTION_RX_TIMEOUT_MS,
     (uint16_t)LS_CAN_DCU_WIN_MOTION_ID},
    {COM_E2E_CFG(DCU_DOOR_STS, LS_E2E_MODE_CYCLIC), LS_CAN_DCU_DOOR_STS_RX_TIMEOUT_MS,
     (uint16_t)LS_CAN_DCU_DOOR_STS_ID},
    {COM_E2E_CFG(DCU_TEMP_STS, LS_E2E_MODE_CYCLIC), LS_CAN_DCU_TEMP_STS_RX_TIMEOUT_MS,
     (uint16_t)LS_CAN_DCU_TEMP_STS_ID},
    {COM_E2E_CFG(DCU_NODE_STS, LS_E2E_MODE_CYCLIC), LS_CAN_DCU_NODE_STS_RX_TIMEOUT_MS,
     (uint16_t)LS_CAN_DCU_NODE_STS_ID},
    {{0u, 0u, LS_E2E_MODE_NONE, 0u, 0u, 0u},
     COM_TIMEOUT_NONE,
     (uint16_t)LOCKSYS_CGW_DCU_VERSION_FRAME_ID},
};

static bool com_due(uint32_t now_ms, uint32_t due_ms)
{
    return cgw_time_reached(now_ms, due_ms);
}

static uint32_t com_min_wait(uint32_t wait, uint32_t now_ms, uint32_t due_ms)
{
    uint32_t w = com_due(now_ms, due_ms) ? 1u : (uint32_t)(due_ms - now_ms);

    return (w < wait) ? w : wait;
}

cgw_com_rx_msg_t cgw_com_rx_msg_of(uint16_t id)
{
    cgw_com_rx_msg_t msg = CGW_COM_RX_COUNT;

    for (uint32_t i = 0u; (i < (uint32_t)CGW_COM_RX_COUNT) && (msg == CGW_COM_RX_COUNT); i++)
    {
        if (k_rx[i].id == id)
        {
            msg = (cgw_com_rx_msg_t)i;
        }
    }
    return msg;
}

void cgw_com_init(cgw_com_t *com, const cgw_com_version_t *version, uint32_t now_ms)
{
    (void)memset(com, 0, sizeof(*com));
    if (version != NULL)
    {
        com->version = *version;
    }
    com->intent.dir = LS_WINDOW_REQUEST_STOP;
    com->node.mode = LS_NODE_MODE_INIT;
    com->win_next_ms = now_ms;
    com->node_next_ms = now_ms + COM_NODE_PHASE_MS;
    com->ver_next_ms = now_ms + COM_VER_PHASE_MS;
    com->bus = CGW_BUS_ACTIVE;
    (void)LsE2e_TxInit(&com->win_tx);
    (void)LsE2e_TxInit(&com->door_tx);
    (void)LsE2e_TxInit(&com->node_tx);
    for (uint32_t i = 0u; i < (uint32_t)CGW_COM_RX_COUNT; i++)
    {
        (void)LsE2e_RxInit(&com->rx[i].e2e);
        com->rx[i].t_last_ms = now_ms;
    }
}

void cgw_com_set_intent(cgw_com_t *com, const cgw_win_intent_t *intent)
{
    com->intent = *intent;
}

void cgw_com_request_door(cgw_com_t *com, uint8_t req_id, uint8_t request, uint32_t now_ms)
{
    com->door_req_id = req_id;
    com->door_request = request;
    com->door_left = (uint8_t)(LS_CAN_CGW_DOOR_CMD_REPETITIONS + 1u);
    com->door_next_ms = now_ms;
}

void cgw_com_set_node_info(cgw_com_t *com, const cgw_node_info_t *info)
{
    com->node = *info;
}

/* @satisfies SWR-CGW-041 */
void cgw_com_win_cmd_values(const cgw_com_t *com, uint32_t now_ms, uint8_t *req, uint8_t *hold)
{
    const cgw_win_intent_t *in = &com->intent;
    uint32_t age = (uint32_t)(now_ms - in->t_ka_ms);

    *req = LS_WINDOW_REQUEST_STOP;
    *hold = (uint8_t)COM_HOLD_AGE_NONE;
    if ((in->can_press_id != 0u) && !in->latched)
    {
        uint32_t steps = age / COM_HOLD_AGE_STEP;

        *hold = (uint8_t)((steps < COM_HOLD_AGE_NONE) ? steps : COM_HOLD_AGE_NONE);
        if (in->active && (age <= LS_T_CGW_KA_TO_MS) &&
            ((in->dir == LS_WINDOW_REQUEST_UP) || (in->dir == LS_WINDOW_REQUEST_DOWN)))
        {
            *req = in->dir;
        }
    }
}

static void com_pack_win_cmd(uint8_t press_id, uint8_t req, uint8_t hold,
                             uint8_t frame[CGW_CAN_DLC_MAX])
{
    struct locksys_cgw_cgw_win_cmd_t msg;

    (void)memset(&msg, 0, sizeof(msg));
    msg.win_cmd_req = req;
    msg.win_cmd_press_id = press_id;
    msg.win_cmd_hold_age = hold;
    (void)memset(frame, 0, CGW_CAN_DLC_MAX);
    (void)locksys_cgw_cgw_win_cmd_pack(frame, &msg, LOCKSYS_CGW_CGW_WIN_CMD_LENGTH);
}

/* Protect with the next alive counter and enqueue; the counter advances only on success. */
/* @satisfies SWR-CGW-042 */
static bool com_send_e2e(const LsE2e_ConfigType *cfg, LsE2e_TxStateType *tx, uint16_t id,
                         uint8_t frame[CGW_CAN_DLC_MAX])
{
    bool sent = false;

    if (LsE2e_ProtectWithCounter(cfg, tx->counter, frame, cfg->dlc) == LS_E2E_E_OK)
    {
        sent = cgw_can_tx(id, frame, cfg->dlc) == CGW_OK;
        if (sent)
        {
            tx->counter = (uint8_t)((tx->counter + 1u) & LS_E2E_COUNTER_MASK);
        }
    }
    return sent;
}

static void com_tick_win_cmd(cgw_com_t *com, uint32_t now_ms)
{
    uint8_t req = LS_WINDOW_REQUEST_STOP;
    uint8_t hold = (uint8_t)COM_HOLD_AGE_NONE;
    bool change;

    cgw_com_win_cmd_values(com, now_ms, &req, &hold);
    change = com->win_sent_any && (req != com->win_last_req) &&
             ((uint32_t)(now_ms - com->win_last_ms) >= LS_CAN_CGW_WIN_CMD_MIN_GAP_MS);
    if (change || com_due(now_ms, com->win_next_ms))
    {
        uint8_t frame[CGW_CAN_DLC_MAX];

        com_pack_win_cmd(com->intent.can_press_id, req, hold, frame);
        if (com_send_e2e(&k_cfg_win_cmd, &com->win_tx, (uint16_t)LS_CAN_CGW_WIN_CMD_ID, frame))
        {
            com->win_last_ms = now_ms;
            com->win_last_req = req;
            com->win_sent_any = true;
            cgw_trace_set(CGW_TRACE_WINCMD_TX, true);
            cgw_trace_set(CGW_TRACE_WINCMD_TX, false);
            if (req == LS_WINDOW_REQUEST_STOP)
            {
                cgw_trace_set(CGW_TRACE_STOP_PENDING, false);
            }
        }
        else
        {
            com->stats.tx_skipped++;
        }
        com->win_next_ms = now_ms + LS_CAN_CGW_WIN_CMD_CYCLE_MS;
    }
}

static void com_tick_door_cmd(cgw_com_t *com, uint32_t now_ms)
{
    if ((com->door_left > 0u) && com_due(now_ms, com->door_next_ms))
    {
        struct locksys_cgw_cgw_door_cmd_t msg;
        uint8_t frame[CGW_CAN_DLC_MAX] = {0u};

        (void)memset(&msg, 0, sizeof(msg));
        msg.door_cmd_req = com->door_request;
        msg.door_cmd_req_id = com->door_req_id;
        (void)locksys_cgw_cgw_door_cmd_pack(frame, &msg, LOCKSYS_CGW_CGW_DOOR_CMD_LENGTH);
        if (!com_send_e2e(&k_cfg_door_cmd, &com->door_tx, (uint16_t)LS_CAN_CGW_DOOR_CMD_ID, frame))
        {
            com->stats.tx_skipped++;
        }
        com->door_left--;
        com->door_next_ms = now_ms + LS_CAN_CGW_DOOR_CMD_MIN_GAP_MS;
    }
}

/* @satisfies SWR-CGW-047 */
static void com_tick_node_sts(cgw_com_t *com, uint32_t now_ms)
{
    if (com_due(now_ms, com->node_next_ms))
    {
        struct locksys_cgw_cgw_node_sts_t msg;
        uint8_t frame[CGW_CAN_DLC_MAX] = {0u};

        (void)memset(&msg, 0, sizeof(msg));
        msg.cgw_sts_mode = com->node.mode;
        msg.cgw_sts_com_ver_major = (uint8_t)LS_CAN_MATRIX_VERSION_MAJOR;
        msg.cgw_sts_com_ver_minor = (uint8_t)LS_CAN_MATRIX_VERSION_MINOR;
        msg.cgw_sts_app_link = com->node.app_link;
        msg.cgw_sts_wifi_clients = com->node.wifi_clients;
        msg.cgw_sts_reset_reason = com->node.reset_reason;
        msg.cgw_sts_dtc_count = com->node.dtc_count;
        msg.cgw_sts_heap_free_pct = com->node.heap_free_pct;
        (void)locksys_cgw_cgw_node_sts_pack(frame, &msg, LOCKSYS_CGW_CGW_NODE_STS_LENGTH);
        if (!com_send_e2e(&k_cfg_node_sts, &com->node_tx, (uint16_t)LS_CAN_CGW_NODE_STS_ID, frame))
        {
            com->stats.tx_skipped++;
        }
        com->node_next_ms = now_ms + LS_CAN_CGW_NODE_STS_CYCLE_MS;
    }
}

static void com_tick_version(cgw_com_t *com, uint32_t now_ms)
{
    if (com_due(now_ms, com->ver_next_ms))
    {
        struct locksys_cgw_cgw_version_t msg;
        uint8_t frame[CGW_CAN_DLC_MAX] = {0u};

        (void)memset(&msg, 0, sizeof(msg));
        msg.cgw_ver_sw_major = com->version.sw_major;
        msg.cgw_ver_sw_minor = com->version.sw_minor;
        msg.cgw_ver_sw_patch = com->version.sw_patch;
        msg.cgw_ver_com_ver_major = (uint8_t)LS_CAN_MATRIX_VERSION_MAJOR;
        msg.cgw_ver_com_ver_minor = (uint8_t)LS_CAN_MATRIX_VERSION_MINOR;
        msg.cgw_ver_git_hash = com->version.git_hash;
        msg.cgw_ver_dirty = com->version.dirty ? 1u : 0u;
        msg.cgw_ver_build_type = com->version.build_type;
        (void)locksys_cgw_cgw_version_pack(frame, &msg, LOCKSYS_CGW_CGW_VERSION_LENGTH);
        if (cgw_can_tx((uint16_t)LS_CAN_CGW_VERSION_ID, frame,
                       (uint8_t)LOCKSYS_CGW_CGW_VERSION_LENGTH) != CGW_OK)
        {
            com->stats.tx_skipped++;
        }
        com->ver_next_ms = now_ms + LS_CAN_CGW_VERSION_CYCLE_MS;
    }
}

/* @satisfies SWR-CGW-044 */
static uint8_t com_tick_timeouts(cgw_com_t *com, uint32_t now_ms)
{
    uint8_t timeouts = 0u;

    for (uint32_t i = 0u; i < (uint32_t)CGW_COM_RX_COUNT; i++)
    {
        cgw_com_rx_t *rx = &com->rx[i];

        if ((k_rx[i].timeout_ms != COM_TIMEOUT_NONE) && !rx->timed_out &&
            ((uint32_t)(now_ms - rx->t_last_ms) > k_rx[i].timeout_ms))
        {
            LsE2e_RxTimeout(&rx->e2e);
            rx->timed_out = true;
            timeouts = (uint8_t)(timeouts | (1u << i));
        }
    }
    return timeouts;
}

static void com_tick_bus(cgw_com_t *com, uint32_t now_ms, cgw_com_tick_out_t *out)
{
    if ((com->bus == CGW_BUS_OFF) && com_due(now_ms, com->bus_next_ms))
    {
        (void)cgw_can_recover();
        if (com->bus_attempts < UINT8_MAX)
        {
            com->bus_attempts++;
        }
        com->bus_next_ms = now_ms + ((com->bus_attempts < LS_N_BUSOFF_FAST) ? LS_T_BUSOFF_FAST_MS
                                                                            : LS_T_BUSOFF_SLOW_MS);
    }
    if (com->reclaim_pending && com_due(now_ms, com->reclaim_ms))
    {
        com->reclaim_pending = false;
        com->stats.reclaimed += cgw_can_reclaim_suspect();
    }
    if (com->healing && (com->bus != CGW_BUS_OFF) &&
        ((uint32_t)(now_ms - com->bus_active_ms) >= LS_T_BUSOFF_HEAL_MS))
    {
        com->healing = false;
        com->bus_attempts = 0u;
        out->busoff_healed = true;
    }
}

uint32_t cgw_com_tick(cgw_com_t *com, uint32_t now_ms, cgw_com_tick_out_t *out)
{
    uint32_t wait = LS_CAN_CGW_WIN_CMD_CYCLE_MS;

    (void)memset(out, 0, sizeof(*out));
    if ((com->bus != CGW_BUS_OFF) && (com->bus != CGW_BUS_STOPPED))
    {
        com_tick_win_cmd(com, now_ms);
        com_tick_door_cmd(com, now_ms);
        com_tick_node_sts(com, now_ms);
        com_tick_version(com, now_ms);
        wait = com_min_wait(wait, now_ms, com->win_next_ms);
        wait = com_min_wait(wait, now_ms, com->node_next_ms);
        wait = com_min_wait(wait, now_ms, com->ver_next_ms);
        if (com->door_left > 0u)
        {
            wait = com_min_wait(wait, now_ms, com->door_next_ms);
        }
    }
    out->timeouts = com_tick_timeouts(com, now_ms);
    com_tick_bus(com, now_ms, out);
    if (com->bus == CGW_BUS_OFF)
    {
        wait = com_min_wait(wait, now_ms, com->bus_next_ms);
    }
    return wait;
}

cgw_com_rx_result_t cgw_com_on_rx(cgw_com_t *com, uint16_t id, const uint8_t *data, uint8_t dlc,
                                  uint32_t now_ms)
{
    cgw_com_rx_result_t res = {CGW_COM_RX_COUNT, LS_E2E_STATUS_BAD_ARG, false};

    res.msg = cgw_com_rx_msg_of(id);
    if (res.msg == CGW_COM_RX_COUNT)
    {
        com->stats.unknown_id++;
    }
    else if (k_rx[res.msg].e2e.mode == LS_E2E_MODE_NONE)
    {
        /* DCU_Version: cached only. */
        res.status = LS_E2E_STATUS_OK;
        res.deliver = (data != NULL) && (dlc == (uint8_t)LOCKSYS_CGW_DCU_VERSION_LENGTH);
    }
    else
    {
        cgw_com_rx_t *rx = &com->rx[res.msg];

        res.status = LsE2e_Check(&k_rx[res.msg].e2e, &rx->e2e, data, dlc);
        if (res.status == LS_E2E_STATUS_OK)
        {
            rx->t_last_ms = now_ms;
            rx->timed_out = false;
        }
        res.deliver = LsE2e_IsDataValid(&rx->e2e);
    }
    return res;
}

/* @satisfies SWR-CGW-046 */
static void com_rewrite_stop(cgw_com_t *com)
{
    uint8_t frames[COM_WIN_SLOTS][CGW_CAN_DLC_MAX];
    uint8_t n;

    for (uint8_t i = 0u; i < COM_WIN_SLOTS; i++)
    {
        com_pack_win_cmd(com->intent.can_press_id, LS_WINDOW_REQUEST_STOP,
                         (uint8_t)COM_HOLD_AGE_NONE, frames[i]);
        (void)LsE2e_ProtectWithCounter(&k_cfg_win_cmd, (uint8_t)(com->win_tx.counter + i),
                                       frames[i], k_cfg_win_cmd.dlc);
    }
    n = cgw_can_rewrite_busy((uint16_t)LS_CAN_CGW_WIN_CMD_ID,
                             (const uint8_t (*)[CGW_CAN_DLC_MAX])frames, COM_WIN_SLOTS,
                             k_cfg_win_cmd.dlc);
    com->win_tx.counter = (uint8_t)((com->win_tx.counter + n) & LS_E2E_COUNTER_MASK);
    com->win_last_req = LS_WINDOW_REQUEST_STOP;
}

void cgw_com_on_bus_state(cgw_com_t *com, cgw_bus_state_t state, uint32_t now_ms)
{
    if ((state == CGW_BUS_OFF) && (com->bus != CGW_BUS_OFF))
    {
        com_rewrite_stop(com);
        com->stats.busoff++;
        com->healing = true;
        com->reclaim_pending = false;
        com->bus_next_ms = now_ms + ((com->bus_attempts < LS_N_BUSOFF_FAST) ? LS_T_BUSOFF_FAST_MS
                                                                            : LS_T_BUSOFF_SLOW_MS);
    }
    else if ((state == CGW_BUS_ACTIVE) && (com->bus == CGW_BUS_OFF))
    {
        com->bus_active_ms = now_ms;
        com->reclaim_pending = true;
        com->reclaim_ms = now_ms + COM_RECLAIM_MS;
        com->win_next_ms = now_ms;
    }
    else
    {
        /* Warning and passive states only change the reported state. */
    }
    com->bus = state;
}
