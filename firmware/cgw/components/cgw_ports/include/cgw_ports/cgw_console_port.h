/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_console_port.h
 * @brief Operator console port: pairing QR output and status indication.
 *
 * Implemented by the composition root, which hands the requests to the sys task (cgw_hmi_esp).
 * The core task is the only caller.
 */

#ifndef CGW_CONSOLE_PORT_H
#define CGW_CONSOLE_PORT_H

#include <stddef.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief Print the pairing URI as a QR code between the secret markers.
 *
 * The URI is copied into a static buffer owned by the console; the sys task prints it with
 * raw stdout writes (never through the logging system) and zeroises the buffer afterwards.
 * The caller zeroises its own copy after the call.
 *
 * @param[in] uri NUL-terminated pairing URI.
 * @param[in] len Length of @p uri without the terminator.
 * @retval CGW_OK      Request accepted.
 * @retval CGW_E_STATE A previous request is still being printed.
 * @retval CGW_E_ARG   URI too long or NULL.
 */
    cgw_rc_t cgw_console_show_pairing_qr(const char *uri, size_t len);

    /**
 * @brief Set the operator indication on the status LED.
 *
 * @param[in] ind Indication.
 */
    void cgw_console_set_indication(cgw_indication_t ind);

#ifdef __cplusplus
}
#endif

#endif /* CGW_CONSOLE_PORT_H */
