/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_ws_esp/cgw_ws_esp.h"

#include <stdatomic.h>
#include <string.h>

#include "cgw_ports/cgw_clock_port.h"
#include "cgw_ports/cgw_link_port.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#define WS_URI          "/ws/v1"
#define WS_SUBPROTOCOL  "locksys.v1"
#define WS_CONN_MAX     (CGW_SESSIONS_MAX)
#define WS_TX_SLOTS     (8u)
#define WS_TX_RESERVED  (3u) /* slots kept for control frames; status pushes use the rest */
#define WS_RING_LEN     (8u)
#define WS_POST_WAIT_MS (20u)
#define WS_HDR_MAX      (32u)

static const char *TAG = "cgw_ws";

typedef enum
{
    WS_CONN_FREE = 0,
    WS_CONN_TCP,
    WS_CONN_WS,
    WS_CONN_AUTH
} ws_conn_state_t;

/* Connection table; fields written by the httpd task, the state also by the core task. */
typedef struct
{
    atomic_uint state;
    int fd;
    uint32_t peer_ipv4;
    uint8_t gen;
} ws_conn_t;

typedef struct
{
    uint8_t data[CGW_WS_FRAME_MAX];
    uint16_t len;
    atomic_bool busy;
} ws_slot_t;

/* ws_tx ring entry: a data frame (slot < WS_TX_SLOTS) or a close request (slot == WS_TX_SLOTS). */
typedef struct
{
    cgw_conn_t conn;
    uint16_t code;
    uint8_t slot;
} ws_ring_t;

static httpd_handle_t s_hd;
static cgw_ws_sink_t s_sink;
static ws_conn_t s_conn[WS_CONN_MAX];
static ws_slot_t s_slot[WS_TX_SLOTS];
static ws_ring_t s_ring[WS_RING_LEN];
static atomic_uint s_head; /* written by the core task */
static atomic_uint s_tail; /* written by the httpd task */
static atomic_bool s_drain_queued;
static uint8_t s_gen;
static uint8_t s_rx[CGW_WS_FRAME_MAX]; /* httpd task only */
static cgw_ws_stats_t s_stats;

static cgw_conn_t ws_conn_id(const ws_conn_t *c)
{
    return ((cgw_conn_t)(uint32_t)c->fd << 8) | c->gen;
}

static ws_conn_t *ws_find_fd(int fd)
{
    ws_conn_t *found = NULL;

    for (uint32_t i = 0u; (i < WS_CONN_MAX) && (found == NULL); i++)
    {
        if ((atomic_load(&s_conn[i].state) != WS_CONN_FREE) && (s_conn[i].fd == fd))
        {
            found = &s_conn[i];
        }
    }
    return found;
}

static ws_conn_t *ws_find_conn(cgw_conn_t conn)
{
    ws_conn_t *c = ws_find_fd((int)(conn >> 8));

    return ((c != NULL) && (c->gen == (uint8_t)(conn & 0xFFu))) ? c : NULL;
}

static uint32_t ws_unauthenticated(void)
{
    uint32_t n = 0u;

    for (uint32_t i = 0u; i < WS_CONN_MAX; i++)
    {
        unsigned st = atomic_load(&s_conn[i].state);

        if ((st == WS_CONN_TCP) || (st == WS_CONN_WS))
        {
            n++;
        }
    }
    return n;
}

static uint32_t ws_peer_ipv4(int fd)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    uint32_t ip = 0u;

    /* IPv6 is disabled (CONFIG_LWIP_IPV6=n): peers are IPv4 only. */
    if ((getpeername(fd, (struct sockaddr *)&addr, &len) == 0) && (addr.sin_family == AF_INET))
    {
        ip = addr.sin_addr.s_addr;
    }
    return ip;
}

/* httpd open callback: the single unauthenticated slot protects the controller. */
static esp_err_t ws_on_open(httpd_handle_t hd, int fd)
{
    ws_conn_t *c = NULL;
    esp_err_t err = ESP_FAIL;

    (void)hd;
    for (uint32_t i = 0u; (i < WS_CONN_MAX) && (c == NULL); i++)
    {
        if (atomic_load(&s_conn[i].state) == WS_CONN_FREE)
        {
            c = &s_conn[i];
        }
    }
    if ((c != NULL) && (ws_unauthenticated() == 0u))
    {
        s_gen = (uint8_t)((s_gen == UINT8_MAX) ? 1u : (s_gen + 1u));
        c->fd = fd;
        c->gen = s_gen;
        c->peer_ipv4 = ws_peer_ipv4(fd);
        atomic_store(&c->state, WS_CONN_TCP);
        (void)s_sink(CGW_WS_EV_TCP_OPEN, ws_conn_id(c), cgw_clock_now_ms(), NULL, 0u);
        err = ESP_OK;
    }
    else
    {
        s_stats.refused++;
    }
    return err;
}

static void ws_on_close(httpd_handle_t hd, int fd)
{
    ws_conn_t *c = ws_find_fd(fd);

    (void)hd;
    if (c != NULL)
    {
        (void)s_sink(CGW_WS_EV_CLOSE, ws_conn_id(c), cgw_clock_now_ms(), NULL, 0u);
        atomic_store(&c->state, WS_CONN_FREE);
    }
    (void)close(fd);
}

static void ws_close_with(httpd_req_t *req, uint16_t code)
{
    uint8_t payload[2] = {(uint8_t)(code >> 8), (uint8_t)code};
    httpd_ws_frame_t f = {.final = true,
                          .fragmented = false,
                          .type = HTTPD_WS_TYPE_CLOSE,
                          .payload = payload,
                          .len = sizeof(payload)};

    (void)httpd_ws_send_frame(req, &f);
    (void)httpd_sess_trigger_close(req->handle, httpd_req_to_sockfd(req));
}

/* @satisfies SWR-CGW-003 */
static esp_err_t ws_on_upgrade(httpd_req_t *req, ws_conn_t *c)
{
    char proto[WS_HDR_MAX] = {0};

    if ((httpd_req_get_hdr_value_str(req, "Sec-WebSocket-Protocol", proto, sizeof(proto)) !=
         ESP_OK) ||
        (strcmp(proto, WS_SUBPROTOCOL) != 0))
    {
        ws_close_with(req, CGW_CLOSE_VERSION);
    }
    else
    {
        atomic_store(&c->state, WS_CONN_WS);
        (void)s_sink(CGW_WS_EV_WS_OPEN, ws_conn_id(c), cgw_clock_now_ms(), NULL, 0u);
    }
    return ESP_OK;
}

static esp_err_t ws_on_message(httpd_req_t *req, const ws_conn_t *c, uint32_t t_rx_ms)
{
    httpd_ws_frame_t f = {0};
    esp_err_t err = httpd_ws_recv_frame(req, &f, 0u);

    if (err != ESP_OK)
    {
        /* Socket error: httpd closes the session. */
    }
    else if ((f.type != HTTPD_WS_TYPE_BINARY) || !f.final)
    {
        ws_close_with(req, CGW_CLOSE_UNSUPPORTED_DATA);
    }
    else if ((f.len == 0u) || (f.len > CGW_WS_FRAME_MAX))
    {
        ws_close_with(req, (f.len == 0u) ? CGW_CLOSE_POLICY : CGW_CLOSE_TOO_BIG);
    }
    else
    {
        f.payload = s_rx;
        err = httpd_ws_recv_frame(req, &f, sizeof(s_rx));
        if ((err == ESP_OK) &&
            (s_sink(CGW_WS_EV_FRAME, ws_conn_id(c), t_rx_ms, s_rx, (uint16_t)f.len) != CGW_OK))
        {
            s_stats.rx_dropped++;
        }
    }
    return err;
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    uint32_t t_rx_ms = cgw_clock_now_ms(); /* receipt time used for keep-alive ages */
    ws_conn_t *c = ws_find_fd(httpd_req_to_sockfd(req));
    esp_err_t err = ESP_OK;

    if (c == NULL)
    {
        err = ESP_FAIL;
    }
    else if (req->method == HTTP_GET)
    {
        err = ws_on_upgrade(req, c);
    }
    else
    {
        err = ws_on_message(req, c, t_rx_ms);
    }
    return err;
}

/* Drain work item: runs in the httpd task; the single consumer of the ring. */
static void ws_drain(void *arg)
{
    unsigned tail = atomic_load(&s_tail);

    (void)arg;
    atomic_store(&s_drain_queued, false);
    while (tail != atomic_load(&s_head))
    {
        const ws_ring_t *e = &s_ring[tail % WS_RING_LEN];
        ws_conn_t *c = ws_find_conn(e->conn);
        bool is_ws = (c != NULL) && (atomic_load(&c->state) >= WS_CONN_WS);

        if ((e->slot < WS_TX_SLOTS) && is_ws)
        {
            httpd_ws_frame_t f = {.final = true,
                                  .fragmented = false,
                                  .type = HTTPD_WS_TYPE_BINARY,
                                  .payload = s_slot[e->slot].data,
                                  .len = s_slot[e->slot].len};

            if (httpd_ws_send_frame_async(s_hd, c->fd, &f) != ESP_OK)
            {
                s_stats.tx_errors++;
            }
        }
        else if ((e->slot >= WS_TX_SLOTS) && (c != NULL))
        {
            if (is_ws && (e->code != CGW_CLOSE_NONE))
            {
                uint8_t payload[2] = {(uint8_t)(e->code >> 8), (uint8_t)e->code};
                httpd_ws_frame_t f = {.final = true,
                                      .fragmented = false,
                                      .type = HTTPD_WS_TYPE_CLOSE,
                                      .payload = payload,
                                      .len = sizeof(payload)};

                (void)httpd_ws_send_frame_async(s_hd, c->fd, &f);
            }
            (void)httpd_sess_trigger_close(s_hd, c->fd);
        }
        else
        {
            /* Connection gone or generation changed: frame discarded. */
        }
        if (e->slot < WS_TX_SLOTS)
        {
            atomic_store(&s_slot[e->slot].busy, false);
        }
        tail++;
        atomic_store(&s_tail, tail);
    }
}

void cgw_link_flush(void)
{
    if ((atomic_load(&s_head) != atomic_load(&s_tail)) && !atomic_load(&s_drain_queued) &&
        (s_hd != NULL))
    {
        atomic_store(&s_drain_queued, true);
        if (httpd_queue_work(s_hd, ws_drain, NULL) != ESP_OK)
        {
            atomic_store(&s_drain_queued, false);
            s_stats.drain_retry++;
        }
    }
}

static cgw_rc_t ws_ring_push(cgw_conn_t conn, uint8_t slot, uint16_t code)
{
    cgw_rc_t rc = CGW_E_FULL;
    unsigned head = atomic_load(&s_head);

    if ((head - atomic_load(&s_tail)) < WS_RING_LEN)
    {
        s_ring[head % WS_RING_LEN].conn = conn;
        s_ring[head % WS_RING_LEN].slot = slot;
        s_ring[head % WS_RING_LEN].code = code;
        atomic_store(&s_head, head + 1u);
        cgw_link_flush();
        rc = CGW_OK;
    }
    return rc;
}

/* @satisfies SWR-CGW-043 */
cgw_rc_t cgw_link_send(cgw_conn_t conn, const uint8_t *data, size_t len, cgw_prio_t prio)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((data != NULL) && (len > 0u) && (len <= CGW_WS_FRAME_MAX))
    {
        uint32_t free_slots = 0u;
        uint8_t slot = WS_TX_SLOTS;

        for (uint8_t i = 0u; i < WS_TX_SLOTS; i++)
        {
            if (!atomic_load(&s_slot[i].busy))
            {
                free_slots++;
                slot = (slot == WS_TX_SLOTS) ? i : slot;
            }
        }
        rc = CGW_E_FULL;
        if ((slot < WS_TX_SLOTS) && ((prio == CGW_PRIO_CONTROL) || (free_slots > WS_TX_RESERVED)))
        {
            (void)memcpy(s_slot[slot].data, data, len);
            s_slot[slot].len = (uint16_t)len;
            atomic_store(&s_slot[slot].busy, true);
            rc = ws_ring_push(conn, slot, 0u);
            if (rc != CGW_OK)
            {
                atomic_store(&s_slot[slot].busy, false);
            }
        }
        if (rc != CGW_OK)
        {
            s_stats.tx_full++;
        }
    }
    return rc;
}

void cgw_link_close(cgw_conn_t conn, uint16_t code)
{
    if (ws_ring_push(conn, WS_TX_SLOTS, code) != CGW_OK)
    {
        ws_conn_t *c = ws_find_conn(conn);

        s_stats.tx_full++;
        if ((c != NULL) && (s_hd != NULL))
        {
            (void)httpd_sess_trigger_close(s_hd, c->fd);
        }
    }
}

void cgw_link_set_authenticated(cgw_conn_t conn)
{
    ws_conn_t *c = ws_find_conn(conn);

    if (c != NULL)
    {
        atomic_store(&c->state, WS_CONN_AUTH);
    }
}

cgw_conn_t cgw_link_find_by_peer(uint32_t ipv4)
{
    cgw_conn_t conn = CGW_CONN_NONE;

    for (uint32_t i = 0u; (i < WS_CONN_MAX) && (conn == CGW_CONN_NONE); i++)
    {
        if ((atomic_load(&s_conn[i].state) != WS_CONN_FREE) && (s_conn[i].peer_ipv4 == ipv4) &&
            (ipv4 != 0u))
        {
            conn = ws_conn_id(&s_conn[i]);
        }
    }
    return conn;
}

cgw_rc_t cgw_ws_start(const cgw_ws_cfg_t *cfg)
{
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t uri = {
        .uri = WS_URI,
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = WS_SUBPROTOCOL,
    };
    esp_err_t err;

    s_sink = cfg->sink;
    hc.server_port = cfg->port;
    hc.task_priority = cfg->priority;
    hc.stack_size = cfg->stack_size;
    hc.core_id = cfg->core;
    hc.max_open_sockets = WS_CONN_MAX;
    hc.max_uri_handlers = 1u;
    hc.lru_purge_enable = false;
    hc.keep_alive_enable = false;
    hc.open_fn = ws_on_open;
    hc.close_fn = ws_on_close;
    err = httpd_start(&s_hd, &hc);
    if (err == ESP_OK)
    {
        err = httpd_register_uri_handler(s_hd, &uri);
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(err));
    }
    return (err == ESP_OK) ? CGW_OK : CGW_E_IO;
}

void cgw_ws_get_stats(cgw_ws_stats_t *out)
{
    *out = s_stats;
}
