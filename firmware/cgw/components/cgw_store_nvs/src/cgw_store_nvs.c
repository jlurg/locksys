/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_store_nvs/cgw_store_nvs.h"

#include <string.h>

#include "cgw_ports/cgw_keystore_port.h"
#include "nvs.h"
#include "nvs_flash.h"

#define STORE_NS        "cgw_sec"
#define STORE_KEY_KPAIR "kpair"
#define STORE_KEY_GEN   "kpair_gen"
#define STORE_KEY_CID   "client_id"
#define STORE_KEY_PASS  "wifi_pass"

cgw_rc_t cgw_store_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();

    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        err = nvs_flash_erase();
        if (err == ESP_OK)
        {
            err = nvs_flash_init();
        }
    }
    return (err == ESP_OK) ? CGW_OK : CGW_E_IO;
}

static cgw_rc_t store_rc(esp_err_t err)
{
    cgw_rc_t rc;

    if (err == ESP_OK)
    {
        rc = CGW_OK;
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        rc = CGW_E_NOT_FOUND;
    }
    else
    {
        rc = CGW_E_IO;
    }
    return rc;
}

static esp_err_t store_get_blob(nvs_handle_t h, const char *key, void *buf, size_t len)
{
    size_t got = len;
    esp_err_t err = nvs_get_blob(h, key, buf, &got);

    if ((err == ESP_OK) && (got != len))
    {
        err = ESP_ERR_NVS_INVALID_LENGTH;
    }
    return err;
}

cgw_rc_t cgw_keystore_load_pairing(cgw_pairing_record_t *rec)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(STORE_NS, NVS_READONLY, &h);

    if (err == ESP_OK)
    {
        err = store_get_blob(h, STORE_KEY_KPAIR, rec->k_pair, sizeof(rec->k_pair));
        if (err == ESP_OK)
        {
            err = store_get_blob(h, STORE_KEY_CID, rec->client_id, sizeof(rec->client_id));
        }
        if (err == ESP_OK)
        {
            err = nvs_get_u32(h, STORE_KEY_GEN, &rec->generation);
        }
        nvs_close(h);
    }
    if (err != ESP_OK)
    {
        (void)memset(rec, 0, sizeof(*rec));
    }
    return store_rc(err);
}

cgw_rc_t cgw_keystore_store_pairing(const cgw_pairing_record_t *rec)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(STORE_NS, NVS_READWRITE, &h);

    if (err == ESP_OK)
    {
        err = nvs_set_blob(h, STORE_KEY_KPAIR, rec->k_pair, sizeof(rec->k_pair));
        if (err == ESP_OK)
        {
            err = nvs_set_blob(h, STORE_KEY_CID, rec->client_id, sizeof(rec->client_id));
        }
        if (err == ESP_OK)
        {
            err = nvs_set_u32(h, STORE_KEY_GEN, rec->generation);
        }
        if (err == ESP_OK)
        {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    return (err == ESP_OK) ? CGW_OK : CGW_E_IO;
}

cgw_rc_t cgw_keystore_load_passphrase(char *buf, size_t size)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((buf != NULL) && (size > CGW_PASSPHRASE_LEN))
    {
        nvs_handle_t h;
        size_t len = size;
        esp_err_t err = nvs_open(STORE_NS, NVS_READONLY, &h);

        if (err == ESP_OK)
        {
            err = nvs_get_str(h, STORE_KEY_PASS, buf, &len);
            nvs_close(h);
        }
        if ((err == ESP_OK) && (strlen(buf) != CGW_PASSPHRASE_LEN))
        {
            err = ESP_ERR_NVS_INVALID_LENGTH;
        }
        rc = store_rc(err);
    }
    return rc;
}

cgw_rc_t cgw_keystore_store_passphrase(const char *passphrase)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(STORE_NS, NVS_READWRITE, &h);

    if (err == ESP_OK)
    {
        err = nvs_set_str(h, STORE_KEY_PASS, passphrase);
        if (err == ESP_OK)
        {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    return (err == ESP_OK) ? CGW_OK : CGW_E_IO;
}

/* @satisfies SWR-CGW-068 */
cgw_rc_t cgw_keystore_erase(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(STORE_NS, NVS_READWRITE, &h);

    if (err == ESP_OK)
    {
        err = nvs_erase_all(h);
        if (err == ESP_OK)
        {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    return (err == ESP_OK) ? CGW_OK : CGW_E_IO;
}
