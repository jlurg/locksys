/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_rng_port.h
 * @brief Random number port: true random bytes for nonces, keys and identifiers.
 *
 * Implemented by cgw_crypto_psa (esp_fill_random) on the target and by a fake in the host
 * tests. The PSA internal DRBG is not used for secrets.
 */

#ifndef CGW_RNG_PORT_H
#define CGW_RNG_PORT_H

#include <stddef.h>
#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief Fill a buffer with true random bytes from the hardware RNG.
 *
 * @param[out] buf Destination.
 * @param[in]  len Number of bytes.
 * @retval CGW_OK           Buffer filled.
 * @retval CGW_E_NO_ENTROPY The RF subsystem is not running yet (before WIFI_EVENT_AP_START);
 *                          nothing written.
 * @retval CGW_E_ARG        @p buf is NULL while @p len is not 0.
 * @note Called only from the core task.
 */
    cgw_rc_t cgw_rng_fill(uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CGW_RNG_PORT_H */
