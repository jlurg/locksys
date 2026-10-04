/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_crypto_psa/cgw_crypto_psa.h"

#include <stdatomic.h>
#include <string.h>

#include "cgw_ports/cgw_crypto_port.h"
#include "cgw_ports/cgw_rng_port.h"
#include "mbedtls/platform_util.h"
#include "psa/crypto.h"

#ifdef ESP_PLATFORM
#include "esp_random.h"
#endif

#define PSA_HMAC_ALG    PSA_ALG_HMAC(PSA_ALG_SHA_256)
#define PSA_TAG_ALG     PSA_ALG_TRUNCATED_MAC(PSA_HMAC_ALG, CGW_TAG_LEN)
#define PSA_KEY_BITS(n) ((size_t)(n) * 8u)

static atomic_bool s_rng_enabled;

/* RFC 4231 test case 2 and RFC 5869 test case 1. */
static const uint8_t k_kat_hmac_key[] = {0x4a, 0x65, 0x66, 0x65};
static const uint8_t k_kat_hmac_data[] = "what do ya want for nothing?";
static const uint8_t k_kat_hmac_mac[CGW_HMAC_LEN] = {
    0x5b, 0xdc, 0xc1, 0x46, 0xbf, 0x60, 0x75, 0x4e, 0x6a, 0x04, 0x24, 0x26, 0x08, 0x95, 0x75, 0xc7,
    0x5a, 0x00, 0x3f, 0x08, 0x9d, 0x27, 0x39, 0x83, 0x9d, 0xec, 0x58, 0xb9, 0x64, 0xec, 0x38, 0x43};
static const uint8_t k_kat_ikm[22] = {0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                                      0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                                      0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b};
static const uint8_t k_kat_salt[13] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                       0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};
static const uint8_t k_kat_info[10] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9};
static const uint8_t k_kat_okm[42] = {
    0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90, 0x43, 0x4f, 0x64, 0xd0, 0x36,
    0x2f, 0x2a, 0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c, 0x5d, 0xb0, 0x2d, 0x56,
    0xec, 0xc4, 0xc5, 0xbf, 0x34, 0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65};

static cgw_rc_t psa_rc(psa_status_t st)
{
    return (st == PSA_SUCCESS) ? CGW_OK : CGW_E_CRYPTO;
}

static psa_status_t psa_hkdf_setup(psa_key_derivation_operation_t *op, const cgw_buf_t *ikm,
                                   const cgw_buf_t *salt, const cgw_buf_t *info)
{
    psa_status_t st = psa_key_derivation_setup(op, PSA_ALG_HKDF(PSA_ALG_SHA_256));

    if (st == PSA_SUCCESS)
    {
        st = psa_key_derivation_input_bytes(op, PSA_KEY_DERIVATION_INPUT_SALT, salt->data,
                                            salt->len);
    }
    if (st == PSA_SUCCESS)
    {
        st = psa_key_derivation_input_bytes(op, PSA_KEY_DERIVATION_INPUT_SECRET, ikm->data,
                                            ikm->len);
    }
    if (st == PSA_SUCCESS)
    {
        st = psa_key_derivation_input_bytes(op, PSA_KEY_DERIVATION_INPUT_INFO, info->data,
                                            info->len);
    }
    return st;
}

static cgw_rc_t crypto_kat(void)
{
    uint8_t mac[CGW_HMAC_LEN];
    uint8_t okm[sizeof(k_kat_okm)];
    cgw_key_t key = CGW_KEY_NONE;
    const cgw_buf_t data = {k_kat_hmac_data, sizeof(k_kat_hmac_data) - 1u};
    const cgw_buf_t ikm = {k_kat_ikm, sizeof(k_kat_ikm)};
    const cgw_buf_t salt = {k_kat_salt, sizeof(k_kat_salt)};
    const cgw_buf_t info = {k_kat_info, sizeof(k_kat_info)};
    psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
    cgw_rc_t rc = cgw_crypto_import_hmac_key(k_kat_hmac_key, sizeof(k_kat_hmac_key), &key);

    if (rc == CGW_OK)
    {
        rc = cgw_crypto_hmac(key, &data, 1u, mac);
    }
    if ((rc == CGW_OK) && (memcmp(mac, k_kat_hmac_mac, sizeof(mac)) != 0))
    {
        rc = CGW_E_CRYPTO;
    }
    cgw_crypto_key_destroy(key);
    if (rc == CGW_OK)
    {
        psa_status_t st = psa_hkdf_setup(&op, &ikm, &salt, &info);

        if (st == PSA_SUCCESS)
        {
            st = psa_key_derivation_output_bytes(&op, okm, sizeof(okm));
        }
        (void)psa_key_derivation_abort(&op);
        rc = ((st == PSA_SUCCESS) && (memcmp(okm, k_kat_okm, sizeof(okm)) == 0)) ? CGW_OK
                                                                                 : CGW_E_CRYPTO;
    }
    return rc;
}

/* @satisfies SWR-CGW-013 */
cgw_rc_t cgw_crypto_psa_init(void)
{
    cgw_rc_t rc = psa_rc(psa_crypto_init());

    if (rc == CGW_OK)
    {
        rc = crypto_kat();
    }
    return rc;
}

void cgw_crypto_psa_enable_rng(bool enable)
{
    atomic_store(&s_rng_enabled, enable);
}

/* @satisfies SWR-CGW-014 */
cgw_rc_t cgw_rng_fill(uint8_t *buf, size_t len)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((buf != NULL) || (len == 0u))
    {
        rc = CGW_E_NO_ENTROPY;
        if (atomic_load(&s_rng_enabled))
        {
#ifdef ESP_PLATFORM
            esp_fill_random(buf, len);
            rc = CGW_OK;
#endif
        }
    }
    return rc;
}

cgw_rc_t cgw_crypto_import_hmac_key(const uint8_t *key, size_t key_len, cgw_key_t *out)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((key != NULL) && (out != NULL) && (key_len > 0u) && (key_len <= 64u))
    {
        psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
        psa_key_id_t id = PSA_KEY_ID_NULL;

        psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
        psa_set_key_bits(&attr, PSA_KEY_BITS(key_len));
        psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE);
        psa_set_key_algorithm(&attr, PSA_HMAC_ALG);
        rc = psa_rc(psa_import_key(&attr, key, key_len, &id));
        *out = (rc == CGW_OK) ? (cgw_key_t)id : CGW_KEY_NONE;
        psa_reset_key_attributes(&attr);
    }
    return rc;
}

static psa_status_t crypto_mac_update(psa_mac_operation_t *op, const cgw_buf_t *parts,
                                      size_t n_parts)
{
    psa_status_t st = PSA_SUCCESS;

    for (size_t i = 0u; (i < n_parts) && (st == PSA_SUCCESS); i++)
    {
        st = psa_mac_update(op, parts[i].data, parts[i].len);
    }
    return st;
}

cgw_rc_t cgw_crypto_hmac(cgw_key_t key, const cgw_buf_t *parts, size_t n_parts,
                         uint8_t mac[CGW_HMAC_LEN])
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((parts != NULL) && (mac != NULL))
    {
        psa_mac_operation_t op = PSA_MAC_OPERATION_INIT;
        size_t len = 0u;
        psa_status_t st = psa_mac_sign_setup(&op, (psa_key_id_t)key, PSA_HMAC_ALG);

        if (st == PSA_SUCCESS)
        {
            st = crypto_mac_update(&op, parts, n_parts);
        }
        if (st == PSA_SUCCESS)
        {
            st = psa_mac_sign_finish(&op, mac, CGW_HMAC_LEN, &len);
        }
        (void)psa_mac_abort(&op);
        rc = psa_rc(st);
    }
    return rc;
}

cgw_rc_t cgw_crypto_hmac_verify(cgw_key_t key, const cgw_buf_t *parts, size_t n_parts,
                                const uint8_t *mac, size_t mac_len)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((parts != NULL) && (mac != NULL))
    {
        psa_mac_operation_t op = PSA_MAC_OPERATION_INIT;
        psa_status_t st = (mac_len == CGW_HMAC_LEN)
                              ? psa_mac_verify_setup(&op, (psa_key_id_t)key, PSA_HMAC_ALG)
                              : PSA_ERROR_INVALID_SIGNATURE;

        if (st == PSA_SUCCESS)
        {
            st = crypto_mac_update(&op, parts, n_parts);
        }
        if (st == PSA_SUCCESS)
        {
            st = psa_mac_verify_finish(&op, mac, mac_len);
        }
        (void)psa_mac_abort(&op);
        rc = psa_rc(st);
    }
    return rc;
}

cgw_rc_t cgw_crypto_derive_session_key(const uint8_t *ikm, size_t ikm_len, const cgw_buf_t *salt,
                                       const cgw_buf_t *info, cgw_key_t *out)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((ikm != NULL) && (salt != NULL) && (info != NULL) && (out != NULL))
    {
        psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;
        psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
        psa_key_id_t id = PSA_KEY_ID_NULL;
        const cgw_buf_t secret = {ikm, ikm_len};
        psa_status_t st = psa_hkdf_setup(&op, &secret, salt, info);

        psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
        psa_set_key_bits(&attr, PSA_KEY_BITS(CGW_KEY_LEN));
        psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE);
        psa_set_key_algorithm(&attr, PSA_TAG_ALG);
        if (st == PSA_SUCCESS)
        {
            st = psa_key_derivation_output_key(&attr, &op, &id);
        }
        (void)psa_key_derivation_abort(&op);
        psa_reset_key_attributes(&attr);
        rc = psa_rc(st);
        *out = (rc == CGW_OK) ? (cgw_key_t)id : CGW_KEY_NONE;
    }
    return rc;
}

static psa_status_t crypto_tag_update(psa_mac_operation_t *op, const cgw_tag_input_t *in)
{
    const uint8_t hdr[5] = {in->dir, (uint8_t)(in->counter >> 24), (uint8_t)(in->counter >> 16),
                            (uint8_t)(in->counter >> 8), (uint8_t)in->counter};
    psa_status_t st = psa_mac_update(op, hdr, sizeof(hdr));

    if (st == PSA_SUCCESS)
    {
        st = psa_mac_update(op, in->body, in->body_len);
    }
    return st;
}

cgw_rc_t cgw_crypto_tag_compute(cgw_key_t key, const cgw_tag_input_t *in, uint8_t tag[CGW_TAG_LEN])
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((in != NULL) && (tag != NULL) && ((in->body != NULL) || (in->body_len == 0u)))
    {
        psa_mac_operation_t op = PSA_MAC_OPERATION_INIT;
        size_t len = 0u;
        psa_status_t st = psa_mac_sign_setup(&op, (psa_key_id_t)key, PSA_TAG_ALG);

        if (st == PSA_SUCCESS)
        {
            st = crypto_tag_update(&op, in);
        }
        if (st == PSA_SUCCESS)
        {
            st = psa_mac_sign_finish(&op, tag, CGW_TAG_LEN, &len);
        }
        (void)psa_mac_abort(&op);
        rc = psa_rc(st);
    }
    return rc;
}

cgw_rc_t cgw_crypto_tag_verify(cgw_key_t key, const cgw_tag_input_t *in, const uint8_t *tag,
                               size_t tag_len)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((in != NULL) && (tag != NULL) && ((in->body != NULL) || (in->body_len == 0u)))
    {
        psa_mac_operation_t op = PSA_MAC_OPERATION_INIT;
        psa_status_t st = (tag_len == CGW_TAG_LEN)
                              ? psa_mac_verify_setup(&op, (psa_key_id_t)key, PSA_TAG_ALG)
                              : PSA_ERROR_INVALID_SIGNATURE;

        if (st == PSA_SUCCESS)
        {
            st = crypto_tag_update(&op, in);
        }
        if (st == PSA_SUCCESS)
        {
            st = psa_mac_verify_finish(&op, tag, tag_len);
        }
        (void)psa_mac_abort(&op);
        rc = psa_rc(st);
    }
    return rc;
}

void cgw_crypto_key_destroy(cgw_key_t key)
{
    if (key != CGW_KEY_NONE)
    {
        (void)psa_destroy_key((psa_key_id_t)key);
    }
}

void cgw_crypto_zeroize(void *buf, size_t len)
{
    if (buf != NULL)
    {
        mbedtls_platform_zeroize(buf, len);
    }
}
