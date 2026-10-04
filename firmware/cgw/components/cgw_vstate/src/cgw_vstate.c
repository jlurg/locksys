/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_vstate/cgw_vstate.h"

#include <string.h>

#include "ls_can_matrix_gen.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"

#define VS_POS_UNKNOWN  (255u)
#define VS_VBAT_INVALID (255u)
#define VS_S16_INVALID  (-32768)

void cgw_vstate_init(cgw_vstate_t *vs)
{
    (void)memset(vs, 0, sizeof(*vs));
}

static void vs_check_version(cgw_vstate_t *vs)
{
    if (vs->node.dcu_sts_com_ver_major != (uint8_t)LS_CAN_MATRIX_VERSION_MAJOR)
    {
        vs->ver_good = 0u;
        if (vs->ver_bad < UINT8_MAX)
        {
            vs->ver_bad++;
        }
        if (vs->ver_bad >= LS_N_VER_DEBOUNCE)
        {
            vs->version_fault = true;
        }
    }
    else
    {
        vs->ver_bad = 0u;
        if (vs->ver_good < UINT8_MAX)
        {
            vs->ver_good++;
        }
        if (vs->ver_good >= LS_N_VER_DEBOUNCE)
        {
            vs->version_fault = false;
        }
    }
}

static int vs_unpack(cgw_vstate_t *vs, cgw_vs_msg_t msg, const uint8_t *data)
{
    int rc;

    switch (msg)
    {
        case CGW_VS_WIN_STS:
            rc = locksys_cgw_dcu_win_sts_unpack(&vs->win, data, LOCKSYS_CGW_DCU_WIN_STS_LENGTH);
            break;
        case CGW_VS_WIN_MOTION:
            rc = locksys_cgw_dcu_win_motion_unpack(&vs->mot, data,
                                                   LOCKSYS_CGW_DCU_WIN_MOTION_LENGTH);
            break;
        case CGW_VS_DOOR_STS:
            rc = locksys_cgw_dcu_door_sts_unpack(&vs->door, data, LOCKSYS_CGW_DCU_DOOR_STS_LENGTH);
            break;
        case CGW_VS_TEMP_STS:
            rc = locksys_cgw_dcu_temp_sts_unpack(&vs->temp, data, LOCKSYS_CGW_DCU_TEMP_STS_LENGTH);
            break;
        case CGW_VS_NODE_STS:
            rc = locksys_cgw_dcu_node_sts_unpack(&vs->node, data, LOCKSYS_CGW_DCU_NODE_STS_LENGTH);
            break;
        case CGW_VS_VERSION:
            rc = locksys_cgw_dcu_version_unpack(&vs->version, data, LOCKSYS_CGW_DCU_VERSION_LENGTH);
            break;
        default:
            rc = -1;
            break;
    }
    return rc;
}

/* @satisfies SWR-CGW-044 */
/* @satisfies SWR-CGW-048 */
cgw_rc_t cgw_vstate_on_rx(cgw_vstate_t *vs, cgw_vs_msg_t msg, const uint8_t *data, uint32_t now_ms)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((vs != NULL) && (data != NULL) && (msg < CGW_VS_MSG_COUNT))
    {
        rc = CGW_E_PROTO;
        if (vs_unpack(vs, msg, data) == 0)
        {
            vs->t_rx_ms[msg] = now_ms;
            vs->fresh[msg] = true;
            vs->seen[msg] = true;
            if (msg == CGW_VS_NODE_STS)
            {
                vs_check_version(vs);
            }
            rc = CGW_OK;
        }
    }
    return rc;
}

void cgw_vstate_on_timeout(cgw_vstate_t *vs, cgw_vs_msg_t msg)
{
    if ((vs != NULL) && (msg < CGW_VS_MSG_COUNT))
    {
        vs->fresh[msg] = false;
    }
}

bool cgw_vstate_is_fresh(const cgw_vstate_t *vs, cgw_vs_msg_t msg)
{
    return (vs != NULL) && (msg < CGW_VS_MSG_COUNT) && vs->fresh[msg];
}

bool cgw_vstate_version_ok(const cgw_vstate_t *vs)
{
    return vs->seen[CGW_VS_NODE_STS] && !vs->version_fault;
}

/* Enumeration value or the given default when the raw value is undefined. */
static uint8_t vs_enum(uint8_t raw, uint8_t count, uint8_t dflt)
{
    return (raw < count) ? raw : dflt;
}

static uint32_t vs_age(const cgw_vstate_t *vs, cgw_vs_msg_t msg, uint32_t now_ms)
{
    return vs->seen[msg] ? (uint32_t)(now_ms - vs->t_rx_ms[msg]) : now_ms;
}

static uint32_t vs_window_faults(const struct locksys_cgw_dcu_win_sts_t *w)
{
    return (uint32_t)(w->win_sts_flt_stall & 1u) | ((uint32_t)(w->win_sts_flt_over_cur & 1u) << 1) |
           ((uint32_t)(w->win_sts_flt_max_run & 1u) << 2) |
           ((uint32_t)(w->win_sts_flt_lim_plaus & 1u) << 3) |
           ((uint32_t)(w->win_sts_flt_driver & 1u) << 4) |
           ((uint32_t)(w->win_sts_flt_supply & 1u) << 5) |
           ((uint32_t)(w->win_sts_flt_comm & 1u) << 6) |
           ((uint32_t)(w->win_sts_flt_over_temp & 1u) << 7) |
           ((uint32_t)(w->win_sts_flt_dir_mismatch & 1u) << 8);
}

static void vs_status_window(const cgw_vstate_t *vs, locksys_app_v1_StatusUpdate *out)
{
    out->window_state = locksys_app_v1_WindowState_WINDOW_STATE_UNKNOWN;
    out->window_position_pct = VS_POS_UNKNOWN;
    out->window_stop_reason = locksys_app_v1_WindowStopReason_WINDOW_STOP_REASON_NONE;
    if (vs->fresh[CGW_VS_WIN_STS])
    {
        out->window_state = (locksys_app_v1_WindowState)vs_enum(
            vs->win.win_sts_state, LS_WINDOW_STATE_COUNT, LS_WINDOW_STATE_UNKNOWN);
        out->window_position_pct = vs->win.win_sts_pos_pct;
        out->window_stop_reason = (locksys_app_v1_WindowStopReason)vs_enum(
            vs->win.win_sts_stop_reason, LS_WINDOW_STOP_REASON_COUNT, LS_WINDOW_STOP_REASON_NONE);
        out->window_fault_flags = vs_window_faults(&vs->win);
    }
    out->window_encoder_status = locksys_app_v1_EncoderStatus_ENCODER_STATUS_UNKNOWN;
    if (vs->fresh[CGW_VS_WIN_MOTION])
    {
        out->window_encoder_status = (locksys_app_v1_EncoderStatus)vs_enum(
            vs->mot.win_mot_enc_sts, LS_ENCODER_STATUS_COUNT, LS_ENCODER_STATUS_UNKNOWN);
        if (vs->mot.win_mot_speed != VS_S16_INVALID)
        {
            out->window_speed_rpm_x10 = vs->mot.win_mot_speed;
        }
    }
}

static void vs_status_door_temp(const cgw_vstate_t *vs, locksys_app_v1_StatusUpdate *out)
{
    out->door_lock_state = locksys_app_v1_DoorLockState_DOOR_LOCK_STATE_UNKNOWN;
    if (vs->fresh[CGW_VS_DOOR_STS])
    {
        out->door_lock_state = (locksys_app_v1_DoorLockState)vs_enum(
            vs->door.door_sts_lock_state, LS_DOOR_LOCK_STATE_COUNT, LS_DOOR_LOCK_STATE_UNKNOWN);
        out->door_fault_flags = (uint32_t)(vs->door.door_sts_flt_actuator & 1u) |
                                ((uint32_t)(vs->door.door_sts_flt_driver & 1u) << 1) |
                                ((uint32_t)(vs->door.door_sts_rate_limited & 1u) << 2);
    }
    out->temp_status = vs->seen[CGW_VS_TEMP_STS] ? locksys_app_v1_TempStatus_TEMP_STATUS_STALE
                                                 : locksys_app_v1_TempStatus_TEMP_STATUS_UNKNOWN;
    if (vs->fresh[CGW_VS_TEMP_STS])
    {
        out->temp_status = (locksys_app_v1_TempStatus)vs_enum(
            vs->temp.temp_sts_status, LS_TEMP_STATUS_COUNT, LS_TEMP_STATUS_UNKNOWN);
        if ((vs->temp.temp_sts_status == LS_TEMP_STATUS_VALID) &&
            (vs->temp.temp_sts_value != VS_S16_INVALID))
        {
            out->temperature_cdeg = vs->temp.temp_sts_value;
        }
    }
}

static void vs_status_node(const cgw_vstate_t *vs, locksys_app_v1_StatusUpdate *out)
{
    bool fresh = vs->fresh[CGW_VS_NODE_STS];

    out->dcu_alive = fresh;
    out->dcu_mode = locksys_app_v1_NodeMode_NODE_MODE_UNKNOWN;
    out->window_inhibited = true;
    out->door_inhibited = true;
    if (fresh)
    {
        out->dcu_mode = (locksys_app_v1_NodeMode)vs_enum(vs->node.dcu_sts_mode, LS_NODE_MODE_COUNT,
                                                         LS_NODE_MODE_UNKNOWN);
        out->dcu_dtc_count = vs->node.dcu_sts_dtc_count;
        out->vbat_dv = (vs->node.dcu_sts_vbat != VS_VBAT_INVALID) ? vs->node.dcu_sts_vbat : 0u;
        out->window_inhibited = vs->node.dcu_sts_win_inhibit != 0u;
        out->door_inhibited = vs->node.dcu_sts_lock_inhibit != 0u;
    }
}

/* @satisfies SWR-CGW-050 */
/* @satisfies SWR-CGW-052 */
void cgw_vstate_build_status(const cgw_vstate_t *vs, const cgw_vs_status_ctx_t *ctx,
                             uint32_t now_ms, locksys_app_v1_StatusUpdate *out)
{
    uint32_t age = vs_age(vs, CGW_VS_WIN_STS, now_ms);
    uint32_t a2 = vs_age(vs, CGW_VS_DOOR_STS, now_ms);
    uint32_t a3 = vs_age(vs, CGW_VS_NODE_STS, now_ms);

    (void)memset(out, 0, sizeof(*out));
    vs_status_window(vs, out);
    vs_status_door_temp(vs, out);
    vs_status_node(vs, out);
    age = (a2 > age) ? a2 : age;
    out->status_age_ms = (a3 > age) ? a3 : age;
    out->seq = ctx->seq;
    out->cgw_mode =
        (locksys_app_v1_NodeMode)vs_enum(ctx->cgw_mode, LS_NODE_MODE_COUNT, LS_NODE_MODE_UNKNOWN);
    out->cgw_dtc_count = ctx->cgw_dtc_count;
    out->last_door_request_id = ctx->last_door_request_id;
    out->last_door_result = (locksys_app_v1_CommandResult)vs_enum(
        ctx->last_door_result, LS_COMMAND_RESULT_COUNT, LS_COMMAND_RESULT_UNSPECIFIED);
    out->pairing_active = ctx->pairing_active;
}

void cgw_vstate_push_reset(cgw_vs_push_t *push)
{
    (void)memset(push, 0, sizeof(*push));
}

/* Changes pushed at the next tick without the coalescing gap. */
static bool vs_urgent_change(const locksys_app_v1_StatusUpdate *a,
                             const locksys_app_v1_StatusUpdate *b)
{
    return (a->door_lock_state != b->door_lock_state) || (a->window_state != b->window_state) ||
           (a->window_stop_reason != b->window_stop_reason) ||
           (a->last_door_result != b->last_door_result) ||
           (a->last_door_request_id != b->last_door_request_id);
}

static bool vs_temp_change(const locksys_app_v1_StatusUpdate *a,
                           const locksys_app_v1_StatusUpdate *b)
{
    int32_t delta = a->temperature_cdeg - b->temperature_cdeg;

    return (a->temp_status != b->temp_status) || (delta >= LS_TEMP_PUSH_DELTA_CDEG) ||
           (delta <= -LS_TEMP_PUSH_DELTA_CDEG);
}

/* Changes coalesced with t_status_push_gap_ms; seq, status_age_ms, vbat_dv and the speed never
 * trigger a push. */
static bool vs_other_change(const locksys_app_v1_StatusUpdate *a,
                            const locksys_app_v1_StatusUpdate *b)
{
    bool modes = (a->dcu_mode != b->dcu_mode) || (a->cgw_mode != b->cgw_mode) ||
                 (a->dcu_alive != b->dcu_alive) || (a->pairing_active != b->pairing_active);
    bool faults = (a->dcu_dtc_count != b->dcu_dtc_count) ||
                  (a->cgw_dtc_count != b->cgw_dtc_count) ||
                  (a->door_fault_flags != b->door_fault_flags) ||
                  (a->window_fault_flags != b->window_fault_flags);
    bool window = (a->window_position_pct != b->window_position_pct) ||
                  (a->window_encoder_status != b->window_encoder_status) ||
                  (a->window_inhibited != b->window_inhibited) ||
                  (a->door_inhibited != b->door_inhibited);

    return modes || faults || window || vs_temp_change(a, b);
}

/* @satisfies SWR-CGW-051 */
bool cgw_vstate_push_due(const cgw_vs_push_t *push, const locksys_app_v1_StatusUpdate *candidate,
                         uint32_t now_ms, bool motion)
{
    bool due = true;

    if (push->sent_any && !push->requested && !vs_urgent_change(&push->last, candidate))
    {
        uint32_t since = (uint32_t)(now_ms - push->t_last_ms);
        uint32_t period = motion ? LS_T_STATUS_PUSH_MOTION_MS : LS_T_STATUS_PUSH_IDLE_MS;

        due = (since >= period) ||
              ((since >= LS_T_STATUS_PUSH_GAP_MS) && vs_other_change(&push->last, candidate));
    }
    return due;
}

void cgw_vstate_push_commit(cgw_vs_push_t *push, const locksys_app_v1_StatusUpdate *sent,
                            uint32_t now_ms)
{
    push->last = *sent;
    push->t_last_ms = now_ms;
    push->sent_any = true;
    push->requested = false;
}
