/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_hmi_esp.h
 * @brief Operator interface adapter (sys task): RGB status LED over RMT, BOOT button sampling
 *        and the pairing QR printer (LS-SAIC-001 section 8.5).
 */

#ifndef CGW_HMI_ESP_H
#define CGW_HMI_ESP_H

#include <stddef.h>
#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Button gesture detected by cgw_hmi_poll(). */
    typedef enum
    {
        CGW_HMI_BTN_NONE = 0,     /**< nothing */
        CGW_HMI_BTN_PAIR,         /**< released after t_pair_btn_hold_ms (< factory reset hold) */
        CGW_HMI_BTN_FACTORY_RESET /**< held for t_factory_reset_hold_ms */
    } cgw_hmi_btn_t;

    /**
 * @brief Initialise the status LED and the BOOT button input.
 *
 * @param[in] led_gpio  Addressable RGB LED (GPIO38 on DevKitC-1 v1.1, GPIO48 on v1.0).
 * @param[in] boot_gpio BOOT button (GPIO0, active low).
 * @retval CGW_OK   Ready.
 * @retval CGW_E_IO LED or GPIO error (the CGW keeps running without the LED).
 */
    cgw_rc_t cgw_hmi_init(int led_gpio, int boot_gpio);

    /**
 * @brief Select the indication shown on the LED.
 *
 * @param[in] ind Indication.
 */
    void cgw_hmi_set_indication(cgw_indication_t ind);

    /**
 * @brief Sample the button and refresh the LED; call every 20 ms from the sys task.
 *
 * The button is ignored for t_boot_btn_ignore_ms after boot (strapping pin).
 *
 * @param[in] now_ms Current time.
 * @return Detected gesture.
 */
    cgw_hmi_btn_t cgw_hmi_poll(uint32_t now_ms);

    /**
 * @brief Print the pairing URI as a QR code between the secret markers with raw stdout writes.
 *
 * Encoding: qrcodegen, ECC MEDIUM, versions 1-10, static buffers. Rendering: one line per module
 * row, two characters per module, dark = two spaces, light = two U+2588, 4-module quiet zone.
 * All buffers are zeroised afterwards.
 *
 * @param[in] uri NUL-terminated URI.
 * @param[in] len Length of @p uri.
 * @retval CGW_OK    Printed.
 * @retval CGW_E_ARG URI does not fit version 10.
 */
    cgw_rc_t cgw_hmi_print_qr(const char *uri, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CGW_HMI_ESP_H */
