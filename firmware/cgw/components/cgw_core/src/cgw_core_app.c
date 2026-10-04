/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include <string.h>

#include "cgw_core_priv.h"
#include "cgw_ports/cgw_canio_port.h"
#include "cgw_ports/cgw_crypto_port.h"
#include "cgw_ports/cgw_keystore_port.h"
#include "cgw_ports/cgw_link_port.h"
#include "cgw_ports/cgw_trace_port.h"
#include "cgw_proto/cgw_proto.h"
#include "ls_dtc_gen.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"

/* Encode a Body, seal it (or frame it as a handshake frame) and queue it. */
static cgw_rc_t core_send(cgw_core_t *core, cgw_conn_t conn, const locksys_app_v1_Body *body,
                          bool sealed)
{
    uint8_t raw[CGW_PROTO_BODY_MAX];
    uint8_t msg[CGW_WS_FRAME_MAX];
    cgw_proto_frame_t fr;
    size_t raw_len = 0u;
    size_t msg_len = 0u;
    cgw_rc_t rc = cgw_proto_encode_body(body, raw, sizeof(raw), &raw_len);

    (void)memset(&fr, 0, sizeof(fr));
    if ((rc == CGW_OK) && sealed)
    {
        rc = cgw_session_seal(&core->ses, conn, raw, raw_len, &fr);
    }
    else if (rc == CGW_OK)
    {
        (void)memcpy(fr.body, raw, raw_len);
        fr.body_len = (uint16_t)raw_len;
    }
    else
    {
        /* Encoding failed. */
    }
    if (rc == CGW_OK)
    {
        rc = cgw_proto_encode_frame(&fr, msg, sizeof(msg), &msg_len);
    }
    if (rc == CGW_OK)
    {
        bool status = body->which_msg == locksys_app_v1_Body_status_update_tag;

        rc = cgw_link_send(conn, msg, msg_len, status ? CGW_PRIO_STATUS : CGW_PRIO_CONTROL);
    }
    if (rc != CGW_OK)
    {
        core->stats.send_failures++;
    }
    cgw_crypto_zeroize(raw, sizeof(raw));
    return rc;
}

static void core_send_ctrl(cgw_core_t *core, const locksys_app_v1_Body *body)
{
    cgw_conn_t conn = cgw_session_controller(&core->ses);

    if (conn != CGW_CONN_NONE)
    {
        (void)core_send(core, conn, body, true);
    }
}

static void core_send_ack(cgw_core_t *core, uint8_t kind, const cgw_arb_ack_t *ack)
{
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    body.which_msg = locksys_app_v1_Body_command_ack_tag;
    body.msg.command_ack.kind = (locksys_app_v1_CommandKind)kind;
    body.msg.command_ack.ref_id = ack->ref_id;
    body.msg.command_ack.result = (locksys_app_v1_CommandResult)ack->result;
    core_send_ctrl(core, &body);
}

static void core_send_session_close(cgw_core_t *core, cgw_conn_t conn, uint8_t reason)
{
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    body.which_msg = locksys_app_v1_Body_session_close_tag;
    body.msg.session_close.reason = (locksys_app_v1_CommandResult)reason;
    (void)core_send(core, conn, &body, true);
}

void cgw_core_notice(cgw_core_t *core, uint32_t code, uint8_t severity)
{
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    body.which_msg = locksys_app_v1_Body_notice_tag;
    body.msg.notice.code = code;
    body.msg.notice.severity = (locksys_app_v1_FaultSeverity)severity;
    core_send_ctrl(core, &body);
}

/* @satisfies SWR-CGW-020 */
/* @satisfies SWR-CGW-053 */
void cgw_core_apply(cgw_core_t *core, const cgw_arb_out_t *out)
{
    if (out->stop_decided)
    {
        cgw_trace_set(CGW_TRACE_STOP_PENDING, true);
    }
    if (out->intent_changed)
    {
        core->intent_dirty = true;
    }
    if (out->win_ack.valid)
    {
        core_send_ack(core, LS_COMMAND_KIND_WINDOW, &out->win_ack);
    }
    if (out->door_ack.valid)
    {
        core_send_ack(core, LS_COMMAND_KIND_DOOR, &out->door_ack);
    }
    if (out->door_result_valid)
    {
        locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

        body.which_msg = locksys_app_v1_Body_door_command_result_tag;
        body.msg.door_command_result.request_id = out->door_request_id;
        body.msg.door_command_result.result = (locksys_app_v1_CommandResult)out->door_result;
        body.msg.door_command_result.lock_state =
            (locksys_app_v1_DoorLockState)out->door_lock_state;
        core_send_ctrl(core, &body);
    }
    if (out->door_can_valid)
    {
        (void)cgw_canio_post_door_request(out->door_can_req_id, out->door_can_request);
    }
    /* WINDOW_STOP_SESSION is logged only: there is no session to inform. */
    if ((out->notice != 0u) && (out->notice != LS_NOTICE_WINDOW_STOP_SESSION))
    {
        uint8_t sev = (out->notice == LS_NOTICE_WINDOW_STOP_DCU) ? LS_FAULT_SEVERITY_INFO
                                                                 : LS_FAULT_SEVERITY_WARNING;

        cgw_core_notice(core, out->notice, sev);
    }
}

void cgw_core_app_close(cgw_core_t *core, cgw_conn_t conn, uint16_t code, bool controller_lost)
{
    cgw_session_mark_closing(&core->ses, conn);
    cgw_link_close(conn, code);
    if (controller_lost)
    {
        cgw_trace_event(CGW_TRACE_EV_SESSION_CLOSED);
        cgw_core_latch(core, CGW_LATCH_SESSION);
    }
}

static void core_violation(cgw_core_t *core, cgw_conn_t conn, uint32_t now_ms)
{
    cgw_ses_verdict_t v = cgw_session_on_violation(&core->ses, conn, now_ms);

    core->stats.violations++;
    if (v.action == CGW_SES_CLOSE)
    {
        cgw_core_app_close(core, conn, v.close_code, v.controller_lost);
    }
}

static void core_send_hello(cgw_core_t *core, cgw_conn_t conn)
{
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;
    cgw_ses_identity_t id;

    id.device_id = core->id.device_id;
    id.fw_version = core->id.fw_version;
    id.pairing_window_open = cgw_pairing_is_open(&core->pair);
    body.which_msg = locksys_app_v1_Body_server_hello_tag;
    if (cgw_session_build_hello(&core->ses, conn, &id, &body.msg.server_hello) == CGW_OK)
    {
        (void)core_send(core, conn, &body, false);
    }
    else
    {
        cgw_core_app_close(core, conn, CGW_CLOSE_INTERNAL, false);
    }
}

void cgw_core_app_link_event(cgw_core_t *core, const cgw_core_event_t *ev)
{
    if (ev->type == CGW_EV_TCP_OPEN)
    {
        if (cgw_session_on_tcp_open(&core->ses, ev->conn, ev->t_ms) != CGW_OK)
        {
            cgw_link_close(ev->conn, CGW_CLOSE_NONE);
        }
    }
    else if (ev->type == CGW_EV_WS_OPEN)
    {
        if (cgw_session_on_ws_open(&core->ses, ev->conn) == CGW_OK)
        {
            core_send_hello(core, ev->conn);
        }
    }
    else if (cgw_session_on_close(&core->ses, ev->conn))
    {
        cgw_trace_event(CGW_TRACE_EV_SESSION_CLOSED);
        cgw_core_latch(core, CGW_LATCH_SESSION);
    }
    else
    {
        /* Close of a connection that was not the controller. */
    }
}

static void core_auth_ok(cgw_core_t *core, cgw_conn_t conn, const cgw_ses_auth_t *auth,
                         uint32_t now_ms)
{
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    if (auth->preempted != CGW_CONN_NONE)
    {
        core_send_session_close(core, auth->preempted, LS_COMMAND_RESULT_REJECTED_BUSY);
        cgw_core_app_close(core, auth->preempted, CGW_CLOSE_SESSION_TIMEOUT, true);
    }
    body.which_msg = locksys_app_v1_Body_auth_result_tag;
    body.msg.auth_result.result = locksys_app_v1_CommandResult_COMMAND_RESULT_OK;
    (void)memcpy(body.msg.auth_result.server_proof, auth->server_proof, CGW_HMAC_LEN);
    body.msg.auth_result.session_id = auth->session_id;
    body.msg.auth_result.session_timeout_ms = LS_T_SESSION_TO_MS;
    body.msg.auth_result.keepalive_period_ms = LS_T_APP_KA_MS;
    body.msg.auth_result.keepalive_timeout_ms = LS_T_CGW_KA_TO_MS;
    (void)core_send(core, conn, &body, true);
    cgw_link_set_authenticated(conn);
    cgw_trace_event(CGW_TRACE_EV_SESSION_AUTH);
    cgw_arbiter_session_start(&core->arb);
    cgw_vstate_push_reset(&core->push);
    core->status_seq = 0u;
    /* First Ping right after AuthResult(OK), so an RTT sample exists before the first press. */
    if (cgw_session_ping_due(&core->ses, now_ms, false))
    {
        body.which_msg = locksys_app_v1_Body_ping_tag;
        body.msg.ping.timestamp_ms = now_ms;
        (void)core_send(core, conn, &body, true);
    }
}

/* @satisfies SWR-CGW-005 */
/* @satisfies SWR-CGW-007 */
static void core_on_handshake(cgw_core_t *core, cgw_conn_t conn, const cgw_proto_frame_t *fr,
                              uint32_t t_rx_ms)
{
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    if ((cgw_proto_decode_body(fr->body, fr->body_len, &body) != CGW_OK) ||
        (body.which_msg != locksys_app_v1_Body_client_auth_tag))
    {
        core_violation(core, conn, t_rx_ms);
    }
    else
    {
        cgw_ses_auth_input_t in;
        cgw_ses_auth_t auth;

        in.device_id = core->id.device_id;
        in.pending_kpair = cgw_pairing_pending_key(&core->pair);
        in.current_kpair = core->paired ? core->rec.k_pair : NULL;
        auth = cgw_session_on_client_auth(&core->ses, conn, &body.msg.client_auth, &in, t_rx_ms);
        if (auth.result == LS_COMMAND_RESULT_OK)
        {
            core_auth_ok(core, conn, &auth, t_rx_ms);
        }
        else
        {
            if (auth.result != LS_COMMAND_RESULT_UNSPECIFIED)
            {
                (void)memset(&body, 0, sizeof(body));
                body.which_msg = locksys_app_v1_Body_auth_result_tag;
                body.msg.auth_result.result = (locksys_app_v1_CommandResult)auth.result;
                (void)core_send(core, conn, &body, false);
            }
            cgw_core_app_close(core, conn, auth.close_code, false);
        }
        cgw_crypto_zeroize(&auth, sizeof(auth));
    }
    cgw_crypto_zeroize(&body, sizeof(body));
}

static void core_on_window_move(cgw_core_t *core, const locksys_app_v1_WindowMove *wm,
                                uint32_t t_rx_ms)
{
    cgw_arb_env_t env;
    cgw_arb_move_t mv;
    cgw_arb_out_t out;

    mv.press_id = wm->press_id;
    mv.hold_ms = wm->hold_ms;
    mv.t_rx_ms = t_rx_ms;
    mv.dir = ((uint32_t)wm->direction <= UINT8_MAX) ? (uint8_t)wm->direction : UINT8_MAX;
    cgw_core_env(core, t_rx_ms, &env);
    cgw_arbiter_on_move(&core->arb, &mv, &env, &out);
    if ((core->arb.win.state == CGW_ARB_WIN_MOVING) && (core->arb.win.press_id == wm->press_id))
    {
        cgw_trace_event(CGW_TRACE_EV_WINDOW_MOVE);
    }
    cgw_core_apply(core, &out);
}

static void core_on_door_command(cgw_core_t *core, const locksys_app_v1_DoorCommand *dc,
                                 uint32_t t_rx_ms)
{
    cgw_arb_env_t env;
    cgw_arb_door_cmd_t cmd;
    cgw_arb_out_t out;

    cgw_trace_event(CGW_TRACE_EV_DOOR_COMMAND);
    cmd.request_id = dc->request_id;
    cmd.t_rx_ms = t_rx_ms;
    cmd.action = ((uint32_t)dc->action <= UINT8_MAX) ? (uint8_t)dc->action : UINT8_MAX;
    cgw_core_env(core, t_rx_ms, &env);
    cgw_arbiter_on_door(&core->arb, &cmd, &env, &out);
    cgw_core_apply(core, &out);
}

static void core_on_body(cgw_core_t *core, cgw_conn_t conn, const locksys_app_v1_Body *body,
                         uint32_t t_rx_ms)
{
    cgw_arb_out_t out;

    switch (body->which_msg)
    {
        case locksys_app_v1_Body_ping_tag:
        {
            locksys_app_v1_Body pong = locksys_app_v1_Body_init_zero;

            pong.which_msg = locksys_app_v1_Body_pong_tag;
            pong.msg.pong.echo_timestamp_ms = body->msg.ping.timestamp_ms;
            (void)core_send(core, conn, &pong, true);
            break;
        }
        case locksys_app_v1_Body_pong_tag:
            cgw_session_on_pong(&core->ses, conn, body->msg.pong.echo_timestamp_ms, t_rx_ms);
            break;
        case locksys_app_v1_Body_window_move_tag:
            core_on_window_move(core, &body->msg.window_move, t_rx_ms);
            break;
        case locksys_app_v1_Body_window_stop_tag:
            cgw_trace_event(CGW_TRACE_EV_WINDOW_STOP);
            cgw_arbiter_on_stop(&core->arb, body->msg.window_stop.press_id, &out);
            cgw_core_apply(core, &out);
            break;
        case locksys_app_v1_Body_door_command_tag:
            core_on_door_command(core, &body->msg.door_command, t_rx_ms);
            break;
        case locksys_app_v1_Body_status_request_tag:
            core->push.requested = true;
            break;
        default:
            /* ClientAuth inside a session is handled as a violation by the caller. */
            break;
    }
}

/* @satisfies SWR-CGW-006 */
static void core_on_deliver(cgw_core_t *core, cgw_conn_t conn, const cgw_proto_frame_t *fr,
                            uint32_t t_rx_ms)
{
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;
    const cgw_session_t *s = cgw_session_find(&core->ses, conn);

    if ((s != NULL) && s->pending_key && cgw_pairing_is_open(&core->pair))
    {
        cgw_core_app_commit_pairing(core, conn, t_rx_ms);
    }
    if (cgw_proto_decode_body(fr->body, fr->body_len, &body) != CGW_OK)
    {
        core_violation(core, conn, t_rx_ms);
    }
    else if (body.which_msg == 0u)
    {
        core->stats.unknown_body++;
    }
    else if (!cgw_proto_is_app_message(body.which_msg) ||
             (body.which_msg == locksys_app_v1_Body_client_auth_tag))
    {
        core_violation(core, conn, t_rx_ms);
    }
    else
    {
        core_on_body(core, conn, &body, t_rx_ms);
    }
}

void cgw_core_app_frame(cgw_core_t *core, const cgw_core_event_t *ev)
{
    cgw_proto_frame_t fr;

    if (cgw_proto_decode_frame(ev->data, ev->len, &fr) != CGW_OK)
    {
        core_violation(core, ev->conn, ev->t_ms);
    }
    else
    {
        cgw_ses_verdict_t v = cgw_session_on_frame(&core->ses, ev->conn, &fr, ev->t_ms);

        if (v.action == CGW_SES_CLOSE)
        {
            core->stats.violations++;
            cgw_core_app_close(core, ev->conn, v.close_code, v.controller_lost);
        }
        else if (v.action == CGW_SES_HANDSHAKE)
        {
            core_on_handshake(core, ev->conn, &fr, ev->t_ms);
        }
        else if (v.action == CGW_SES_DELIVER)
        {
            core_on_deliver(core, ev->conn, &fr, ev->t_ms);
        }
        else
        {
            /* Dropped. */
        }
    }
}

/* @satisfies SWR-CGW-018 */
void cgw_core_app_commit_pairing(cgw_core_t *core, cgw_conn_t conn, uint32_t now_ms)
{
    const cgw_session_t *s = cgw_session_find(&core->ses, conn);
    cgw_pairing_record_t rec = core->rec;
    cgw_rc_t rc = (s != NULL) ? cgw_pairing_commit(&core->pair, s->client_id, &rec) : CGW_E_STATE;

    if (rc == CGW_OK)
    {
        core->rec = rec;
        core->paired = true;
        cgw_core_notice(core, LS_NOTICE_PAIRING_COMMITTED, LS_FAULT_SEVERITY_INFO);
        /* Every other connection used the old key or none: close with 4005. */
        for (uint32_t i = 0u; i < CGW_SESSIONS_MAX; i++)
        {
            const cgw_session_t *e = &core->ses.entries[i];

            if ((e->state != CGW_SES_FREE) && (e->state != CGW_SES_CLOSING) && (e->conn != conn))
            {
                cgw_core_app_close(core, e->conn, CGW_CLOSE_PAIRING_REQUIRED, false);
            }
        }
    }
    else if (rc == CGW_E_IO)
    {
        /* Key store fault: erase, stay unpaired (SWR-CGW-068). */
        (void)cgw_keystore_erase();
        core->paired = false;
        cgw_core_report(core, CGW_DTC_NVS, true, now_ms);
        core_send_session_close(core, conn, LS_COMMAND_RESULT_REJECTED_AUTH);
        cgw_core_app_close(core, conn, CGW_CLOSE_PAIRING_REQUIRED, true);
    }
    else
    {
        /* Window closed meanwhile. */
    }
    cgw_crypto_zeroize(&rec, sizeof(rec));
}

static void core_status_push(cgw_core_t *core, uint32_t now_ms)
{
    cgw_conn_t conn = cgw_session_controller(&core->ses);

    if (conn != CGW_CONN_NONE)
    {
        locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;
        locksys_app_v1_StatusUpdate *su = &body.msg.status_update;
        cgw_vs_status_ctx_t ctx;
        bool motion;

        ctx.seq = core->status_seq + 1u;
        ctx.last_door_request_id = core->arb.last_door_request;
        ctx.last_door_result = core->arb.last_door_result;
        ctx.cgw_mode = core->health.mode;
        ctx.cgw_dtc_count = cgw_health_dtc_count(&core->health);
        ctx.pairing_active = cgw_pairing_is_open(&core->pair);
        cgw_vstate_build_status(&core->vs, &ctx, now_ms, su);
        motion = cgw_arbiter_press_active(&core->arb) ||
                 (su->window_state == locksys_app_v1_WindowState_WINDOW_STATE_MOVING_UP) ||
                 (su->window_state == locksys_app_v1_WindowState_WINDOW_STATE_MOVING_DOWN);
        if (cgw_vstate_push_due(&core->push, su, now_ms, motion))
        {
            body.which_msg = locksys_app_v1_Body_status_update_tag;
            if (core_send(core, conn, &body, true) == CGW_OK)
            {
                core->status_seq = ctx.seq;
                cgw_vstate_push_commit(&core->push, su, now_ms);
            }
        }
    }
}

void cgw_core_app_tick(cgw_core_t *core, uint32_t now_ms)
{
    cgw_ses_tick_result_t closes;
    cgw_conn_t ctrl;

    cgw_session_tick(&core->ses, now_ms, &closes);
    for (uint8_t i = 0u; i < closes.count; i++)
    {
        const cgw_ses_close_t *c = &closes.closes[i];

        if (c->send_session_close)
        {
            core_send_session_close(core, c->conn, LS_COMMAND_RESULT_FAILED_TIMEOUT);
        }
        cgw_core_app_close(core, c->conn, c->close_code, c->controller_lost);
    }
    ctrl = cgw_session_controller(&core->ses);
    if ((ctrl != CGW_CONN_NONE) &&
        cgw_session_ping_due(&core->ses, now_ms, cgw_arbiter_press_active(&core->arb)))
    {
        locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

        body.which_msg = locksys_app_v1_Body_ping_tag;
        body.msg.ping.timestamp_ms = now_ms;
        (void)core_send(core, ctrl, &body, true);
    }
    core_status_push(core, now_ms);
    cgw_link_flush();
}
