/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_main.c
 * @brief CGW entry point: start-up sequence of LS-CGW-SAD-001 sections 4, 8 and 12.
 *
 * Order: trace outputs, NVS, crypto self-test, identity, SoftAP passphrase (first boot inside the
 * bootloader random-enable bracket), pairing record, tasks (CGW_WinCmd starts as STOP), network.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "bootloader_random.h"
#include "cgw_crypto_psa/cgw_crypto_psa.h"
#include "cgw_hmi_esp/cgw_hmi_esp.h"
#include "cgw_pairing/cgw_pairing.h"
#include "cgw_platform_esp/cgw_platform_esp.h"
#include "cgw_ports/cgw_crypto_port.h"
#include "cgw_ports/cgw_keystore_port.h"
#include "cgw_store_nvs/cgw_store_nvs.h"
#include "cgw_wifi_esp/cgw_wifi_esp.h"
#include "cgw_wiring.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"

#if CONFIG_CGW_FAULT_INJECTION
#include "cgw_platform_esp/cgw_fi.h"
#endif

#define BOOT_GPIO (0)

static const char *TAG = "cgw_main";

/** @brief ESP-IDF application entry point. */
void app_main(void);

static cgw_core_identity_t s_id;
static cgw_pairing_record_t s_rec;

static void main_trace_init(void)
{
#if CONFIG_CGW_TRACE_PINS
    const cgw_trace_cfg_t cfg = {CONFIG_CGW_TRACE0_GPIO, CONFIG_CGW_TRACE1_GPIO,
                                 CONFIG_CGW_TRACE2_GPIO, CONFIG_CGW_TRACE3_GPIO};
#else
    const cgw_trace_cfg_t cfg = {-1, -1, -1, -1};
#endif

    if (cgw_platform_trace_init(&cfg) != CGW_OK)
    {
        ESP_LOGW(TAG, "trace outputs unavailable");
    }
}

/* First boot: the passphrase is generated inside the bootloader random-enable bracket, before
 * Wi-Fi starts (SWR-CGW-002, SWR-CGW-014). */
static bool main_load_passphrase(void)
{
    bool ok = cgw_keystore_load_passphrase(s_id.passphrase, sizeof(s_id.passphrase)) == CGW_OK;

    if (!ok)
    {
        bootloader_random_enable();
        cgw_crypto_psa_enable_rng(true);
        ok = (cgw_pairing_make_passphrase(s_id.passphrase, sizeof(s_id.passphrase)) == CGW_OK) &&
             (cgw_keystore_store_passphrase(s_id.passphrase) == CGW_OK);
        cgw_crypto_psa_enable_rng(false);
        bootloader_random_disable();
    }
    return ok;
}

static void main_identity(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    size_t n = strnlen(app->version, sizeof(s_id.fw_version) - 1u);

    (void)memset(&s_id, 0, sizeof(s_id));
    (void)memcpy(s_id.fw_version, app->version, n);
    if (cgw_platform_device_id(s_id.device_id) != CGW_OK)
    {
        ESP_LOGE(TAG, "device_id unavailable");
    }
    if (cgw_wifi_ap_mac(s_id.ap_mac) != CGW_OK)
    {
        ESP_LOGE(TAG, "SoftAP MAC unavailable");
    }
    cgw_pairing_ssid(s_id.ap_mac, s_id.ssid);
    s_id.reset_src = cgw_platform_reset_source();
    s_id.wdt_resets = cgw_platform_reset_storm_count(s_id.reset_src);
#if CONFIG_CGW_WIFI_TRANSITION
    s_id.wifi_transition = true;
#endif
}

void app_main(void)
{
    bool nvs_ok;
    bool crypto_ok;
    bool paired;

    main_trace_init();
    nvs_ok = cgw_store_nvs_init() == CGW_OK;
    crypto_ok = cgw_crypto_psa_init() == CGW_OK;
    main_identity();
    nvs_ok = nvs_ok && main_load_passphrase();
    paired = nvs_ok && (cgw_keystore_load_pairing(&s_rec) == CGW_OK);
    (void)cgw_hmi_init(CONFIG_CGW_STATUS_LED_GPIO, BOOT_GPIO);
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (cgw_wiring_start(&s_id, paired ? &s_rec : NULL) != CGW_OK)
    {
        ESP_LOGE(TAG, "task start failed");
        abort();
    }
    cgw_crypto_zeroize(&s_rec, sizeof(s_rec));
    if (!nvs_ok)
    {
        cgw_wiring_report_dtc(CGW_DTC_NVS, true);
    }
    if (!crypto_ok)
    {
        /* B1B16 is critical: the CGW enters SAFE and keeps CGW_WinCmd at STOP. */
        cgw_wiring_report_dtc(CGW_DTC_CRYPTO, true);
    }
    if (cgw_wiring_start_network(&s_id) != CGW_OK)
    {
        cgw_wiring_report_dtc(CGW_DTC_SOFTAP, true);
    }
    cgw_crypto_zeroize(s_id.passphrase, sizeof(s_id.passphrase));
#if CONFIG_CGW_FAULT_INJECTION
    (void)cgw_fi_init();
#endif
    ESP_LOGI(TAG, "LockSys CGW %s started (%s)", s_id.fw_version, s_id.ssid);
}
