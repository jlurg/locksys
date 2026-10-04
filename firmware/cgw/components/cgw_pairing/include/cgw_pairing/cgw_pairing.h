/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_pairing.h
 * @brief Pairing window, pending K_pair, pairing payload and SoftAP credentials
 *        (LS-SAIC-001 sections 8.1 and 8.5).
 *
 * Runs in the core task. Secrets are zeroised as soon as they are no longer needed and are never
 * logged; the payload is printed only through the console port between the secret markers.
 */

#ifndef CGW_PAIRING_H
#define CGW_PAIRING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cgw_ports/cgw_keystore_port.h"
#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Largest pairing payload including the terminator (LS-SAIC-001 section 8.5). */
#define CGW_PAIR_URI_MAX (257u)

/** @brief SSID buffer size: "LockSys-XXXX" plus terminator. */
#define CGW_SSID_SIZE (13u)

/** @brief Length of a 6-byte MAC address. */
#define CGW_MAC_LEN (6u)

    /** @brief Pairing state. Initialise with cgw_pairing_init(). */
    typedef struct
    {
        uint8_t pending[CGW_KEY_LEN]; /**< pending K_pair while the window is open */
        uint32_t t_open_ms;           /**< window opening time */
        bool open;                    /**< pairing window open */
    } cgw_pairing_t;

    /** @brief Inputs of the pairing payload. */
    typedef struct
    {
        const uint8_t *device_id; /**< CGW_DEVICE_ID_LEN bytes */
        const char *ssid;         /**< SoftAP SSID */
        const char *passphrase;   /**< SoftAP passphrase (base32) */
        const uint8_t *k_pair;    /**< CGW_KEY_LEN bytes */
        const uint8_t *bssid;     /**< CGW_MAC_LEN bytes, AP MAC */
        bool transition;          /**< DEV transition mode: sec=wpa2wpa3 */
    } cgw_pair_uri_in_t;

    /**
 * @brief Initialise: window closed.
 *
 * @param[out] p Pairing state.
 */
    void cgw_pairing_init(cgw_pairing_t *p);

    /**
 * @brief Open the pairing window with a new pending K_pair from the hardware RNG.
 *
 * @param[in,out] p      Pairing state.
 * @param[in]     now_ms Current time.
 * @retval CGW_OK           Window open.
 * @retval CGW_E_STATE      Window already open.
 * @retval CGW_E_NO_ENTROPY RNG not available (RF off); window stays closed.
 */
    cgw_rc_t cgw_pairing_open(cgw_pairing_t *p, uint32_t now_ms);

    /**
 * @brief Test whether the pairing window is open.
 *
 * @param[in] p Pairing state.
 * @return true while open.
 */
    bool cgw_pairing_is_open(const cgw_pairing_t *p);

    /**
 * @brief Pending K_pair.
 *
 * @param[in] p Pairing state.
 * @return CGW_KEY_LEN bytes while the window is open, otherwise NULL.
 */
    const uint8_t *cgw_pairing_pending_key(const cgw_pairing_t *p);

    /**
 * @brief Close the window when t_pairing_window_ms has elapsed; the pending key is zeroised.
 *
 * @param[in,out] p      Pairing state.
 * @param[in]     now_ms Current time.
 * @return true when the window closed in this call (Notice PAIRING_WINDOW_CLOSED).
 */
    bool cgw_pairing_tick(cgw_pairing_t *p, uint32_t now_ms);

    /**
 * @brief Commit the pending K_pair after key confirmation and close the window.
 *
 * @param[in,out] p         Pairing state.
 * @param[in]     client_id Client identifier of the confirming session.
 * @param[in,out] rec       Current record in, committed record out (generation + 1); the caller
 *                          zeroises it after use.
 * @retval CGW_OK      Committed to the key store.
 * @retval CGW_E_STATE Window not open.
 * @retval CGW_E_IO    Key store error; the window is closed and the old pairing stays.
 */
    cgw_rc_t cgw_pairing_commit(cgw_pairing_t *p, const uint8_t *client_id,
                                cgw_pairing_record_t *rec);

    /**
 * @brief Close the window and zeroise the pending key.
 *
 * @param[in,out] p Pairing state.
 */
    void cgw_pairing_close(cgw_pairing_t *p);

    /**
 * @brief Build the pairing payload
 *        locksys://pair?v=1&id=..&s=..&p=..&k=..&b=..&sec=..
 *
 * @param[in]  in   Inputs.
 * @param[out] out  Destination, NUL-terminated.
 * @param[in]  size Size of @p out (at most CGW_PAIR_URI_MAX is needed).
 * @return Length without the terminator, or 0 when @p out is too small or an input is missing.
 */
    size_t cgw_pairing_build_uri(const cgw_pair_uri_in_t *in, char *out, size_t size);

    /**
 * @brief Generate a SoftAP passphrase of CGW_PASSPHRASE_LEN RFC 4648 base32 characters.
 *
 * @param[out] out  Destination, NUL-terminated.
 * @param[in]  size Size of @p out; at least CGW_PASSPHRASE_LEN + 1.
 * @retval CGW_OK           Passphrase generated.
 * @retval CGW_E_NO_ENTROPY RNG not available.
 * @retval CGW_E_ARG        Buffer too small.
 */
    cgw_rc_t cgw_pairing_make_passphrase(char *out, size_t size);

    /**
 * @brief Encode bytes as base64url without padding.
 *
 * @param[in]  in   Input.
 * @param[in]  len  Input length.
 * @param[out] out  Destination, NUL-terminated.
 * @param[in]  size Size of @p out.
 * @return Encoded length, or 0 when @p out is too small.
 */
    size_t cgw_pairing_base64url(const uint8_t *in, size_t len, char *out, size_t size);

    /**
 * @brief Build the SSID "LockSys-XXXX" from the last two bytes of the AP MAC.
 *
 * @param[in]  mac AP MAC (CGW_MAC_LEN bytes).
 * @param[out] out CGW_SSID_SIZE bytes.
 */
    void cgw_pairing_ssid(const uint8_t *mac, char *out);

#ifdef __cplusplus
}
#endif

#endif /* CGW_PAIRING_H */
