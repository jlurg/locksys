/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "soft_crypto.h"

#include <string.h>

#include "cgw_ports/cgw_crypto_port.h"

#define SC_KEYS  (16u)
#define SC_BLOCK (64u)
#define SC_HASH  (32u)

typedef struct
{
    uint32_t h[8];
    uint8_t buf[SC_BLOCK];
    uint64_t bits;
    size_t used;
} sc_sha_t;

typedef struct
{
    uint8_t key[SC_BLOCK];
    size_t len;
    bool used;
    bool tag_only;
} sc_key_t;

static sc_key_t s_keys[SC_KEYS];

static const uint32_t k_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u};

static uint32_t sc_ror(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32u - n));
}

static void sc_block(sc_sha_t *s, const uint8_t *p)
{
    uint32_t w[64];
    uint32_t v[8];

    for (uint32_t i = 0u; i < 16u; i++)
    {
        w[i] = ((uint32_t)p[4u * i] << 24) | ((uint32_t)p[(4u * i) + 1u] << 16) |
               ((uint32_t)p[(4u * i) + 2u] << 8) | (uint32_t)p[(4u * i) + 3u];
    }
    for (uint32_t i = 16u; i < 64u; i++)
    {
        uint32_t s0 = sc_ror(w[i - 15u], 7u) ^ sc_ror(w[i - 15u], 18u) ^ (w[i - 15u] >> 3);
        uint32_t s1 = sc_ror(w[i - 2u], 17u) ^ sc_ror(w[i - 2u], 19u) ^ (w[i - 2u] >> 10);

        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }
    (void)memcpy(v, s->h, sizeof(v));
    for (uint32_t i = 0u; i < 64u; i++)
    {
        uint32_t s1 = sc_ror(v[4], 6u) ^ sc_ror(v[4], 11u) ^ sc_ror(v[4], 25u);
        uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
        uint32_t t1 = v[7] + s1 + ch + k_k[i] + w[i];
        uint32_t s0 = sc_ror(v[0], 2u) ^ sc_ror(v[0], 13u) ^ sc_ror(v[0], 22u);
        uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);

        (void)memmove(&v[1], &v[0], 7u * sizeof(v[0]));
        v[4] += t1;
        v[0] = t1 + s0 + maj;
    }
    for (uint32_t i = 0u; i < 8u; i++)
    {
        s->h[i] += v[i];
    }
}

static void sc_init(sc_sha_t *s)
{
    static const uint32_t h0[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

    (void)memset(s, 0, sizeof(*s));
    (void)memcpy(s->h, h0, sizeof(h0));
}

static void sc_update(sc_sha_t *s, const uint8_t *p, size_t len)
{
    for (size_t i = 0u; i < len; i++)
    {
        s->buf[s->used] = p[i];
        s->used++;
        s->bits += 8u;
        if (s->used == SC_BLOCK)
        {
            sc_block(s, s->buf);
            s->used = 0u;
        }
    }
}

static void sc_final(sc_sha_t *s, uint8_t out[SC_HASH])
{
    uint64_t bits = s->bits;
    uint8_t pad = 0x80u;
    uint8_t len[8];

    sc_update(s, &pad, 1u);
    pad = 0u;
    while (s->used != 56u)
    {
        sc_update(s, &pad, 1u);
    }
    for (uint32_t i = 0u; i < 8u; i++)
    {
        len[i] = (uint8_t)(bits >> (56u - (8u * i)));
    }
    sc_update(s, len, sizeof(len));
    for (uint32_t i = 0u; i < 8u; i++)
    {
        out[4u * i] = (uint8_t)(s->h[i] >> 24);
        out[(4u * i) + 1u] = (uint8_t)(s->h[i] >> 16);
        out[(4u * i) + 2u] = (uint8_t)(s->h[i] >> 8);
        out[(4u * i) + 3u] = (uint8_t)s->h[i];
    }
}

/* HMAC-SHA256 over the concatenation of parts. */
static void sc_hmac(const uint8_t *key, size_t key_len, const cgw_buf_t *parts, size_t n,
                    uint8_t out[SC_HASH])
{
    uint8_t k[SC_BLOCK] = {0u};
    uint8_t pad[SC_BLOCK];
    uint8_t inner[SC_HASH];
    sc_sha_t s;

    if (key_len > SC_BLOCK)
    {
        sc_init(&s);
        sc_update(&s, key, key_len);
        sc_final(&s, k);
    }
    else
    {
        (void)memcpy(k, key, key_len);
    }
    for (size_t i = 0u; i < SC_BLOCK; i++)
    {
        pad[i] = (uint8_t)(k[i] ^ 0x36u);
    }
    sc_init(&s);
    sc_update(&s, pad, SC_BLOCK);
    for (size_t i = 0u; i < n; i++)
    {
        sc_update(&s, parts[i].data, parts[i].len);
    }
    sc_final(&s, inner);
    for (size_t i = 0u; i < SC_BLOCK; i++)
    {
        pad[i] = (uint8_t)(k[i] ^ 0x5cu);
    }
    sc_init(&s);
    sc_update(&s, pad, SC_BLOCK);
    sc_update(&s, inner, SC_HASH);
    sc_final(&s, out);
}

static bool sc_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0u;

    for (size_t i = 0u; i < len; i++)
    {
        diff = (uint8_t)(diff | (a[i] ^ b[i]));
    }
    return diff == 0u;
}

static sc_key_t *sc_key(cgw_key_t id)
{
    return ((id >= 1u) && (id <= SC_KEYS) && s_keys[id - 1u].used) ? &s_keys[id - 1u] : NULL;
}

static cgw_key_t sc_store(const uint8_t *key, size_t len, bool tag_only)
{
    cgw_key_t id = CGW_KEY_NONE;

    for (uint32_t i = 0u; (i < SC_KEYS) && (id == CGW_KEY_NONE); i++)
    {
        if (!s_keys[i].used)
        {
            (void)memcpy(s_keys[i].key, key, len);
            s_keys[i].len = len;
            s_keys[i].used = true;
            s_keys[i].tag_only = tag_only;
            id = (cgw_key_t)(i + 1u);
        }
    }
    return id;
}

void soft_crypto_reset(void)
{
    (void)memset(s_keys, 0, sizeof(s_keys));
}

uint32_t soft_crypto_live_keys(void)
{
    uint32_t n = 0u;

    for (uint32_t i = 0u; i < SC_KEYS; i++)
    {
        n += s_keys[i].used ? 1u : 0u;
    }
    return n;
}

cgw_rc_t cgw_crypto_import_hmac_key(const uint8_t *key, size_t key_len, cgw_key_t *out)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((key != NULL) && (out != NULL) && (key_len > 0u) && (key_len <= SC_BLOCK))
    {
        *out = sc_store(key, key_len, false);
        rc = (*out != CGW_KEY_NONE) ? CGW_OK : CGW_E_CRYPTO;
    }
    return rc;
}

cgw_rc_t cgw_crypto_hmac(cgw_key_t key, const cgw_buf_t *parts, size_t n_parts,
                         uint8_t mac[CGW_HMAC_LEN])
{
    const sc_key_t *k = sc_key(key);
    cgw_rc_t rc = CGW_E_CRYPTO;

    if ((k != NULL) && !k->tag_only)
    {
        sc_hmac(k->key, k->len, parts, n_parts, mac);
        rc = CGW_OK;
    }
    return rc;
}

cgw_rc_t cgw_crypto_hmac_verify(cgw_key_t key, const cgw_buf_t *parts, size_t n_parts,
                                const uint8_t *mac, size_t mac_len)
{
    uint8_t calc[CGW_HMAC_LEN];
    cgw_rc_t rc = cgw_crypto_hmac(key, parts, n_parts, calc);

    if ((rc == CGW_OK) && ((mac_len != CGW_HMAC_LEN) || !sc_equal(calc, mac, CGW_HMAC_LEN)))
    {
        rc = CGW_E_CRYPTO;
    }
    return rc;
}

cgw_rc_t cgw_crypto_derive_session_key(const uint8_t *ikm, size_t ikm_len, const cgw_buf_t *salt,
                                       const cgw_buf_t *info, cgw_key_t *out)
{
    uint8_t prk[SC_HASH];
    uint8_t okm[SC_HASH];
    const uint8_t one = 1u;
    const cgw_buf_t ikm_part = {ikm, ikm_len};
    cgw_buf_t expand[2];

    /* RFC 5869: PRK = HMAC(salt, IKM); OKM = T(1) = HMAC(PRK, info || 0x01) for L = 32. */
    sc_hmac(salt->data, salt->len, &ikm_part, 1u, prk);
    expand[0] = *info;
    expand[1].data = &one;
    expand[1].len = 1u;
    sc_hmac(prk, sizeof(prk), expand, 2u, okm);
    *out = sc_store(okm, sizeof(okm), true);
    return (*out != CGW_KEY_NONE) ? CGW_OK : CGW_E_CRYPTO;
}

static cgw_rc_t sc_tag(cgw_key_t key, const cgw_tag_input_t *in, uint8_t mac[CGW_HMAC_LEN])
{
    const sc_key_t *k = sc_key(key);
    const uint8_t hdr[5] = {in->dir, (uint8_t)(in->counter >> 24), (uint8_t)(in->counter >> 16),
                            (uint8_t)(in->counter >> 8), (uint8_t)in->counter};
    const cgw_buf_t parts[2] = {{hdr, sizeof(hdr)}, {in->body, in->body_len}};
    cgw_rc_t rc = CGW_E_CRYPTO;

    if ((k != NULL) && k->tag_only)
    {
        sc_hmac(k->key, k->len, parts, 2u, mac);
        rc = CGW_OK;
    }
    return rc;
}

cgw_rc_t cgw_crypto_tag_compute(cgw_key_t key, const cgw_tag_input_t *in, uint8_t tag[CGW_TAG_LEN])
{
    uint8_t mac[CGW_HMAC_LEN];
    cgw_rc_t rc = sc_tag(key, in, mac);

    if (rc == CGW_OK)
    {
        (void)memcpy(tag, mac, CGW_TAG_LEN);
    }
    return rc;
}

cgw_rc_t cgw_crypto_tag_verify(cgw_key_t key, const cgw_tag_input_t *in, const uint8_t *tag,
                               size_t tag_len)
{
    uint8_t mac[CGW_HMAC_LEN];
    cgw_rc_t rc = sc_tag(key, in, mac);

    if ((rc == CGW_OK) && ((tag_len != CGW_TAG_LEN) || !sc_equal(mac, tag, CGW_TAG_LEN)))
    {
        rc = CGW_E_CRYPTO;
    }
    return rc;
}

void cgw_crypto_key_destroy(cgw_key_t key)
{
    sc_key_t *k = sc_key(key);

    if (k != NULL)
    {
        (void)memset(k, 0, sizeof(*k));
    }
}

void cgw_crypto_zeroize(void *buf, size_t len)
{
    volatile uint8_t *p = buf;

    for (size_t i = 0u; (p != NULL) && (i < len); i++)
    {
        p[i] = 0u;
    }
}
