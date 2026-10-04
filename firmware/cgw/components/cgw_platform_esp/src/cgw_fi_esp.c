/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_platform_esp/cgw_fi.h"

#include <stdlib.h>
#include <string.h>

#include "esp_console.h"
#include "esp_log.h"

static const char *TAG = "cgw_fi";

cgw_fi_state_t cgw_fi_state;

static uint32_t cgw_fi_arg(int argc, char **argv)
{
    return (argc > 2) ? (uint32_t)strtoul(argv[2], NULL, 10) : 1u;
}

static uint8_t cgw_fi_u8(uint32_t v)
{
    return (v > UINT8_MAX) ? UINT8_MAX : (uint8_t)v;
}

/* fi <name> [value]; "fi off" clears every injection. */
static int cgw_fi_cmd(int argc, char **argv)
{
    int rc = 0;
    const char *name = (argc > 1) ? argv[1] : "";
    uint32_t v = cgw_fi_arg(argc, argv);

    if (strcmp(name, "core_hang") == 0)
    {
        cgw_fi_state.core_hang_ms = v;
    }
    else if (strcmp(name, "canio_hang") == 0)
    {
        cgw_fi_state.canio_hang = true;
    }
    else if (strcmp(name, "can_silent") == 0)
    {
        cgw_fi_state.can_silent = true;
    }
    else if (strcmp(name, "e2e_crc") == 0)
    {
        cgw_fi_state.e2e_crc = cgw_fi_u8(v);
    }
    else if (strcmp(name, "e2e_ctr") == 0)
    {
        cgw_fi_state.e2e_ctr = cgw_fi_u8(v);
    }
    else if (strcmp(name, "drop_stop") == 0)
    {
        cgw_fi_state.drop_stop = true;
    }
    else if (strcmp(name, "wifi_off") == 0)
    {
        cgw_fi_state.wifi_off = true;
    }
    else if (strcmp(name, "off") == 0)
    {
        (void)memset(&cgw_fi_state, 0, sizeof(cgw_fi_state));
    }
    else
    {
        rc = 1;
    }
    ESP_LOGW(TAG, "fi %s %s", name, (rc == 0) ? "set" : "unknown");
    return rc;
}

cgw_rc_t cgw_fi_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    const esp_console_cmd_t cmd = {
        .command = "fi",
        .help = "fault injection: core_hang <ms> | canio_hang | can_silent | e2e_crc <n> | "
                "e2e_ctr <n> | drop_stop | wifi_off | off",
        .hint = NULL,
        .func = cgw_fi_cmd,
    };
    esp_err_t err;

    repl_cfg.prompt = "cgw>";
    repl_cfg.task_stack_size = 4096u;
    repl_cfg.task_priority = 2u;
    err = esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl);
    if (err == ESP_OK)
    {
        err = esp_console_cmd_register(&cmd);
    }
    if (err == ESP_OK)
    {
        err = esp_console_start_repl(repl);
    }
    return (err == ESP_OK) ? CGW_OK : CGW_E_IO;
}
