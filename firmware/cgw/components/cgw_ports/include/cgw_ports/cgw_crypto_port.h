/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_crypto_port.h
 * @brief Crypto port: HMAC-SHA256, HKDF-SHA256 session keys, frame tags, key hygiene.
 *
 * Implemented by cgw_crypto_psa (PSA Crypto API) on the target. Keys live in the backend key
 * store and are referenced by handle; K_sess never leaves it. All calls are made from the core
 * task only (single-thread rule, MBEDTLS_THREADING_C is off).
 */

#ifndef CGW_CRYPTO_PORT_H
#define CGW_CRYPTO_PORT_H

#include <stddef.h>
#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Handle of a key held by the crypto backend (PSA key identifier). */
    typedef uint32_t cgw_key_t;

/** @brief No key. */
#define CGW_KEY_NONE ((cgw_key_t)0u)

/** @name Frame tag direction bytes (LS-SAIC-001 section 8.2) @{ */
#define CGW_DIR_APP_TO_CGW (0x41u)
#define CGW_DIR_CGW_TO_APP (0x43u)
    /** @} */

    /** @brief One contiguous input segment of a MAC computation. */
    typedef struct
    {
        const uint8_t *data; /**< segment bytes; may be NULL when len is 0 */
        size_t len;          /**< segment length */
    } cgw_buf_t;

    /** @brief Input of a frame tag: dir || counter (big endian, 32 bit) || raw body. */
    typedef struct
    {
        const uint8_t *body; /**< raw body bytes as received or sent */
        size_t body_len;     /**< body length */
        uint32_t counter;    /**< frame counter */
        uint8_t dir;         /**< CGW_DIR_APP_TO_CGW or CGW_DIR_CGW_TO_APP */
    } cgw_tag_input_t;

    /**
 * @brief Import raw key bytes as a volatile HMAC-SHA256 key (sign and verify).
 *
 * @param[in]  key     Key bytes.
 * @param[in]  key_len Key length (1 .. 64).
 * @param[out] out     Key handle; destroy with cgw_crypto_key_destroy().
 * @retval CGW_OK       Key imported.
 * @retval CGW_E_ARG    Invalid argument.
 * @retval CGW_E_CRYPTO Backend failure.
 */
    cgw_rc_t cgw_crypto_import_hmac_key(const uint8_t *key, size_t key_len, cgw_key_t *out);

    /**
 * @brief Compute HMAC-SHA256 over the concatenation of @p parts.
 *
 * @param[in]  key     HMAC key handle.
 * @param[in]  parts   Input segments, processed in order.
 * @param[in]  n_parts Number of segments.
 * @param[out] mac     CGW_HMAC_LEN bytes of output.
 * @retval CGW_OK       MAC written.
 * @retval CGW_E_ARG    Invalid argument.
 * @retval CGW_E_CRYPTO Backend failure.
 */
    cgw_rc_t cgw_crypto_hmac(cgw_key_t key, const cgw_buf_t *parts, size_t n_parts,
                             uint8_t mac[CGW_HMAC_LEN]);

    /**
 * @brief Verify an HMAC-SHA256 over the concatenation of @p parts in constant time.
 *
 * @param[in] key     HMAC key handle.
 * @param[in] parts   Input segments, processed in order.
 * @param[in] n_parts Number of segments.
 * @param[in] mac     Expected MAC.
 * @param[in] mac_len Length of @p mac; must be CGW_HMAC_LEN.
 * @retval CGW_OK       MAC matches.
 * @retval CGW_E_CRYPTO MAC mismatch or backend failure.
 * @retval CGW_E_ARG    Invalid argument.
 */
    cgw_rc_t cgw_crypto_hmac_verify(cgw_key_t key, const cgw_buf_t *parts, size_t n_parts,
                                    const uint8_t *mac, size_t mac_len);

    /**
 * @brief Derive K_sess with HKDF-SHA256 (RFC 5869) into a volatile tag key.
 *
 * The derived key is usable only for frame tags (truncated HMAC-SHA256, CGW_TAG_LEN bytes).
 *
 * @param[in]  ikm     Input keying material (K_pair).
 * @param[in]  ikm_len Length of @p ikm.
 * @param[in]  salt    Salt (server_nonce || client_nonce).
 * @param[in]  info    Context (label || device_id || client_id).
 * @param[out] out     Key handle; destroy with cgw_crypto_key_destroy().
 * @retval CGW_OK       Key derived.
 * @retval CGW_E_ARG    Invalid argument.
 * @retval CGW_E_CRYPTO Backend failure.
 */
    cgw_rc_t cgw_crypto_derive_session_key(const uint8_t *ikm, size_t ikm_len,
                                           const cgw_buf_t *salt, const cgw_buf_t *info,
                                           cgw_key_t *out);

    /**
 * @brief Compute the frame tag of an outgoing frame.
 *
 * @param[in]  key Session key handle.
 * @param[in]  in  Tag input.
 * @param[out] tag CGW_TAG_LEN bytes of output.
 * @retval CGW_OK       Tag written.
 * @retval CGW_E_ARG    Invalid argument.
 * @retval CGW_E_CRYPTO Backend failure.
 */
    cgw_rc_t cgw_crypto_tag_compute(cgw_key_t key, const cgw_tag_input_t *in,
                                    uint8_t tag[CGW_TAG_LEN]);

    /**
 * @brief Verify the tag of a received frame in constant time.
 *
 * @param[in] key     Session key handle.
 * @param[in] in      Tag input.
 * @param[in] tag     Received tag.
 * @param[in] tag_len Length of @p tag; anything other than CGW_TAG_LEN fails.
 * @retval CGW_OK       Tag valid.
 * @retval CGW_E_CRYPTO Tag invalid or backend failure.
 * @retval CGW_E_ARG    Invalid argument.
 */
    cgw_rc_t cgw_crypto_tag_verify(cgw_key_t key, const cgw_tag_input_t *in, const uint8_t *tag,
                                   size_t tag_len);

    /**
 * @brief Destroy a key held by the backend.
 *
 * @param[in] key Key handle; CGW_KEY_NONE is ignored.
 */
    void cgw_crypto_key_destroy(cgw_key_t key);

    /**
 * @brief Overwrite a buffer with zeros in a way the compiler does not remove.
 *
 * @param[out] buf Buffer; NULL is ignored.
 * @param[in]  len Number of bytes.
 */
    void cgw_crypto_zeroize(void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CGW_CRYPTO_PORT_H */
