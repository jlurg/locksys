/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_keystore_port.h
 * @brief Key store port: persistent pairing record and SoftAP passphrase.
 *
 * Implemented by cgw_store_nvs (NVS namespace cgw_sec). Writes stall both CPUs during a flash
 * erase; the core calls the store functions only while the window is idle.
 */

#ifndef CGW_KEYSTORE_PORT_H
#define CGW_KEYSTORE_PORT_H

#include <stddef.h>
#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Persistent pairing record. Zeroise every copy after use. */
    typedef struct
    {
        uint8_t k_pair[CGW_KEY_LEN];          /**< pairing key */
        uint8_t client_id[CGW_CLIENT_ID_LEN]; /**< identifier of the paired APP */
        uint32_t generation;                  /**< pairing generation, incremented per pairing */
    } cgw_pairing_record_t;

    /**
 * @brief Load the pairing record.
 *
 * @param[out] rec Record.
 * @retval CGW_OK          Record loaded.
 * @retval CGW_E_NOT_FOUND No pairing stored.
 * @retval CGW_E_IO        Storage error.
 */
    cgw_rc_t cgw_keystore_load_pairing(cgw_pairing_record_t *rec);

    /**
 * @brief Store the pairing record, replacing the previous one.
 *
 * @param[in] rec Record.
 * @retval CGW_OK   Record stored and committed.
 * @retval CGW_E_IO Storage error.
 */
    cgw_rc_t cgw_keystore_store_pairing(const cgw_pairing_record_t *rec);

    /**
 * @brief Load the SoftAP passphrase.
 *
 * @param[out] buf  Destination, NUL-terminated on success.
 * @param[in]  size Size of @p buf; at least CGW_PASSPHRASE_LEN + 1.
 * @retval CGW_OK          Passphrase loaded.
 * @retval CGW_E_NOT_FOUND No passphrase stored.
 * @retval CGW_E_ARG       Buffer too small.
 * @retval CGW_E_IO        Storage error.
 */
    cgw_rc_t cgw_keystore_load_passphrase(char *buf, size_t size);

    /**
 * @brief Store the SoftAP passphrase.
 *
 * @param[in] passphrase NUL-terminated passphrase of CGW_PASSPHRASE_LEN characters.
 * @retval CGW_OK   Stored and committed.
 * @retval CGW_E_IO Storage error.
 */
    cgw_rc_t cgw_keystore_store_passphrase(const char *passphrase);

    /**
 * @brief Erase the pairing record and the passphrase (factory reset).
 *
 * @retval CGW_OK   Erased and committed.
 * @retval CGW_E_IO Storage error.
 */
    cgw_rc_t cgw_keystore_erase(void);

#ifdef __cplusplus
}
#endif

#endif /* CGW_KEYSTORE_PORT_H */
