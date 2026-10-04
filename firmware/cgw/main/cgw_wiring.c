/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_wiring.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "cgw_can_esp/cgw_can_esp.h"
#include "cgw_com/cgw_com.h"
#include "cgw_crypto_psa/cgw_crypto_psa.h"
#include "cgw_hmi_esp/cgw_hmi_esp.h"
#include "cgw_platform_esp/cgw_platform_esp.h"
#include "cgw_ports/cgw_canio_port.h"
#include "cgw_ports/cgw_clock_port.h"
#include "cgw_ports/cgw_console_port.h"
#include "cgw_ports/cgw_crypto_port.h"
#include "cgw_ports/cgw_trace_port.h"
#include "cgw_wifi_esp/cgw_wifi_esp.h"
#include "cgw_ws_esp/cgw_ws_esp.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "ls_can_matrix_gen.h"
#include "ls_params_gen.h"
#include "ls_version_gen.h"
#include "sdkconfig.h"

#if CONFIG_CGW_FAULT_INJECTION
#include "cgw_platform_esp/cgw_fi.h"
#endif

/* Task model (LS-CGW-SAD-001 section 4.1). */
#define CORE_STACK     (8192u)
#define CORE_PRIO      (16u)
#define CORE_Q_LEN     (12u)
#define CANIO_STACK    (4096u)
#define CANIO_PRIO     (20u)
#define CANIO_Q_LEN    (32u)
#define SYS_STACK      (4096u)
#define SYS_PRIO       (4u)
#define SYS_Q_LEN      (8u)
#define SYS_PERIOD_MS  (20u)
#define CPU_NET        (0)
#define CPU_RT         (1)
#define HTTPD_STACK    (6144u)
#define HTTPD_PRIO     (12u)
#define CTRL_WAIT_MS   (20u)
#define AP_START_TRIES (3u)
#define ALIVE_GRACE_MS (2000u)
#define BOOT_GPIO      (0)

_Static_assert((int)CGW_COM_RX_COUNT == (int)CGW_VS_MSG_COUNT, "message order of com and vstate");

static const char *TAG = "cgw_wiring";

typedef enum
{
    SYS_MSG_SHOW_QR = 0,
    SYS_MSG_INDICATION
} sys_msg_type_t;

typedef struct
{
    uint8_t type;
    uint8_t ind;
} sys_msg_t;

static cgw_core_t s_core;
static cgw_com_t s_com;
static cgw_ao_t s_ao_core;
static cgw_ao_t s_ao_canio;
static cgw_ao_t s_ao_sys;
static atomic_uint s_core_drops;
static atomic_bool s_ap_started;

static StackType_t s_core_stack[CORE_STACK / sizeof(StackType_t)];
static StaticTask_t s_core_tcb;
static uint8_t s_core_q[CORE_Q_LEN * sizeof(cgw_core_event_t)];
static StaticQueue_t s_core_qbuf;
static StackType_t s_canio_stack[CANIO_STACK / sizeof(StackType_t)];
static StaticTask_t s_canio_tcb;
static uint8_t s_canio_q[CANIO_Q_LEN * sizeof(cgw_canio_event_t)];
static StaticQueue_t s_canio_qbuf;
static StackType_t s_sys_stack[SYS_STACK / sizeof(StackType_t)];
static StaticTask_t s_sys_tcb;
static uint8_t s_sys_q[SYS_Q_LEN * sizeof(sys_msg_t)];
static StaticQueue_t s_sys_qbuf;

/* QR handoff core -> sys: ownership flag; sys zeroises after printing. */
static char s_qr_uri[CGW_PAIR_URI_MAX];
static size_t s_qr_len;
static atomic_bool s_qr_busy;

cgw_rc_t cgw_wiring_post_core(const cgw_core_event_t *ev, uint32_t wait_ms)
{
    cgw_rc_t rc = cgw_platform_ao_post(&s_ao_core, ev, wait_ms);

    if (rc != CGW_OK)
    {
        atomic_fetch_add(&s_core_drops, 1u);
    }
    return rc;
}

static void post_simple(cgw_core_ev_type_t type, uint32_t arg, uint8_t aux, uint32_t wait_ms)
{
    cgw_core_event_t ev;

    ev.type = (uint8_t)type;
    ev.t_ms = cgw_clock_now_ms();
    ev.conn = CGW_CONN_NONE;
    ev.arg = arg;
    ev.aux = aux;
    ev.len = 0u;
    (void)cgw_wiring_post_core(&ev, wait_ms);
}

void cgw_wiring_report_dtc(cgw_dtc_t dtc, bool failed)
{
    post_simple(CGW_EV_DTC, (uint32_t)dtc, failed ? 1u : 0u, CTRL_WAIT_MS);
}

/* ---------------------------------------------------------------- can_io and console ports */

cgw_rc_t cgw_canio_post_intent(const cgw_win_intent_t *intent)
{
    cgw_canio_event_t ev;

    ev.type = (uint8_t)CGW_CANIO_EV_INTENT;
    ev.u.intent = *intent;
    return cgw_platform_ao_post_front(&s_ao_canio, &ev, 0u);
}

cgw_rc_t cgw_canio_post_door_request(uint8_t req_id, uint8_t request)
{
    cgw_canio_event_t ev;

    ev.type = (uint8_t)CGW_CANIO_EV_DOOR;
    ev.u.door.req_id = req_id;
    ev.u.door.request = request;
    return cgw_platform_ao_post(&s_ao_canio, &ev, 0u);
}

cgw_rc_t cgw_canio_post_node_info(const cgw_node_info_t *info)
{
    cgw_canio_event_t ev;

    ev.type = (uint8_t)CGW_CANIO_EV_NODE;
    ev.u.node = *info;
    return cgw_platform_ao_post(&s_ao_canio, &ev, 0u);
}

cgw_rc_t cgw_console_show_pairing_qr(const char *uri, size_t len)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((uri != NULL) && (len < sizeof(s_qr_uri)))
    {
        rc = CGW_E_STATE;
        if (!atomic_exchange(&s_qr_busy, true))
        {
            const sys_msg_t msg = {(uint8_t)SYS_MSG_SHOW_QR, 0u};

            (void)memcpy(s_qr_uri, uri, len);
            s_qr_uri[len] = '\0';
            s_qr_len = len;
            rc = cgw_platform_ao_post(&s_ao_sys, &msg, 0u);
            if (rc != CGW_OK)
            {
                cgw_crypto_zeroize(s_qr_uri, sizeof(s_qr_uri));
                atomic_store(&s_qr_busy, false);
            }
        }
    }
    return rc;
}

void cgw_console_set_indication(cgw_indication_t ind)
{
    const sys_msg_t msg = {(uint8_t)SYS_MSG_INDICATION, (uint8_t)ind};

    (void)cgw_platform_ao_post(&s_ao_sys, &msg, 0u);
}

/* --------------------------------------------------------------------------------- core task */

static void core_run(const cgw_core_event_t *ev)
{
    cgw_trace_set(CGW_TRACE_CORE_BUSY, true);
#if CONFIG_CGW_FAULT_INJECTION
    if (cgw_fi_state.core_hang_ms > 0u)
    {
        uint32_t until = cgw_clock_now_ms() + cgw_fi_state.core_hang_ms;

        cgw_fi_state.core_hang_ms = 0u;
        while (!cgw_time_reached(cgw_clock_now_ms(), until))
        {
        }
    }
#endif
    cgw_core_dispatch(&s_core, ev);
    cgw_trace_set(CGW_TRACE_CORE_BUSY, false);
}

static void core_task(void *arg)
{
    static cgw_core_event_t s_ev;
    static cgw_core_event_t s_tick;
    uint32_t next_tick = cgw_clock_now_ms() + LS_T_CGW_TICK_MS;

    (void)arg;
    (void)esp_task_wdt_add(NULL);
    for (;;)
    {
        uint32_t now = cgw_clock_now_ms();
        TickType_t wait = cgw_time_reached(now, next_tick) ? 0 : pdMS_TO_TICKS(next_tick - now);

        if (xQueueReceive(s_ao_core.q, &s_ev, wait) == pdTRUE)
        {
            core_run(&s_ev);
        }
        now = cgw_clock_now_ms();
        if (cgw_time_reached(now, next_tick))
        {
            (void)memset(&s_tick, 0, sizeof(s_tick));
            s_tick.type = (uint8_t)CGW_EV_TICK;
            s_tick.t_ms = now;
            s_tick.aux = cgw_platform_heap_free_pct();
            core_run(&s_tick);
            next_tick += LS_T_CGW_TICK_MS;
            if (cgw_time_reached(now, next_tick))
            {
                next_tick = now + LS_T_CGW_TICK_MS;
            }
        }
        cgw_platform_alive_kick(CGW_ALIVE_CORE);
        (void)esp_task_wdt_reset();
    }
}

/* ------------------------------------------------------------------------------- can_io task */

static void canio_on_rx(const cgw_canio_event_t *ev, uint32_t now)
{
    cgw_com_rx_result_t res = cgw_com_on_rx(&s_com, ev->id, ev->u.data, ev->dlc, now);

    if (res.deliver)
    {
        cgw_core_event_t cev;

        cev.type = (uint8_t)CGW_EV_CAN_RX;
        cev.t_ms = now;
        cev.conn = CGW_CONN_NONE;
        cev.arg = (uint32_t)res.msg;
        cev.aux = 0u;
        cev.len = CGW_CAN_DLC_MAX;
        (void)memcpy(cev.data, ev->u.data, CGW_CAN_DLC_MAX);
        (void)cgw_wiring_post_core(&cev, 0u);
    }
    else if ((res.status == LS_E2E_STATUS_CRC_ERROR) ||
             (res.status == LS_E2E_STATUS_WRONG_SEQUENCE))
    {
        post_simple(CGW_EV_E2E_ERROR, res.status, 0u, 0u);
    }
    else
    {
        /* Not yet VALID, repeated, or not in the whitelist. */
    }
}

static void canio_handle(const cgw_canio_event_t *ev, uint32_t now)
{
    switch ((cgw_canio_ev_t)ev->type)
    {
        case CGW_CANIO_EV_RX:
            canio_on_rx(ev, now);
            break;
        case CGW_CANIO_EV_BUS:
        {
            cgw_bus_state_t state = cgw_can_esp_on_state(ev->u.state);

            cgw_com_on_bus_state(&s_com, state, now);
            post_simple(CGW_EV_CAN_BUS, (uint32_t)state, 0u, CTRL_WAIT_MS);
            break;
        }
        case CGW_CANIO_EV_TX_DONE:
            cgw_can_esp_on_tx_done(ev->u.frame);
            break;
        case CGW_CANIO_EV_INTENT:
            cgw_com_set_intent(&s_com, &ev->u.intent);
            break;
        case CGW_CANIO_EV_DOOR:
            cgw_com_request_door(&s_com, ev->u.door.req_id, ev->u.door.request, now);
            break;
        case CGW_CANIO_EV_NODE:
            cgw_com_set_node_info(&s_com, &ev->u.node);
            break;
        default:
            break;
    }
}

static void canio_report_tick(const cgw_com_tick_out_t *out)
{
    for (uint32_t i = 0u; i < (uint32_t)CGW_COM_RX_COUNT; i++)
    {
        if ((out->timeouts & (1u << i)) != 0u)
        {
            post_simple(CGW_EV_RX_TIMEOUT, i, 0u, CTRL_WAIT_MS);
        }
    }
    if (out->busoff_healed)
    {
        post_simple(CGW_EV_BUSOFF_HEALED, 0u, 0u, CTRL_WAIT_MS);
    }
}

static void canio_task(void *arg)
{
    const cgw_can_esp_cfg_t cfg = {s_ao_canio.q, CONFIG_CGW_CAN_TX_GPIO, CONFIG_CGW_CAN_RX_GPIO};
    const cgw_com_version_t ver = {LS_GIT_HASH & 0x0FFFFFFFu, LS_VERSION_MAJOR,
                                   LS_VERSION_MINOR,          LS_VERSION_PATCH,
                                   CONFIG_CGW_BUILD_TYPE,     LS_GIT_DIRTY != 0u};
    cgw_canio_event_t ev;
    cgw_com_tick_out_t out;
    uint32_t wait = 1u;
    bool ok = cgw_can_esp_start(&cfg) == CGW_OK;

    (void)arg;
    (void)esp_task_wdt_add(NULL);
    cgw_com_init(&s_com, &ver, cgw_clock_now_ms());
    if (ok)
    {
        post_simple(CGW_EV_CAN_BUS, (uint32_t)CGW_BUS_ACTIVE, 0u, CTRL_WAIT_MS);
    }
    else
    {
        cgw_wiring_report_dtc(CGW_DTC_TWAI, true);
    }
    for (;;)
    {
        uint32_t now;

        if (xQueueReceive(s_ao_canio.q, &ev, pdMS_TO_TICKS(wait)) == pdTRUE)
        {
            canio_handle(&ev, cgw_clock_now_ms());
        }
        now = cgw_clock_now_ms();
#if CONFIG_CGW_FAULT_INJECTION
        while (cgw_fi_state.canio_hang)
        {
        }
        if (cgw_fi_state.can_silent)
        {
            continue;
        }
#endif
        wait = ok ? cgw_com_tick(&s_com, now, &out) : LS_CAN_CGW_WIN_CMD_CYCLE_MS;
        if (ok)
        {
            canio_report_tick(&out);
        }
        cgw_platform_alive_kick(CGW_ALIVE_CANIO);
        (void)esp_task_wdt_reset();
    }
}

/* ---------------------------------------------------------------------------------- sys task */

static void sys_handle(const sys_msg_t *msg)
{
    if (msg->type == (uint8_t)SYS_MSG_SHOW_QR)
    {
        (void)cgw_hmi_print_qr(s_qr_uri, s_qr_len);
        cgw_crypto_zeroize(s_qr_uri, sizeof(s_qr_uri));
        s_qr_len = 0u;
        atomic_store(&s_qr_busy, false);
    }
    else
    {
        cgw_hmi_set_indication((cgw_indication_t)msg->ind);
    }
}

/* SoftAP start supervision: 3 attempts of t_ap_start_to_ms, then B1B10 (SWR-CGW-004). */
static void sys_supervise_ap(uint32_t now, uint32_t *t_try, uint8_t *tries)
{
    if (!atomic_load(&s_ap_started) && (*tries <= AP_START_TRIES) &&
        ((uint32_t)(now - *t_try) >= LS_T_AP_START_TO_MS))
    {
        (*tries)++;
        *t_try = now;
        if (*tries < AP_START_TRIES)
        {
            (void)cgw_wifi_restart();
        }
        else if (*tries == AP_START_TRIES)
        {
            post_simple(CGW_EV_AP_FAILED, 0u, 0u, CTRL_WAIT_MS);
        }
        else
        {
            /* Reported once. */
        }
    }
}

static void sys_task(void *arg)
{
    sys_msg_t msg;
    uint32_t t_try = cgw_clock_now_ms();
    uint8_t tries = 0u;

    (void)arg;
    (void)esp_task_wdt_add(NULL);
    for (;;)
    {
        uint32_t now;
        cgw_hmi_btn_t btn;

        if (xQueueReceive(s_ao_sys.q, &msg, pdMS_TO_TICKS(SYS_PERIOD_MS)) == pdTRUE)
        {
            sys_handle(&msg);
        }
        now = cgw_clock_now_ms();
        btn = cgw_hmi_poll(now);
        if (btn == CGW_HMI_BTN_PAIR)
        {
            post_simple(CGW_EV_PAIR_REQ, 0u, 0u, CTRL_WAIT_MS);
        }
        else if (btn == CGW_HMI_BTN_FACTORY_RESET)
        {
            post_simple(CGW_EV_FACTORY_RESET, 0u, 0u, CTRL_WAIT_MS);
        }
        else
        {
            /* No gesture. */
        }
        sys_supervise_ap(now, &t_try, &tries);
        if ((now > ALIVE_GRACE_MS) && !cgw_platform_alive_ok(now))
        {
            /* SM-16: a hung core or can_io task resets the CGW; CGW_WinCmd stops on the bus. */
            ESP_LOGE(TAG, "alive supervision failed");
            abort();
        }
        (void)esp_task_wdt_reset();
    }
}

cgw_rc_t cgw_wiring_start(const cgw_core_identity_t *id, const cgw_pairing_record_t *rec)
{
    const cgw_ao_def_t core = {"core",      core_task, NULL,         s_core_stack,
                               &s_core_tcb, s_core_q,  &s_core_qbuf, CORE_STACK,
                               CORE_PRIO,   CPU_RT,    CORE_Q_LEN,   sizeof(cgw_core_event_t)};
    const cgw_ao_def_t canio = {"can_io",     canio_task, NULL,          s_canio_stack,
                                &s_canio_tcb, s_canio_q,  &s_canio_qbuf, CANIO_STACK,
                                CANIO_PRIO,   CPU_RT,     CANIO_Q_LEN,   sizeof(cgw_canio_event_t)};
    const cgw_ao_def_t sys = {"sys",      sys_task, NULL,        s_sys_stack,
                              &s_sys_tcb, s_sys_q,  &s_sys_qbuf, SYS_STACK,
                              SYS_PRIO,   CPU_NET,  SYS_Q_LEN,   sizeof(sys_msg_t)};
    cgw_rc_t rc;

    cgw_core_init(&s_core, id, rec, cgw_clock_now_ms());
    cgw_platform_alive_kick(CGW_ALIVE_CORE);
    cgw_platform_alive_kick(CGW_ALIVE_CANIO);
    rc = cgw_platform_ao_start(&sys, &s_ao_sys);
    if (rc == CGW_OK)
    {
        rc = cgw_platform_ao_start(&core, &s_ao_core);
    }
    if (rc == CGW_OK)
    {
        rc = cgw_platform_ao_start(&canio, &s_ao_canio);
    }
    return rc;
}

/* ------------------------------------------------------------------------------ network side */

static void wifi_event(cgw_wifi_ev_t ev, uint32_t ipv4, uint8_t stations)
{
    if (ev == CGW_WIFI_EV_AP_STARTED)
    {
        /* RF is on: the hardware RNG now delivers true random numbers (SWR-CGW-014). */
        cgw_crypto_psa_enable_rng(true);
        atomic_store(&s_ap_started, true);
        post_simple(CGW_EV_AP_STARTED, 0u, stations, CTRL_WAIT_MS);
    }
    else
    {
        post_simple((ev == CGW_WIFI_EV_STA_JOIN) ? CGW_EV_STA_JOIN : CGW_EV_STA_LEAVE, ipv4,
                    stations, CTRL_WAIT_MS);
    }
}

static cgw_rc_t ws_sink(cgw_ws_ev_t ev, cgw_conn_t conn, uint32_t t_ms, const uint8_t *data,
                        uint16_t len)
{
    static const uint8_t k_type[] = {(uint8_t)CGW_EV_TCP_OPEN, (uint8_t)CGW_EV_WS_OPEN,
                                     (uint8_t)CGW_EV_WS_CLOSE, (uint8_t)CGW_EV_WS_FRAME};
    cgw_core_event_t cev;
    bool frame = (ev == CGW_WS_EV_FRAME);

    cev.type = k_type[ev];
    cev.t_ms = t_ms;
    cev.conn = conn;
    cev.arg = 0u;
    cev.aux = 0u;
    cev.len = 0u;
    if (frame && (data != NULL) && (len <= sizeof(cev.data)))
    {
        (void)memcpy(cev.data, data, len);
        cev.len = len;
    }
    return cgw_wiring_post_core(&cev, frame ? 0u : CTRL_WAIT_MS);
}

cgw_rc_t cgw_wiring_start_network(const cgw_core_identity_t *id)
{
    const cgw_wifi_cfg_t wifi = {
        .ssid = id->ssid,
        .passphrase = id->passphrase,
        .country = CONFIG_CGW_WIFI_COUNTRY,
        .inactive_s = (uint16_t)(LS_T_AP_INACTIVE_MS / 1000u),
        .channel = (uint8_t)CONFIG_CGW_WIFI_CHANNEL,
        .max_conn = (uint8_t)LS_N_WIFI_CLIENTS_MAX,
        .transition = id->wifi_transition,
#if CONFIG_LS_SOFTAP_OFFER_ROUTER
        .offer_router = true,
#else
        .offer_router = false,
#endif
    };
    const cgw_ws_cfg_t ws = {ws_sink, HTTPD_STACK, 80u, HTTPD_PRIO, (uint8_t)CPU_NET};
    cgw_rc_t rc = cgw_ws_start(&ws);

    if (rc == CGW_OK)
    {
        rc = cgw_wifi_start(&wifi, wifi_event);
    }
    return rc;
}
