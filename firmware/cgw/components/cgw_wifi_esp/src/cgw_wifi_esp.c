/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_wifi_esp/cgw_wifi_esp.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#define WIFI_STA_MAX   (4u)
#define WIFI_BEACON_TU (100u)

static const char *TAG = "cgw_wifi";

typedef struct
{
    uint8_t mac[6];
    uint32_t ipv4;
    bool used;
} wifi_sta_t;

static cgw_wifi_cb_t s_cb;
static wifi_sta_t s_sta[WIFI_STA_MAX];
static uint8_t s_stations;

static wifi_sta_t *wifi_sta_find(const uint8_t *mac)
{
    wifi_sta_t *found = NULL;

    for (uint32_t i = 0u; (i < WIFI_STA_MAX) && (found == NULL); i++)
    {
        if (s_sta[i].used && (memcmp(s_sta[i].mac, mac, sizeof(s_sta[i].mac)) == 0))
        {
            found = &s_sta[i];
        }
    }
    return found;
}

static void wifi_sta_join(const uint8_t *mac)
{
    wifi_sta_t *e = wifi_sta_find(mac);

    for (uint32_t i = 0u; (i < WIFI_STA_MAX) && (e == NULL); i++)
    {
        if (!s_sta[i].used)
        {
            e = &s_sta[i];
        }
    }
    if (e != NULL)
    {
        (void)memcpy(e->mac, mac, sizeof(e->mac));
        e->ipv4 = 0u;
        e->used = true;
    }
    if (s_stations < UINT8_MAX)
    {
        s_stations++;
    }
}

static uint32_t wifi_sta_leave(const uint8_t *mac)
{
    wifi_sta_t *e = wifi_sta_find(mac);
    uint32_t ipv4 = 0u;

    if (e != NULL)
    {
        ipv4 = e->ipv4;
        e->used = false;
    }
    if (s_stations > 0u)
    {
        s_stations--;
    }
    return ipv4;
}

static void wifi_on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if ((base == WIFI_EVENT) && (id == WIFI_EVENT_AP_START))
    {
        s_cb(CGW_WIFI_EV_AP_STARTED, 0u, s_stations);
    }
    else if ((base == WIFI_EVENT) && (id == WIFI_EVENT_AP_STACONNECTED))
    {
        const wifi_event_ap_staconnected_t *ev = data;

        wifi_sta_join(ev->mac);
        s_cb(CGW_WIFI_EV_STA_JOIN, 0u, s_stations);
    }
    else if ((base == WIFI_EVENT) && (id == WIFI_EVENT_AP_STADISCONNECTED))
    {
        const wifi_event_ap_stadisconnected_t *ev = data;
        uint32_t ipv4 = wifi_sta_leave(ev->mac);

        s_cb(CGW_WIFI_EV_STA_LEAVE, ipv4, s_stations);
    }
    else if ((base == IP_EVENT) && (id == IP_EVENT_AP_STAIPASSIGNED))
    {
        const ip_event_ap_staipassigned_t *ev = data;
        wifi_sta_t *e = wifi_sta_find(ev->mac);

        if (e != NULL)
        {
            e->ipv4 = ev->ip.addr;
        }
    }
    else
    {
        /* Other events are not used. */
    }
}

cgw_rc_t cgw_wifi_ap_mac(uint8_t *mac)
{
    return (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) == ESP_OK) ? CGW_OK : CGW_E_IO;
}

/* DHCP offers without the router option unless configured (LS-SAIC-001 section 8.1). */
static esp_err_t wifi_netif_init(bool offer_router)
{
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    esp_err_t err = (netif != NULL) ? ESP_OK : ESP_FAIL;

    if ((err == ESP_OK) && !offer_router)
    {
        uint8_t offer = 0u;

        (void)esp_netif_dhcps_stop(netif);
        err = esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,
                                     &offer, sizeof(offer));
        if (err == ESP_OK)
        {
            err = esp_netif_dhcps_start(netif);
        }
    }
    return err;
}

static void wifi_fill_config(const cgw_wifi_cfg_t *cfg, wifi_config_t *wc)
{
    (void)memset(wc, 0, sizeof(*wc));
    (void)strncpy((char *)wc->ap.ssid, cfg->ssid, sizeof(wc->ap.ssid) - 1u);
    (void)strncpy((char *)wc->ap.password, cfg->passphrase, sizeof(wc->ap.password) - 1u);
    wc->ap.ssid_len = (uint8_t)strlen((const char *)wc->ap.ssid);
    wc->ap.channel = cfg->channel;
    wc->ap.max_connection = cfg->max_conn;
    wc->ap.beacon_interval = WIFI_BEACON_TU;
    wc->ap.dtim_period = 1u;
    wc->ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
    wc->ap.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    if (cfg->transition)
    {
        wc->ap.authmode = WIFI_AUTH_WPA2_WPA3_PSK;
        wc->ap.pmf_cfg.capable = true;
        wc->ap.pmf_cfg.required = false;
        wc->ap.transition_disable = 0u;
    }
    else
    {
        wc->ap.authmode = WIFI_AUTH_WPA3_PSK;
        wc->ap.pmf_cfg.capable = true;
        wc->ap.pmf_cfg.required = true;
    }
}

static esp_err_t wifi_driver_init(bool offer_router)
{
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_netif_init();

    if (err == ESP_OK)
    {
        err = wifi_netif_init(offer_router);
    }
    if (err == ESP_OK)
    {
        err = esp_wifi_init(&init);
    }
    if (err == ESP_OK)
    {
        err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_on_event, NULL);
    }
    if (err == ESP_OK)
    {
        err = esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, wifi_on_event, NULL);
    }
    return err;
}

static esp_err_t wifi_driver_configure(const cgw_wifi_cfg_t *cfg)
{
    wifi_config_t wc;
    esp_err_t err = esp_wifi_set_storage(WIFI_STORAGE_RAM);

    if (err == ESP_OK)
    {
        err = esp_wifi_set_mode(WIFI_MODE_AP);
    }
    if (err == ESP_OK)
    {
        err = esp_wifi_set_country_code(cfg->country, true);
    }
    if (err == ESP_OK)
    {
        wifi_fill_config(cfg, &wc);
        err = esp_wifi_set_config(WIFI_IF_AP, &wc);
        (void)memset(&wc, 0, sizeof(wc));
    }
    if (err == ESP_OK)
    {
        err = esp_wifi_set_inactive_time(WIFI_IF_AP, cfg->inactive_s);
    }
    return err;
}

/* @satisfies SWR-CGW-001 */
cgw_rc_t cgw_wifi_start(const cgw_wifi_cfg_t *cfg, cgw_wifi_cb_t cb)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((cfg != NULL) && (cb != NULL) && (cfg->ssid != NULL) && (cfg->passphrase != NULL))
    {
        esp_err_t err;

        s_cb = cb;
        err = wifi_driver_init(cfg->offer_router);
        if (err == ESP_OK)
        {
            err = wifi_driver_configure(cfg);
        }
        if (err == ESP_OK)
        {
            err = esp_wifi_start();
        }
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "SoftAP start failed: %s", esp_err_to_name(err));
        }
        rc = (err == ESP_OK) ? CGW_OK : CGW_E_IO;
    }
    return rc;
}

/* @satisfies SWR-CGW-004 */
cgw_rc_t cgw_wifi_restart(void)
{
    esp_err_t err = esp_wifi_stop();

    if (err == ESP_OK)
    {
        err = esp_wifi_start();
    }
    return (err == ESP_OK) ? CGW_OK : CGW_E_IO;
}
