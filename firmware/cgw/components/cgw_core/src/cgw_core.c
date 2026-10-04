/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_core/cgw_core.h"

#include <string.h>

#include "cgw_core_priv.h"
#include "cgw_ports/cgw_can_port.h"
#include "cgw_ports/cgw_canio_port.h"
#include "cgw_ports/cgw_console_port.h"
#include "cgw_ports/cgw_crypto_port.h"
#include "cgw_ports/cgw_keystore_port.h"
#include "cgw_ports/cgw_link_port.h"
#include "cgw_ports/cgw_system_port.h"
#include "cgw_ports/cgw_trace_port.h"
#include "ls_dtc_gen.h"
#include "ls_e2e/ls_e2e.h"
#include "ls_enums_gen.h"

void cgw_core_init(cgw_core_t *core, const cgw_core_identity_t *id, const cgw_pairing_record_t *rec,
                   uint32_t now_ms)
{
    cgw_dtc_t reset_dtc = CGW_DTC_COUNT;

    (void)memset(core, 0, sizeof(*core));
    core->id = *id;
    if (rec != NULL)
    {
        core->rec = *rec;
        core->paired = true;
    }
    cgw_session_init(&core->ses);
    cgw_arbiter_init(&core->arb);
    cgw_vstate_init(&core->vs);
    cgw_vstate_push_reset(&core->push);
    cgw_health_init(&core->health);
    cgw_pairing_init(&core->pair);
    core->reset_reason = cgw_health_map_reset(id->reset_src, &reset_dtc);
    if (reset_dtc != CGW_DTC_COUNT)
    {
        (void)cgw_health_report(&core->health, reset_dtc, true, now_ms);
    }
    cgw_health_check_reset_storm(&core->health, id->wdt_resets);
    core->bus = (uint8_t)CGW_BUS_STOPPED;
    core->dcu_mode_prev = LS_NODE_MODE_UNKNOWN;
    core->indication = (uint8_t)CGW_IND_OFF;
    cgw_arbiter_intent(&core->arb, &core->intent);
    core->intent_dirty = true;
    core->node_dirty = true;
}

uint8_t cgw_core_mode(const cgw_core_t *core)
{
    return core->health.mode;
}

void cgw_core_env(const cgw_core_t *core, uint32_t now_ms, cgw_arb_env_t *env)
{
    const cgw_vstate_t *vs = &core->vs;
    bool node = cgw_vstate_is_fresh(vs, CGW_VS_NODE_STS);
    bool door = cgw_vstate_is_fresh(vs, CGW_VS_DOOR_STS);

    (void)memset(env, 0, sizeof(*env));
    env->cgw_mode = core->health.mode;
    env->dcu_mode = node ? vs->node.dcu_sts_mode : LS_NODE_MODE_UNKNOWN;
    env->lock_state = door ? vs->door.door_sts_lock_state : LS_DOOR_LOCK_STATE_UNKNOWN;
    env->pairing_busy = cgw_pairing_is_open(&core->pair) || core->factory_reset;
    env->dcu_alive = node;
    env->win_inhibit = !node || (vs->node.dcu_sts_win_inhibit != 0u);
    env->lock_inhibit = !node || (vs->node.dcu_sts_lock_inhibit != 0u);
    env->version_ok = cgw_vstate_version_ok(vs);
    env->bus_off = core->bus == (uint8_t)CGW_BUS_OFF;
    env->win_sts_valid = cgw_vstate_is_fresh(vs, CGW_VS_WIN_STS);
    env->door_sts_valid = door;
    env->door_rate_limited = door && (vs->door.door_sts_rate_limited != 0u);
    env->link_ok = cgw_session_link_ok(&core->ses, now_ms);
}

void cgw_core_latch(cgw_core_t *core, cgw_latch_reason_t reason)
{
    cgw_arb_out_t out;

    (void)memset(&out, 0, sizeof(out));
    cgw_arbiter_latch(&core->arb, reason, &out);
    cgw_core_apply(core, &out);
}

void cgw_core_report(cgw_core_t *core, cgw_dtc_t dtc, bool failed, uint32_t now_ms)
{
    if (cgw_health_report(&core->health, dtc, failed, now_ms))
    {
        cgw_core_notice(core, cgw_health_dtc_value(dtc), cgw_health_dtc_severity(dtc));
    }
}

/* @satisfies SWR-CGW-060 */
static void core_update_mode(cgw_core_t *core)
{
    cgw_health_inputs_t in;
    uint8_t prev = core->health.mode;
    uint8_t mode;
    uint8_t dcu_mode;
    bool node = cgw_vstate_is_fresh(&core->vs, CGW_VS_NODE_STS);

    in.ap_started = core->ap_started;
    in.can_active = core->can_enabled && (core->bus != (uint8_t)CGW_BUS_OFF);
    in.dcu_alive = node;
    in.dcu_seen = core->vs.seen[CGW_VS_NODE_STS];
    in.version_fault = core->vs.version_fault;
    mode = cgw_health_update_mode(&core->health, &in);
    dcu_mode = node ? core->vs.node.dcu_sts_mode : LS_NODE_MODE_UNKNOWN;
    if ((mode != prev) || (dcu_mode != core->dcu_mode_prev))
    {
        cgw_core_latch(core, CGW_LATCH_MODE);
    }
    core->dcu_mode_prev = dcu_mode;
}

static uint8_t core_app_link(const cgw_core_t *core)
{
    uint8_t link = LS_APP_LINK_STATE_NONE;

    if (cgw_pairing_is_open(&core->pair))
    {
        link = LS_APP_LINK_STATE_PAIRING;
    }
    else if (cgw_session_controller(&core->ses) != CGW_CONN_NONE)
    {
        link = LS_APP_LINK_STATE_AUTHENTICATED;
    }
    else
    {
        for (uint32_t i = 0u; i < CGW_SESSIONS_MAX; i++)
        {
            if (core->ses.entries[i].state != CGW_SES_FREE)
            {
                link = LS_APP_LINK_STATE_CONNECTED;
            }
        }
    }
    return link;
}

static cgw_indication_t core_indication(const cgw_core_t *core, uint8_t link)
{
    cgw_indication_t ind;

    switch (core->health.mode)
    {
        case LS_NODE_MODE_SAFE:
            ind = CGW_IND_FAULT;
            break;
        case LS_NODE_MODE_DEGRADED:
            ind = CGW_IND_DEGRADED;
            break;
        case LS_NODE_MODE_NORMAL:
            ind = CGW_IND_READY;
            break;
        default:
            ind = CGW_IND_STARTING;
            break;
    }
    if (link == LS_APP_LINK_STATE_PAIRING)
    {
        ind = CGW_IND_PAIRING;
    }
    else if ((link == LS_APP_LINK_STATE_AUTHENTICATED) && (ind == CGW_IND_READY))
    {
        ind = CGW_IND_CONNECTED;
    }
    else
    {
        /* Mode indication. */
    }
    return ind;
}

/* Post intent and node information to can_io; a full queue is retried at the next tick. */
static void core_publish(cgw_core_t *core)
{
    cgw_node_info_t node;
    cgw_win_intent_t intent;
    uint8_t link = core_app_link(core);
    cgw_indication_t ind = core_indication(core, link);

    cgw_arbiter_intent(&core->arb, &intent);
    if (core->intent_dirty || (memcmp(&intent, &core->intent, sizeof(intent)) != 0))
    {
        core->intent = intent;
        core->intent_dirty = cgw_canio_post_intent(&intent) != CGW_OK;
    }
    (void)memset(&node, 0, sizeof(node));
    node.mode = core->health.mode;
    node.app_link = link;
    node.wifi_clients = core->wifi_clients;
    node.reset_reason = core->reset_reason;
    node.dtc_count = cgw_health_dtc_count(&core->health);
    node.heap_free_pct = core->heap_free_pct;
    if (core->node_dirty || (memcmp(&node, &core->node, sizeof(node)) != 0))
    {
        core->node = node;
        core->node_dirty = cgw_canio_post_node_info(&node) != CGW_OK;
    }
    if ((uint8_t)ind != core->indication)
    {
        core->indication = (uint8_t)ind;
        cgw_console_set_indication(ind);
    }
}

static void core_on_can_rx(cgw_core_t *core, const cgw_core_event_t *ev)
{
    cgw_vs_msg_t msg = (cgw_vs_msg_t)ev->arg;
    cgw_arb_out_t out;

    (void)memset(&out, 0, sizeof(out));
    if (cgw_vstate_on_rx(&core->vs, msg, ev->data, ev->t_ms) == CGW_OK)
    {
        if (msg == CGW_VS_WIN_STS)
        {
            cgw_arbiter_on_win_sts(&core->arb, core->vs.win.win_sts_press_id_echo,
                                   core->vs.win.win_sts_win_result, &out);
        }
        else if (msg == CGW_VS_DOOR_STS)
        {
            cgw_arb_door_sts_t sts;

            sts.t_rx_ms = ev->t_ms;
            sts.last_req_id = core->vs.door.door_sts_last_req_id;
            sts.last_result = core->vs.door.door_sts_last_result;
            sts.lock_state = core->vs.door.door_sts_lock_state;
            cgw_arbiter_on_door_sts(&core->arb, &sts, &out);
        }
        else if (msg == CGW_VS_NODE_STS)
        {
            cgw_core_report(core, CGW_DTC_DCU_LOST, false, ev->t_ms);
            cgw_core_report(core, CGW_DTC_VERSION, core->vs.version_fault, ev->t_ms);
        }
        else
        {
            /* Cached only. */
        }
        cgw_core_apply(core, &out);
    }
}

/* @satisfies SWR-CGW-049 */
static void core_on_rx_timeout(cgw_core_t *core, const cgw_core_event_t *ev)
{
    cgw_vs_msg_t msg = (cgw_vs_msg_t)ev->arg;

    cgw_vstate_on_timeout(&core->vs, msg);
    if ((msg == CGW_VS_NODE_STS) || (msg == CGW_VS_WIN_STS))
    {
        cgw_core_latch(core, CGW_LATCH_COMM);
    }
    if (msg == CGW_VS_NODE_STS)
    {
        cgw_core_report(core, CGW_DTC_DCU_LOST, true, ev->t_ms);
    }
}

static void core_on_can_bus(cgw_core_t *core, const cgw_core_event_t *ev)
{
    cgw_bus_state_t state = (cgw_bus_state_t)ev->arg;

    if (ev->type == CGW_EV_BUSOFF_HEALED)
    {
        cgw_core_report(core, CGW_DTC_BUS_OFF, false, ev->t_ms);
    }
    else if (ev->type == CGW_EV_E2E_ERROR)
    {
        cgw_dtc_t dtc =
            (ev->arg == LS_E2E_STATUS_CRC_ERROR) ? CGW_DTC_E2E_CRC : CGW_DTC_E2E_SEQUENCE;

        cgw_core_report(core, dtc, true, ev->t_ms);
    }
    else
    {
        core->bus = (uint8_t)state;
        if (state == CGW_BUS_OFF)
        {
            cgw_core_latch(core, CGW_LATCH_COMM);
            cgw_core_report(core, CGW_DTC_BUS_OFF, true, ev->t_ms);
        }
        else if (state != CGW_BUS_STOPPED)
        {
            core->can_enabled = true;
        }
        else
        {
            /* Controller stopped. */
        }
    }
}

static void core_on_pair_req(cgw_core_t *core, const cgw_core_event_t *ev)
{
    uint32_t now_ms = ev->t_ms;
    char uri[CGW_PAIR_URI_MAX];

    if (!cgw_arbiter_press_active(&core->arb) && !core->factory_reset &&
        (cgw_pairing_open(&core->pair, now_ms) == CGW_OK))
    {
        cgw_pair_uri_in_t in;
        size_t len;

        in.device_id = core->id.device_id;
        in.ssid = core->id.ssid;
        in.passphrase = core->id.passphrase;
        in.k_pair = cgw_pairing_pending_key(&core->pair);
        in.bssid = core->id.ap_mac;
        in.transition = core->id.wifi_transition;
        len = cgw_pairing_build_uri(&in, uri, sizeof(uri));
        if (len > 0u)
        {
            (void)cgw_console_show_pairing_qr(uri, len);
        }
        cgw_crypto_zeroize(uri, sizeof(uri));
        cgw_core_notice(core, LS_NOTICE_PAIRING_WINDOW_OPEN, LS_FAULT_SEVERITY_INFO);
    }
}

/* @satisfies SWR-CGW-019 */
static void core_on_factory_reset(cgw_core_t *core, const cgw_core_event_t *ev)
{
    uint32_t now_ms = ev->t_ms;
    char pass[CGW_PASSPHRASE_LEN + 1u];

    core->factory_reset = true;
    cgw_core_latch(core, CGW_LATCH_MODE);
    cgw_pairing_close(&core->pair);
    cgw_crypto_zeroize(&core->rec, sizeof(core->rec));
    core->paired = false;
    if ((cgw_keystore_erase() != CGW_OK) ||
        (cgw_pairing_make_passphrase(pass, sizeof(pass)) != CGW_OK) ||
        (cgw_keystore_store_passphrase(pass) != CGW_OK))
    {
        cgw_core_report(core, CGW_DTC_NVS, true, now_ms);
    }
    cgw_crypto_zeroize(pass, sizeof(pass));
    cgw_system_restart();
}

static void core_on_wifi(cgw_core_t *core, const cgw_core_event_t *ev)
{
    if (ev->type == CGW_EV_AP_STARTED)
    {
        core->ap_started = true;
        cgw_trace_event(CGW_TRACE_EV_AP_START);
        cgw_core_report(core, CGW_DTC_SOFTAP, false, ev->t_ms);
    }
    else if (ev->type == CGW_EV_AP_FAILED)
    {
        cgw_core_report(core, CGW_DTC_SOFTAP, true, ev->t_ms);
    }
    else
    {
        core->wifi_clients = ev->aux;
        if (ev->type == CGW_EV_STA_LEAVE)
        {
            cgw_conn_t ctrl = cgw_session_controller(&core->ses);

            /* @satisfies SWR-CGW-027 */
            if ((ctrl != CGW_CONN_NONE) &&
                ((ev->arg == 0u) ? (core->wifi_clients == 0u)
                                 : (cgw_link_find_by_peer(ev->arg) == ctrl)))
            {
                cgw_core_app_close(core, ctrl, CGW_CLOSE_NONE, true);
            }
        }
    }
}

static void core_on_tick(cgw_core_t *core, const cgw_core_event_t *ev)
{
    cgw_arb_env_t env;
    cgw_arb_out_t out;

    core->heap_free_pct = ev->aux;
    cgw_health_tick(&core->health, ev->t_ms);
    if (cgw_pairing_tick(&core->pair, ev->t_ms))
    {
        cgw_core_notice(core, LS_NOTICE_PAIRING_WINDOW_CLOSED, LS_FAULT_SEVERITY_INFO);
    }
    cgw_core_env(core, ev->t_ms, &env);
    cgw_arbiter_tick(&core->arb, ev->t_ms, &env, &out);
    cgw_core_apply(core, &out);
    cgw_core_app_tick(core, ev->t_ms);
}

static void core_on_dtc(cgw_core_t *core, const cgw_core_event_t *ev)
{
    if (ev->arg < (uint32_t)CGW_DTC_COUNT)
    {
        cgw_core_report(core, (cgw_dtc_t)ev->arg, ev->aux != 0u, ev->t_ms);
    }
}

/* Handler of each event type, indexed by cgw_core_ev_type_t. */
typedef void (*core_handler_t)(cgw_core_t *core, const cgw_core_event_t *ev);

static const core_handler_t k_handlers[] = {
    NULL,                    /* NONE */
    core_on_tick,            /* TICK */
    cgw_core_app_link_event, /* TCP_OPEN */
    cgw_core_app_link_event, /* WS_OPEN */
    cgw_core_app_link_event, /* WS_CLOSE */
    cgw_core_app_frame,      /* WS_FRAME */
    core_on_wifi,            /* AP_STARTED */
    core_on_wifi,            /* AP_FAILED */
    core_on_wifi,            /* STA_JOIN */
    core_on_wifi,            /* STA_LEAVE */
    core_on_can_rx,          /* CAN_RX */
    core_on_rx_timeout,      /* RX_TIMEOUT */
    core_on_can_bus,         /* CAN_BUS */
    core_on_can_bus,         /* BUSOFF_HEALED */
    core_on_can_bus,         /* E2E_ERROR */
    core_on_pair_req,        /* PAIR_REQ */
    core_on_factory_reset,   /* FACTORY_RESET */
    core_on_dtc,             /* DTC */
};

_Static_assert((sizeof(k_handlers) / sizeof(k_handlers[0])) == ((size_t)CGW_EV_DTC + 1u),
               "one handler per core event type");

void cgw_core_dispatch(cgw_core_t *core, const cgw_core_event_t *ev)
{
    if ((ev->type < (sizeof(k_handlers) / sizeof(k_handlers[0]))) && (k_handlers[ev->type] != NULL))
    {
        k_handlers[ev->type](core, ev);
    }
    core_update_mode(core);
    core_publish(core);
}
