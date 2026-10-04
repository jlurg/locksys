/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "fake_ports.h"

#include <string.h>

#include "cgw_ports/cgw_canio_port.h"
#include "cgw_ports/cgw_console_port.h"
#include "cgw_ports/cgw_link_port.h"
#include "cgw_ports/cgw_rng_port.h"
#include "cgw_ports/cgw_system_port.h"

fake_ports_t fake;

void fake_ports_reset(void)
{
    (void)memset(&fake, 0, sizeof(fake));
    fake.rng_enabled = true;
    for (size_t i = 0u; i < sizeof(fake.rng); i++)
    {
        fake.rng[i] = (uint8_t)(i + 1u);
    }
    fake.rng_len = sizeof(fake.rng);
}

void fake_rng_script(const uint8_t *bytes, size_t len)
{
    (void)memcpy(fake.rng, bytes, len);
    fake.rng_len = len;
    fake.rng_pos = 0u;
}

static uint8_t fake_nibble(char c)
{
    uint8_t v = 0u;

    if ((c >= '0') && (c <= '9'))
    {
        v = (uint8_t)(c - '0');
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        v = (uint8_t)(c - 'a' + 10);
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        v = (uint8_t)(c - 'A' + 10);
    }
    return v;
}

size_t fake_hex(const char *hex, uint8_t *out, size_t max)
{
    size_t n = 0u;

    while ((hex[2u * n] != '\0') && (hex[(2u * n) + 1u] != '\0') && (n < max))
    {
        out[n] = (uint8_t)((fake_nibble(hex[2u * n]) << 4) | fake_nibble(hex[(2u * n) + 1u]));
        n++;
    }
    return n;
}

cgw_rc_t cgw_rng_fill(uint8_t *buf, size_t len)
{
    cgw_rc_t rc = CGW_E_NO_ENTROPY;

    if (fake.rng_enabled)
    {
        for (size_t i = 0u; i < len; i++)
        {
            buf[i] = fake.rng[fake.rng_pos];
            fake.rng_pos = (fake.rng_pos + 1u) % fake.rng_len;
        }
        rc = CGW_OK;
    }
    return rc;
}

cgw_rc_t cgw_link_send(cgw_conn_t conn, const uint8_t *data, size_t len, cgw_prio_t prio)
{
    if ((fake.link_rc == CGW_OK) && (fake.link_count < FAKE_LINK_MAX))
    {
        fake_link_msg_t *m = &fake.link[fake.link_count];

        m->conn = conn;
        (void)memcpy(m->data, data, len);
        m->len = len;
        m->prio = prio;
        fake.link_count++;
    }
    return fake.link_rc;
}

void cgw_link_close(cgw_conn_t conn, uint16_t code)
{
    if (fake.close_count < FAKE_CLOSE_MAX)
    {
        fake.closed[fake.close_count] = conn;
        fake.close_code[fake.close_count] = code;
        fake.close_count++;
    }
}

void cgw_link_set_authenticated(cgw_conn_t conn)
{
    fake.authenticated = conn;
}

cgw_conn_t cgw_link_find_by_peer(uint32_t ipv4)
{
    return (ipv4 != 0u) ? fake.peer_conn : CGW_CONN_NONE;
}

void cgw_link_flush(void)
{
    fake.flushes++;
}

cgw_rc_t cgw_canio_post_intent(const cgw_win_intent_t *intent)
{
    if (fake.canio_rc == CGW_OK)
    {
        fake.intent = *intent;
        fake.intent_count++;
    }
    return fake.canio_rc;
}

cgw_rc_t cgw_canio_post_door_request(uint8_t req_id, uint8_t request)
{
    fake.door_req_id = req_id;
    fake.door_request = request;
    fake.door_count++;
    return CGW_OK;
}

cgw_rc_t cgw_canio_post_node_info(const cgw_node_info_t *info)
{
    if (fake.canio_rc == CGW_OK)
    {
        fake.node = *info;
        fake.node_count++;
    }
    return fake.canio_rc;
}

cgw_rc_t cgw_can_tx(uint16_t id, const uint8_t *data, uint8_t dlc)
{
    if ((fake.can_rc == CGW_OK) && (fake.can_count < FAKE_CAN_MAX))
    {
        fake_can_frame_t *f = &fake.can[fake.can_count];

        f->id = id;
        (void)memcpy(f->data, data, dlc);
        f->dlc = dlc;
        fake.can_count++;
    }
    return fake.can_rc;
}

cgw_rc_t cgw_can_recover(void)
{
    fake.can_recover++;
    return CGW_OK;
}

uint8_t cgw_can_rewrite_busy(uint16_t id, const uint8_t frames[][CGW_CAN_DLC_MAX], uint8_t n_frames,
                             uint8_t dlc)
{
    uint8_t n = (fake.can_busy < n_frames) ? fake.can_busy : n_frames;

    (void)id;
    for (uint8_t i = 0u; (i < n) && (i < 2u); i++)
    {
        (void)memcpy(fake.can_rewritten[i], frames[i], dlc);
    }
    return n;
}

uint8_t cgw_can_reclaim_suspect(void)
{
    return fake.can_reclaim;
}

cgw_rc_t cgw_console_show_pairing_qr(const char *uri, size_t len)
{
    (void)memcpy(fake.qr, uri, len);
    fake.qr[len] = '\0';
    fake.qr_len = len;
    fake.qr_count++;
    return CGW_OK;
}

void cgw_console_set_indication(cgw_indication_t ind)
{
    fake.indication = ind;
}

void cgw_trace_event(cgw_trace_event_t ev)
{
    fake.trace_events[ev]++;
}

void cgw_trace_set(cgw_trace_pin_t pin, bool level)
{
    if (level && !fake.trace_level[pin])
    {
        fake.trace_pulses[pin]++;
    }
    fake.trace_level[pin] = level;
}

cgw_rc_t cgw_keystore_load_pairing(cgw_pairing_record_t *rec)
{
    cgw_rc_t rc = fake.ks_has_rec ? CGW_OK : CGW_E_NOT_FOUND;

    if (rc == CGW_OK)
    {
        *rec = fake.ks_rec;
    }
    return rc;
}

cgw_rc_t cgw_keystore_store_pairing(const cgw_pairing_record_t *rec)
{
    if (fake.ks_rc == CGW_OK)
    {
        fake.ks_rec = *rec;
        fake.ks_has_rec = true;
    }
    return fake.ks_rc;
}

cgw_rc_t cgw_keystore_load_passphrase(char *buf, size_t size)
{
    cgw_rc_t rc = fake.ks_has_pass ? CGW_OK : CGW_E_NOT_FOUND;

    if ((rc == CGW_OK) && (size > CGW_PASSPHRASE_LEN))
    {
        (void)memcpy(buf, fake.ks_pass, sizeof(fake.ks_pass));
    }
    return rc;
}

cgw_rc_t cgw_keystore_store_passphrase(const char *passphrase)
{
    if (fake.ks_rc == CGW_OK)
    {
        (void)memcpy(fake.ks_pass, passphrase, CGW_PASSPHRASE_LEN);
        fake.ks_pass[CGW_PASSPHRASE_LEN] = '\0';
        fake.ks_has_pass = true;
    }
    return fake.ks_rc;
}

cgw_rc_t cgw_keystore_erase(void)
{
    fake.ks_erase++;
    fake.ks_has_rec = false;
    fake.ks_has_pass = false;
    return fake.ks_rc;
}

void cgw_system_restart(void)
{
    fake.restarts++;
}
