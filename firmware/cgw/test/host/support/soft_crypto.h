/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file soft_crypto.h
 * @brief Host-test implementation of the crypto port: SHA-256, HMAC-SHA256 (RFC 2104) and
 *        HKDF-SHA256 (RFC 5869) in software, keys in a small table.
 */

#ifndef SOFT_CRYPTO_H
#define SOFT_CRYPTO_H

#include <stdint.h>

/** @brief Destroy every key. */
void soft_crypto_reset(void);

/**
 * @brief Number of keys currently held (key hygiene checks).
 *
 * @return Live keys.
 */
uint32_t soft_crypto_live_keys(void);

#endif /* SOFT_CRYPTO_H */
