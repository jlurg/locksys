/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_pairing/cgw_pairing.h"

#include <string.h>

#include "cgw_ports/cgw_crypto_port.h"
#include "cgw_ports/cgw_rng_port.h"
#include "ls_params_gen.h"

static const char k_base32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static const char k_base64url[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
static const char k_hex[] = "0123456789ABCDEF";

/* Bounded string builder. */
typedef struct
{
    char *buf;
    size_t size;
    size_t len;
    bool overflow;
} pair_sb_t;

static void sb_put(pair_sb_t *sb, char c)
{
    if ((sb->len + 1u) < sb->size)
    {
        sb->buf[sb->len] = c;
        sb->len++;
        sb->buf[sb->len] = '\0';
    }
    else
    {
        sb->overflow = true;
    }
}

static void sb_puts(pair_sb_t *sb, const char *s)
{
    for (size_t i = 0u; s[i] != '\0'; i++)
    {
        sb_put(sb, s[i]);
    }
}

static void sb_hex(pair_sb_t *sb, const uint8_t *data, size_t len, char sep)
{
    for (size_t i = 0u; i < len; i++)
    {
        if ((sep != '\0') && (i > 0u))
        {
            sb_put(sb, sep);
        }
        sb_put(sb, k_hex[(data[i] >> 4) & 0x0Fu]);
        sb_put(sb, k_hex[data[i] & 0x0Fu]);
    }
}

void cgw_pairing_init(cgw_pairing_t *p)
{
    (void)memset(p, 0, sizeof(*p));
}

/* @satisfies SWR-CGW-016 */
cgw_rc_t cgw_pairing_open(cgw_pairing_t *p, uint32_t now_ms)
{
    cgw_rc_t rc = CGW_E_STATE;

    if (!p->open)
    {
        rc = cgw_rng_fill(p->pending, sizeof(p->pending));
        if (rc == CGW_OK)
        {
            p->open = true;
            p->t_open_ms = now_ms;
        }
        else
        {
            cgw_crypto_zeroize(p->pending, sizeof(p->pending));
        }
    }
    return rc;
}

bool cgw_pairing_is_open(const cgw_pairing_t *p)
{
    return p->open;
}

const uint8_t *cgw_pairing_pending_key(const cgw_pairing_t *p)
{
    return p->open ? p->pending : NULL;
}

void cgw_pairing_close(cgw_pairing_t *p)
{
    cgw_crypto_zeroize(p->pending, sizeof(p->pending));
    p->open = false;
}

bool cgw_pairing_tick(cgw_pairing_t *p, uint32_t now_ms)
{
    bool expired = p->open && ((uint32_t)(now_ms - p->t_open_ms) >= LS_T_PAIRING_WINDOW_MS);

    if (expired)
    {
        cgw_pairing_close(p);
    }
    return expired;
}

/* @satisfies SWR-CGW-018 */
cgw_rc_t cgw_pairing_commit(cgw_pairing_t *p, const uint8_t *client_id, cgw_pairing_record_t *rec)
{
    cgw_rc_t rc = CGW_E_STATE;

    if (p->open && (client_id != NULL) && (rec != NULL))
    {
        (void)memcpy(rec->k_pair, p->pending, CGW_KEY_LEN);
        (void)memcpy(rec->client_id, client_id, CGW_CLIENT_ID_LEN);
        rec->generation++;
        rc = (cgw_keystore_store_pairing(rec) == CGW_OK) ? CGW_OK : CGW_E_IO;
        cgw_pairing_close(p);
    }
    return rc;
}

size_t cgw_pairing_base64url(const uint8_t *in, size_t len, char *out, size_t size)
{
    pair_sb_t sb = {out, size, 0u, false};
    uint32_t acc = 0u;
    uint32_t bits = 0u;

    if ((out == NULL) || (size == 0u) || ((in == NULL) && (len > 0u)))
    {
        return 0u;
    }
    out[0] = '\0';
    for (size_t i = 0u; i < len; i++)
    {
        acc = ((acc << 8) | in[i]) & 0xFFFFu;
        bits += 8u;
        while (bits >= 6u)
        {
            bits -= 6u;
            sb_put(&sb, k_base64url[(acc >> bits) & 0x3Fu]);
        }
    }
    if (bits > 0u)
    {
        sb_put(&sb, k_base64url[(acc << (6u - bits)) & 0x3Fu]);
    }
    return sb.overflow ? 0u : sb.len;
}

/* @satisfies SWR-CGW-002 */
cgw_rc_t cgw_pairing_make_passphrase(char *out, size_t size)
{
    uint8_t rnd[CGW_PASSPHRASE_LEN];
    cgw_rc_t rc = CGW_E_ARG;

    if ((out != NULL) && (size > CGW_PASSPHRASE_LEN))
    {
        rc = cgw_rng_fill(rnd, sizeof(rnd));
        if (rc == CGW_OK)
        {
            /* 256 is a multiple of 32: every character is uniform (5 bits of entropy). */
            for (size_t i = 0u; i < CGW_PASSPHRASE_LEN; i++)
            {
                out[i] = k_base32[rnd[i] & 0x1Fu];
            }
            out[CGW_PASSPHRASE_LEN] = '\0';
        }
        cgw_crypto_zeroize(rnd, sizeof(rnd));
    }
    return rc;
}

void cgw_pairing_ssid(const uint8_t *mac, char *out)
{
    pair_sb_t sb = {out, CGW_SSID_SIZE, 0u, false};

    out[0] = '\0';
    sb_puts(&sb, "LockSys-");
    sb_hex(&sb, &mac[CGW_MAC_LEN - 2u], 2u, '\0');
}

/* @satisfies SWR-CGW-017 */
size_t cgw_pairing_build_uri(const cgw_pair_uri_in_t *in, char *out, size_t size)
{
    char key[48];
    pair_sb_t sb = {out, size, 0u, false};
    size_t len = 0u;

    if ((in != NULL) && (out != NULL) && (size > 0u) && (in->device_id != NULL) &&
        (in->ssid != NULL) && (in->passphrase != NULL) && (in->k_pair != NULL) &&
        (in->bssid != NULL) &&
        (cgw_pairing_base64url(in->k_pair, CGW_KEY_LEN, key, sizeof(key)) != 0u))
    {
        out[0] = '\0';
        sb_puts(&sb, "locksys://pair?v=1&id=");
        sb_hex(&sb, in->device_id, CGW_DEVICE_ID_LEN, '\0');
        sb_puts(&sb, "&s=");
        sb_puts(&sb, in->ssid);
        sb_puts(&sb, "&p=");
        sb_puts(&sb, in->passphrase);
        sb_puts(&sb, "&k=");
        sb_puts(&sb, key);
        sb_puts(&sb, "&b=");
        sb_hex(&sb, in->bssid, CGW_MAC_LEN, ':');
        sb_puts(&sb, in->transition ? "&sec=wpa2wpa3" : "&sec=wpa3");
        len = sb.overflow ? 0u : sb.len;
        cgw_crypto_zeroize(key, sizeof(key));
    }
    if ((len == 0u) && (out != NULL) && (size > 0u))
    {
        cgw_crypto_zeroize(out, size);
    }
    return len;
}
