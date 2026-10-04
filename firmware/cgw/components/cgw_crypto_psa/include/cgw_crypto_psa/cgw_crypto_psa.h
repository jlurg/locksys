/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_crypto_psa.h
 * @brief Crypto and RNG adapter over the PSA Crypto API (cgw_crypto_port.h, cgw_rng_port.h).
 *
 * All keys are volatile PSA keys. After start-up every call is made from the core task only
 * (MBEDTLS_THREADING_C is off).
 */

#ifndef CGW_CRYPTO_PSA_H
#define CGW_CRYPTO_PSA_H

#include <stdbool.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief Initialise PSA Crypto and run the known-answer tests (RFC 4231 case 2 and RFC 5869
 *        case 1, interfaces/vectors/crypto_kat.json).
 *
 * @retval CGW_OK       Ready.
 * @retval CGW_E_CRYPTO Initialisation or a known-answer test failed (B1B16, SAFE).
 */
    cgw_rc_t cgw_crypto_psa_init(void);

    /**
 * @brief Allow or forbid true random numbers.
 *
 * Enabled after WIFI_EVENT_AP_START, or inside the bootloader random-enable bracket for the
 * first-boot passphrase (SWR-CGW-014). While disabled, cgw_rng_fill() returns
 * CGW_E_NO_ENTROPY.
 *
 * @param[in] enable Entropy source running.
 */
    void cgw_crypto_psa_enable_rng(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* CGW_CRYPTO_PSA_H */
