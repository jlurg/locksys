/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_wifi_esp.h
 * @brief SoftAP adapter (LS-SAIC-001 section 8.1): WPA3-SAE only with PMF required and
 *        sae_pwe_h2e = BOTH (DEV transition mode optional), DHCP offers without router and DNS
 *        options, station table for controller-loss detection.
 */

#ifndef CGW_WIFI_ESP_H
#define CGW_WIFI_ESP_H

#include <stdbool.h>
#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief SoftAP events reported to the composition root. */
    typedef enum
    {
        CGW_WIFI_EV_AP_STARTED = 0, /**< WIFI_EVENT_AP_START */
        CGW_WIFI_EV_STA_JOIN,       /**< station associated */
        CGW_WIFI_EV_STA_LEAVE /**< station left; ipv4 = its address (0 if none was assigned) */
    } cgw_wifi_ev_t;

    /**
 * @brief Event callback; runs in the ESP-IDF event task and must only post an event.
 *
 * @param[in] ev       Event.
 * @param[in] ipv4     Station address in network byte order (STA_LEAVE), else 0.
 * @param[in] stations Associated stations after the event.
 */
    typedef void (*cgw_wifi_cb_t)(cgw_wifi_ev_t ev, uint32_t ipv4, uint8_t stations);

    /** @brief SoftAP configuration. */
    typedef struct
    {
        const char *ssid;       /**< "LockSys-XXXX" */
        const char *passphrase; /**< 20 base32 characters */
        const char *country;    /**< ISO 3166 country code, "01" = world-safe */
        uint16_t inactive_s;    /**< station inactivity time in s */
        uint8_t channel;        /**< 1, 6 or 11 */
        uint8_t max_conn;       /**< n_wifi_clients_max */
        bool transition;        /**< DEV only: WPA2/WPA3 transition mode */
        bool offer_router;      /**< offer the router option in DHCP (default false) */
    } cgw_wifi_cfg_t;

    /**
 * @brief Read the SoftAP MAC (BSSID); valid before the Wi-Fi driver starts.
 *
 * @param[out] mac 6 bytes.
 * @retval CGW_OK   MAC read.
 * @retval CGW_E_IO eFuse read error.
 */
    cgw_rc_t cgw_wifi_ap_mac(uint8_t *mac);

    /**
 * @brief Initialise netif and Wi-Fi in AP mode and start the SoftAP.
 *
 * Requires the default event loop. The driver keeps its configuration in RAM only.
 *
 * @param[in] cfg Configuration; strings are copied.
 * @param[in] cb  Event callback.
 * @retval CGW_OK    Start requested; WIFI_EVENT_AP_START follows.
 * @retval CGW_E_IO  Driver error.
 * @retval CGW_E_ARG Invalid configuration.
 */
    cgw_rc_t cgw_wifi_start(const cgw_wifi_cfg_t *cfg, cgw_wifi_cb_t cb);

    /**
 * @brief Restart the SoftAP after a start timeout (supervision by the sys task).
 *
 * @retval CGW_OK   Restart requested.
 * @retval CGW_E_IO Driver error.
 */
    cgw_rc_t cgw_wifi_restart(void);

#ifdef __cplusplus
}
#endif

#endif /* CGW_WIFI_ESP_H */
