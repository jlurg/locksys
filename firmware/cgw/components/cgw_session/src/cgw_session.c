/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_session/cgw_session.h"

#include <string.h>

#include "cgw_ports/cgw_rng_port.h"
#include "ls_can_matrix_gen.h"
#include "ls_enums_gen.h"

/* ASCII labels of LS-SAIC-001 section 8.3. */
static const uint8_t k_label_cli[] = {0x4Cu, 0x53u, 0x76u, 0x31u, 0x7Cu, 0x63u, 0x6Cu, 0x69u};
static const uint8_t k_label_srv[] = {0x4Cu, 0x53u, 0x76u, 0x31u, 0x7Cu, 0x73u, 0x72u, 0x76u};
static const uint8_t k_label_session[] = {0x4Cu, 0x53u, 0x76u, 0x31u, 0x7Cu, 0x73u,
                                          0x65u, 0x73u, 0x73u, 0x69u, 0x6Fu, 0x6Eu};

#define SES_PROOF_PARTS (5u)
#define SES_SALT_LEN    (2u * CGW_NONCE_LEN)
#define SES_INFO_LEN    (sizeof(k_label_session) + CGW_DEVICE_ID_LEN + CGW_CLIENT_ID_LEN)

/* Index of the entry of a connection, or CGW_SESSIONS_MAX when not found. */
static size_t ses_index(const cgw_session_table_t *t, cgw_conn_t conn)
{
    size_t idx = CGW_SESSIONS_MAX;

    if ((t != NULL) && (conn != CGW_CONN_NONE))
    {
        for (size_t i = 0u; (i < CGW_SESSIONS_MAX) && (idx == CGW_SESSIONS_MAX); i++)
        {
            if ((t->entries[i].state != CGW_SES_FREE) && (t->entries[i].conn == conn))
            {
                idx = i;
            }
        }
    }
    return idx;
}

static cgw_session_t *ses_lookup(cgw_session_table_t *t, cgw_conn_t conn)
{
    size_t idx = ses_index(t, conn);

    return (idx < CGW_SESSIONS_MAX) ? &t->entries[idx] : NULL;
}

static cgw_session_t *ses_controller_entry(cgw_session_table_t *t)
{
    cgw_session_t *found = NULL;

    for (size_t i = 0u; (i < CGW_SESSIONS_MAX) && (found == NULL); i++)
    {
        if (t->entries[i].state == CGW_SES_AUTH)
        {
            found = &t->entries[i];
        }
    }
    return found;
}

static void ses_release(cgw_session_t *s)
{
    cgw_crypto_key_destroy(s->k_sess);
    cgw_crypto_zeroize(s, sizeof(*s));
    s->state = CGW_SES_FREE;
}

static cgw_ses_verdict_t ses_close(cgw_session_t *s, uint16_t code)
{
    cgw_ses_verdict_t v;

    v.action = CGW_SES_CLOSE;
    v.close_code = code;
    v.controller_lost = (s->state == CGW_SES_AUTH);
    s->state = CGW_SES_CLOSING;
    return v;
}

static void ses_count_failure(cgw_session_table_t *t, uint32_t now_ms)
{
    if ((uint32_t)(now_ms - t->t_last_failure_ms) >= LS_T_AUTH_FAIL_DECAY_MS)
    {
        t->failures = 0u;
    }
    if (t->failures < UINT8_MAX)
    {
        t->failures++;
    }
    t->t_last_failure_ms = now_ms;
    t->stats.auth_failures++;
}

static bool ses_throttled(cgw_session_table_t *t, uint32_t now_ms)
{
    uint32_t since = (uint32_t)(now_ms - t->t_last_failure_ms);

    if (since >= LS_T_AUTH_FAIL_DECAY_MS)
    {
        t->failures = 0u;
    }
    return (t->failures >= LS_N_AUTH_FAIL_THROTTLE) && (since < LS_T_AUTH_THROTTLE_MS);
}

void cgw_session_init(cgw_session_table_t *t)
{
    if (t != NULL)
    {
        (void)memset(t, 0, sizeof(*t));
    }
}

cgw_rc_t cgw_session_on_tcp_open(cgw_session_table_t *t, cgw_conn_t conn, uint32_t t_accept_ms)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((t != NULL) && (conn != CGW_CONN_NONE) && (ses_lookup(t, conn) == NULL))
    {
        cgw_session_t *s = NULL;

        for (size_t i = 0u; (i < CGW_SESSIONS_MAX) && (s == NULL); i++)
        {
            if (t->entries[i].state == CGW_SES_FREE)
            {
                s = &t->entries[i];
            }
        }
        if (s == NULL)
        {
            t->stats.refused++;
            rc = CGW_E_FULL;
        }
        else
        {
            (void)memset(s, 0, sizeof(*s));
            s->conn = conn;
            s->t_accept_ms = t_accept_ms;
            s->state = CGW_SES_TCP;
            rc = CGW_OK;
        }
    }
    return rc;
}

cgw_rc_t cgw_session_on_ws_open(cgw_session_table_t *t, cgw_conn_t conn)
{
    cgw_rc_t rc = CGW_E_STATE;
    cgw_session_t *s = ses_lookup(t, conn);

    if ((s != NULL) && (s->state == CGW_SES_TCP))
    {
        s->state = CGW_SES_OPEN;
        rc = CGW_OK;
    }
    return rc;
}

bool cgw_session_on_close(cgw_session_table_t *t, cgw_conn_t conn)
{
    bool controller = false;
    cgw_session_t *s = ses_lookup(t, conn);

    if (s != NULL)
    {
        controller = (s->state == CGW_SES_AUTH);
        ses_release(s);
    }
    return controller;
}

void cgw_session_mark_closing(cgw_session_table_t *t, cgw_conn_t conn)
{
    cgw_session_t *s = ses_lookup(t, conn);

    if (s != NULL)
    {
        s->state = CGW_SES_CLOSING;
    }
}

const cgw_session_t *cgw_session_find(const cgw_session_table_t *t, cgw_conn_t conn)
{
    size_t idx = ses_index(t, conn);

    return (idx < CGW_SESSIONS_MAX) ? &t->entries[idx] : NULL;
}

cgw_conn_t cgw_session_controller(const cgw_session_table_t *t)
{
    cgw_conn_t conn = CGW_CONN_NONE;

    if (t != NULL)
    {
        for (size_t i = 0u; (i < CGW_SESSIONS_MAX) && (conn == CGW_CONN_NONE); i++)
        {
            if (t->entries[i].state == CGW_SES_AUTH)
            {
                conn = t->entries[i].conn;
            }
        }
    }
    return conn;
}

/* @satisfies SWR-CGW-006 */
bool cgw_session_counter_is_valid(const cgw_session_t *s, uint32_t counter)
{
    return (s != NULL) && (counter != 0u) && (counter > s->rx_counter);
}

/* @satisfies SWR-CGW-011 */
bool cgw_session_rate_admit(cgw_rate_window_t *w, uint32_t t_rx_ms)
{
    bool admitted = (w != NULL);

    if (admitted && (w->count >= LS_N_RATE_LIMIT_FRAMES))
    {
        /* When the ring is full, the next write position holds the oldest receipt time. */
        admitted = (uint32_t)(t_rx_ms - w->t_ms[w->next]) >= LS_T_RATE_WINDOW_MS;
    }
    if (admitted)
    {
        w->t_ms[w->next] = t_rx_ms;
        w->next = (uint8_t)((w->next + 1u) % LS_N_RATE_LIMIT_FRAMES);
        if (w->count < LS_N_RATE_LIMIT_FRAMES)
        {
            w->count++;
        }
    }
    return admitted;
}

static bool ses_tag_ok(const cgw_session_t *s, const cgw_proto_frame_t *frame)
{
    cgw_tag_input_t in;

    in.body = frame->body;
    in.body_len = frame->body_len;
    in.counter = frame->counter;
    in.dir = CGW_DIR_APP_TO_CGW;
    return (frame->tag_len == CGW_TAG_LEN) &&
           (cgw_crypto_tag_verify(s->k_sess, &in, frame->tag, frame->tag_len) == CGW_OK);
}

/* @satisfies SWR-CGW-006 */
static cgw_ses_verdict_t ses_check_auth_frame(cgw_session_table_t *t, cgw_session_t *s,
                                              const cgw_proto_frame_t *frame, uint32_t t_rx_ms)
{
    cgw_ses_verdict_t v = {CGW_SES_DELIVER, CGW_CLOSE_NONE, false};

    if (!cgw_session_rate_admit(&s->rate, t_rx_ms))
    {
        t->stats.rate_closes++;
        v = ses_close(s, CGW_CLOSE_POLICY);
    }
    else if (!cgw_session_counter_is_valid(s, frame->counter))
    {
        t->stats.replays++;
        v = ses_close(s, CGW_CLOSE_POLICY);
    }
    else if (!ses_tag_ok(s, frame))
    {
        t->stats.bad_tags++;
        v = ses_close(s, CGW_CLOSE_POLICY);
    }
    else
    {
        s->rx_counter = frame->counter;
        s->t_last_rx_ms = t_rx_ms;
    }
    return v;
}

static cgw_ses_verdict_t ses_check_handshake_frame(cgw_session_table_t *t, cgw_session_t *s,
                                                   const cgw_proto_frame_t *frame, uint32_t t_rx_ms)
{
    cgw_ses_verdict_t v = {CGW_SES_HANDSHAKE, CGW_CLOSE_NONE, false};

    if (!cgw_session_rate_admit(&s->rate, t_rx_ms))
    {
        t->stats.rate_closes++;
        v = ses_close(s, CGW_CLOSE_POLICY);
    }
    else if ((s->state != CGW_SES_HELLO_SENT) || (frame->counter != 0u) || (frame->tag_len != 0u))
    {
        ses_count_failure(t, t_rx_ms);
        v = ses_close(s, CGW_CLOSE_POLICY);
    }
    else
    {
        /* Handshake frame: the caller decodes the ClientAuth. */
    }
    return v;
}

cgw_ses_verdict_t cgw_session_on_frame(cgw_session_table_t *t, cgw_conn_t conn,
                                       const cgw_proto_frame_t *frame, uint32_t t_rx_ms)
{
    cgw_ses_verdict_t v = {CGW_SES_DROP, CGW_CLOSE_NONE, false};
    cgw_session_t *s = ses_lookup(t, conn);

    if ((s != NULL) && (frame != NULL))
    {
        if (s->state == CGW_SES_AUTH)
        {
            v = ses_check_auth_frame(t, s, frame, t_rx_ms);
        }
        else if ((s->state == CGW_SES_OPEN) || (s->state == CGW_SES_HELLO_SENT))
        {
            v = ses_check_handshake_frame(t, s, frame, t_rx_ms);
        }
        else
        {
            /* TCP (not upgraded) or CLOSING: drop. */
        }
    }
    return v;
}

cgw_ses_verdict_t cgw_session_on_violation(cgw_session_table_t *t, cgw_conn_t conn, uint32_t now_ms)
{
    cgw_ses_verdict_t v = {CGW_SES_DROP, CGW_CLOSE_NONE, false};
    cgw_session_t *s = ses_lookup(t, conn);

    if ((s != NULL) && (s->state != CGW_SES_CLOSING))
    {
        if (s->state != CGW_SES_AUTH)
        {
            ses_count_failure(t, now_ms);
        }
        v = ses_close(s, CGW_CLOSE_POLICY);
    }
    return v;
}

static void ses_copy_string(char *dst, size_t size, const char *src)
{
    size_t n = 0u;

    if (src != NULL)
    {
        while ((n + 1u < size) && (src[n] != '\0'))
        {
            dst[n] = src[n];
            n++;
        }
    }
    dst[n] = '\0';
}

cgw_rc_t cgw_session_build_hello(cgw_session_table_t *t, cgw_conn_t conn,
                                 const cgw_ses_identity_t *id, locksys_app_v1_ServerHello *hello)
{
    cgw_rc_t rc = CGW_E_ARG;
    cgw_session_t *s = ses_lookup(t, conn);

    if ((id == NULL) || (id->device_id == NULL) || (hello == NULL))
    {
        rc = CGW_E_ARG;
    }
    else if ((s == NULL) || (s->state != CGW_SES_OPEN))
    {
        rc = CGW_E_STATE;
    }
    else
    {
        rc = cgw_rng_fill(s->server_nonce, sizeof(s->server_nonce));
    }
    if (rc == CGW_OK)
    {
        (void)memset(hello, 0, sizeof(*hello));
        hello->has_proto = true;
        hello->proto.major = CGW_PROTO_MAJOR;
        hello->proto.minor = CGW_PROTO_MINOR;
        (void)memcpy(hello->device_id, id->device_id, CGW_DEVICE_ID_LEN);
        (void)memcpy(hello->server_nonce, s->server_nonce, CGW_NONCE_LEN);
        ses_copy_string(hello->fw_version, sizeof(hello->fw_version), id->fw_version);
        hello->has_com_matrix = true;
        hello->com_matrix.major = LS_CAN_MATRIX_VERSION_MAJOR;
        hello->com_matrix.minor = LS_CAN_MATRIX_VERSION_MINOR;
        hello->pairing_window_open = id->pairing_window_open;
        s->state = CGW_SES_HELLO_SENT;
    }
    return rc;
}

static bool ses_proof_matches(const uint8_t *kpair, const cgw_session_t *s,
                              const locksys_app_v1_ClientAuth *ca, const uint8_t *device_id,
                              cgw_key_t *key_out)
{
    bool match = false;
    cgw_key_t key = CGW_KEY_NONE;

    if ((kpair != NULL) && (cgw_crypto_import_hmac_key(kpair, CGW_KEY_LEN, &key) == CGW_OK))
    {
        const cgw_buf_t parts[SES_PROOF_PARTS] = {
            {k_label_cli, sizeof(k_label_cli)}, {device_id, CGW_DEVICE_ID_LEN},
            {s->server_nonce, CGW_NONCE_LEN},   {ca->client_nonce, CGW_NONCE_LEN},
            {ca->client_id, CGW_CLIENT_ID_LEN},
        };

        match = cgw_crypto_hmac_verify(key, parts, SES_PROOF_PARTS, ca->client_proof,
                                       sizeof(ca->client_proof)) == CGW_OK;
        if (match)
        {
            *key_out = key;
        }
        else
        {
            cgw_crypto_key_destroy(key);
        }
    }
    return match;
}

static cgw_rc_t ses_derive(const cgw_session_t *s, const locksys_app_v1_ClientAuth *ca,
                           const uint8_t *device_id, const uint8_t *kpair, cgw_key_t *k_sess)
{
    uint8_t salt[SES_SALT_LEN];
    uint8_t info[SES_INFO_LEN];
    cgw_buf_t salt_buf = {salt, sizeof(salt)};
    cgw_buf_t info_buf = {info, sizeof(info)};
    cgw_rc_t rc;

    (void)memcpy(salt, s->server_nonce, CGW_NONCE_LEN);
    (void)memcpy(&salt[CGW_NONCE_LEN], ca->client_nonce, CGW_NONCE_LEN);
    (void)memcpy(info, k_label_session, sizeof(k_label_session));
    (void)memcpy(&info[sizeof(k_label_session)], device_id, CGW_DEVICE_ID_LEN);
    (void)memcpy(&info[sizeof(k_label_session) + CGW_DEVICE_ID_LEN], ca->client_id,
                 CGW_CLIENT_ID_LEN);
    rc = cgw_crypto_derive_session_key(kpair, CGW_KEY_LEN, &salt_buf, &info_buf, k_sess);
    return rc;
}

static cgw_rc_t ses_server_proof(const cgw_session_t *s, const locksys_app_v1_ClientAuth *ca,
                                 const uint8_t *device_id, cgw_key_t key,
                                 uint8_t proof[CGW_HMAC_LEN])
{
    const cgw_buf_t parts[SES_PROOF_PARTS] = {
        {k_label_srv, sizeof(k_label_srv)}, {device_id, CGW_DEVICE_ID_LEN},
        {ca->client_nonce, CGW_NONCE_LEN},  {s->server_nonce, CGW_NONCE_LEN},
        {ca->client_id, CGW_CLIENT_ID_LEN},
    };

    return cgw_crypto_hmac(key, parts, SES_PROOF_PARTS, proof);
}

static void ses_activate(cgw_session_t *s, const locksys_app_v1_ClientAuth *ca, cgw_key_t k_sess,
                         uint32_t now_ms, const cgw_ses_auth_t *out)
{
    s->state = CGW_SES_AUTH;
    s->k_sess = k_sess;
    s->session_id = out->session_id;
    s->pending_key = out->used_pending_key;
    s->rx_counter = 0u;
    s->tx_counter = 0u;
    s->t_last_rx_ms = now_ms;
    (void)memcpy(s->client_id, ca->client_id, CGW_CLIENT_ID_LEN);
    (void)memset(&s->ping, 0, sizeof(s->ping));
}

/* Inputs and output of one ClientAuth verification. */
typedef struct
{
    cgw_session_table_t *t;
    cgw_session_t *s;
    const locksys_app_v1_ClientAuth *ca;
    const cgw_ses_auth_input_t *in;
    cgw_ses_auth_t *out;
    uint32_t now_ms;
} ses_auth_ctx_t;

static void ses_establish(const ses_auth_ctx_t *c, const uint8_t *kpair, cgw_key_t proof_key)
{
    cgw_key_t k_sess = CGW_KEY_NONE;
    uint8_t sid[sizeof(uint32_t)] = {0u};
    cgw_rc_t rc = ses_derive(c->s, c->ca, c->in->device_id, kpair, &k_sess);

    if (rc == CGW_OK)
    {
        rc = ses_server_proof(c->s, c->ca, c->in->device_id, proof_key, c->out->server_proof);
    }
    if (rc == CGW_OK)
    {
        rc = cgw_rng_fill(sid, sizeof(sid));
    }
    if (rc == CGW_OK)
    {
        c->out->session_id = ((uint32_t)sid[0] << 24) | ((uint32_t)sid[1] << 16) |
                             ((uint32_t)sid[2] << 8) | (uint32_t)sid[3];
        c->out->result = LS_COMMAND_RESULT_OK;
        c->out->close_code = CGW_CLOSE_NONE;
        ses_activate(c->s, c->ca, k_sess, c->now_ms, c->out);
    }
    else
    {
        cgw_crypto_key_destroy(k_sess);
        cgw_crypto_zeroize(c->out->server_proof, sizeof(c->out->server_proof));
        c->out->result = LS_COMMAND_RESULT_UNSPECIFIED;
        c->out->close_code = CGW_CLOSE_INTERNAL;
    }
}

static bool ses_busy(const cgw_session_t *ctrl, const locksys_app_v1_ClientAuth *ca,
                     bool pending_key, uint32_t now_ms)
{
    bool busy = false;

    if ((ctrl != NULL) && !pending_key &&
        (memcmp(ctrl->client_id, ca->client_id, CGW_CLIENT_ID_LEN) != 0))
    {
        busy = (uint32_t)(now_ms - ctrl->t_last_rx_ms) < LS_T_SESSION_TO_MS;
    }
    return busy;
}

/* @satisfies SWR-CGW-005 */
/* @satisfies SWR-CGW-009 */
static void ses_authenticate(const ses_auth_ctx_t *c)
{
    cgw_key_t key = CGW_KEY_NONE;
    const uint8_t *kpair = NULL;

    if (ses_proof_matches(c->in->pending_kpair, c->s, c->ca, c->in->device_id, &key))
    {
        kpair = c->in->pending_kpair;
        c->out->used_pending_key = true;
    }
    else if (ses_proof_matches(c->in->current_kpair, c->s, c->ca, c->in->device_id, &key))
    {
        kpair = c->in->current_kpair;
    }
    else
    {
        ses_count_failure(c->t, c->now_ms);
        c->out->result = LS_COMMAND_RESULT_REJECTED_AUTH;
        c->out->close_code = CGW_CLOSE_AUTH_FAILED;
    }

    if (kpair != NULL)
    {
        cgw_session_t *ctrl = ses_controller_entry(c->t);

        if (ses_busy(ctrl, c->ca, c->out->used_pending_key, c->now_ms))
        {
            c->out->result = LS_COMMAND_RESULT_REJECTED_BUSY;
            c->out->close_code = CGW_CLOSE_BUSY;
        }
        else
        {
            if (ctrl != NULL)
            {
                /* Pre-emption: the caller sends SessionClose, closes with 4004, latches STOP. */
                c->out->preempted = ctrl->conn;
                ctrl->state = CGW_SES_CLOSING;
            }
            ses_establish(c, kpair, key);
        }
    }
    cgw_crypto_key_destroy(key);
}

/* @satisfies SWR-CGW-007 */
/* @satisfies SWR-CGW-008 */
cgw_ses_auth_t cgw_session_on_client_auth(cgw_session_table_t *t, cgw_conn_t conn,
                                          const locksys_app_v1_ClientAuth *ca,
                                          const cgw_ses_auth_input_t *in, uint32_t now_ms)
{
    cgw_ses_auth_t out;
    cgw_session_t *s = ses_lookup(t, conn);

    (void)memset(&out, 0, sizeof(out));
    out.result = LS_COMMAND_RESULT_UNSPECIFIED;

    if ((s == NULL) || (ca == NULL) || (in == NULL) || (in->device_id == NULL) ||
        (s->state != CGW_SES_HELLO_SENT))
    {
        out.close_code = CGW_CLOSE_POLICY;
    }
    else if (!ca->has_proto || (ca->proto.major != CGW_PROTO_MAJOR))
    {
        out.result = LS_COMMAND_RESULT_REJECTED_VERSION;
        out.close_code = CGW_CLOSE_VERSION;
    }
    else if (ses_throttled(t, now_ms))
    {
        t->stats.throttled++;
        out.result = LS_COMMAND_RESULT_REJECTED_RATE_LIMIT;
        out.close_code = CGW_CLOSE_HANDSHAKE_TIMEOUT;
    }
    else if ((in->pending_kpair == NULL) && (in->current_kpair == NULL))
    {
        out.close_code = CGW_CLOSE_PAIRING_REQUIRED;
    }
    else
    {
        const ses_auth_ctx_t ctx = {t, s, ca, in, &out, now_ms};

        ses_authenticate(&ctx);
    }

    if ((s != NULL) && (out.close_code != CGW_CLOSE_NONE))
    {
        s->state = CGW_SES_CLOSING;
    }
    return out;
}

cgw_rc_t cgw_session_seal(cgw_session_table_t *t, cgw_conn_t conn, const uint8_t *body,
                          size_t body_len, cgw_proto_frame_t *out)
{
    cgw_rc_t rc = CGW_E_ARG;
    cgw_session_t *s = ses_lookup(t, conn);

    if ((body == NULL) || (out == NULL) || (body_len == 0u) || (body_len > CGW_PROTO_BODY_MAX))
    {
        rc = CGW_E_ARG;
    }
    else if ((s == NULL) || (s->k_sess == CGW_KEY_NONE) || (s->tx_counter == UINT32_MAX) ||
             ((s->state != CGW_SES_AUTH) && (s->state != CGW_SES_CLOSING)))
    {
        rc = CGW_E_STATE;
    }
    else
    {
        cgw_tag_input_t in;

        in.body = body;
        in.body_len = body_len;
        in.counter = s->tx_counter + 1u;
        in.dir = CGW_DIR_CGW_TO_APP;
        rc = cgw_crypto_tag_compute(s->k_sess, &in, out->tag);
        if (rc == CGW_OK)
        {
            s->tx_counter = in.counter;
            out->counter = in.counter;
            (void)memcpy(out->body, body, body_len);
            out->body_len = (uint16_t)body_len;
            out->tag_len = (uint8_t)CGW_TAG_LEN;
        }
    }
    return rc;
}

static bool ses_tick_entry(cgw_session_table_t *t, cgw_session_t *s, uint32_t now_ms,
                           cgw_ses_close_t *req)
{
    bool closing = false;

    if ((s->state == CGW_SES_TCP) || (s->state == CGW_SES_OPEN) || (s->state == CGW_SES_HELLO_SENT))
    {
        if ((uint32_t)(now_ms - s->t_accept_ms) >= LS_T_CGW_HANDSHAKE_TO_MS)
        {
            ses_count_failure(t, now_ms);
            req->close_code =
                (s->state == CGW_SES_TCP) ? CGW_CLOSE_NONE : CGW_CLOSE_HANDSHAKE_TIMEOUT;
            req->controller_lost = false;
            req->send_session_close = false;
            closing = true;
        }
    }
    else if (s->state == CGW_SES_AUTH)
    {
        if ((uint32_t)(now_ms - s->t_last_rx_ms) >= LS_T_SESSION_TO_MS)
        {
            req->close_code = CGW_CLOSE_SESSION_TIMEOUT;
            req->controller_lost = true;
            req->send_session_close = true;
            closing = true;
        }
    }
    else
    {
        /* FREE or CLOSING: nothing to supervise. */
    }
    if (closing)
    {
        req->conn = s->conn;
        s->state = CGW_SES_CLOSING;
    }
    return closing;
}

/* @satisfies SWR-CGW-005 */
/* @satisfies SWR-CGW-010 */
void cgw_session_tick(cgw_session_table_t *t, uint32_t now_ms, cgw_ses_tick_result_t *out)
{
    if (out != NULL)
    {
        out->count = 0u;
        if (t != NULL)
        {
            for (size_t i = 0u; i < CGW_SESSIONS_MAX; i++)
            {
                if (ses_tick_entry(t, &t->entries[i], now_ms, &out->closes[out->count]))
                {
                    out->count++;
                }
            }
        }
    }
}

/* @satisfies SWR-CGW-023 */
bool cgw_session_ping_due(cgw_session_table_t *t, uint32_t now_ms, bool moving)
{
    bool due = false;
    cgw_session_t *s = (t != NULL) ? ses_controller_entry(t) : NULL;

    if (s != NULL)
    {
        cgw_ses_ping_t *p = &s->ping;
        uint32_t period = moving ? LS_T_PING_MOTION_MS : LS_T_PING_IDLE_MS;

        if (p->pending && ((uint32_t)(now_ms - p->t_sent_ms) >= LS_T_PONG_TO_MS))
        {
            p->pending = false;
            p->missed = true;
        }
        if (!p->pending && (!p->sent_any || ((uint32_t)(now_ms - p->t_sent_ms) >= period)))
        {
            p->pending = true;
            p->sent_any = true;
            p->t_sent_ms = now_ms;
            due = true;
        }
    }
    return due;
}

void cgw_session_on_pong(cgw_session_table_t *t, cgw_conn_t conn, uint32_t echo_ms,
                         uint32_t t_rx_ms)
{
    cgw_session_t *s = ses_lookup(t, conn);

    if ((s != NULL) && (s->state == CGW_SES_AUTH) && s->ping.pending &&
        (echo_ms == s->ping.t_sent_ms))
    {
        s->ping.rtt_ms = (uint32_t)(t_rx_ms - s->ping.t_sent_ms);
        s->ping.t_sample_ms = t_rx_ms;
        s->ping.pending = false;
        s->ping.sampled = true;
        s->ping.missed = false;
    }
}

bool cgw_session_link_ok(const cgw_session_table_t *t, uint32_t now_ms)
{
    bool ok = false;
    const cgw_session_t *s = cgw_session_find(t, cgw_session_controller(t));

    if (s != NULL)
    {
        const cgw_ses_ping_t *p = &s->ping;
        bool overdue = p->pending && ((uint32_t)(now_ms - p->t_sent_ms) >= LS_T_PONG_TO_MS);

        ok = p->sampled && !p->missed && !overdue && (p->rtt_ms <= LS_T_RTT_MAX_MS) &&
             ((uint32_t)(now_ms - p->t_sample_ms) <= LS_T_RTT_SAMPLE_MAX_AGE_MS);
    }
    return ok;
}
