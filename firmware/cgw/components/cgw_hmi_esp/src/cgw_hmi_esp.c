/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_hmi_esp/cgw_hmi_esp.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "ls_params_gen.h"
#include "mbedtls/platform_util.h"
#include "qrcodegen.h"

#define HMI_QR_VERSION_MAX (10)
#define HMI_QR_BUF_LEN     ((size_t)qrcodegen_BUFFER_LEN_FOR_VERSION(HMI_QR_VERSION_MAX))
#define HMI_QR_QUIET       (4)
#define HMI_QR_SIDE_MAX    (17 + (4 * HMI_QR_VERSION_MAX) + (2 * HMI_QR_QUIET))
#define HMI_BLOCK          "\xE2\x96\x88\xE2\x96\x88" /* two U+2588 FULL BLOCK */
#define HMI_LINE_LEN       ((HMI_QR_SIDE_MAX * 6) + 2)
#define HMI_BLINK_MS       (500u)
#define HMI_LEVEL          (16u)

static const char *TAG = "cgw_hmi";

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool blink;
} hmi_color_t;

/* Indexed by cgw_indication_t. */
static const hmi_color_t k_colors[] = {
    {0u, 0u, 0u, false},                    /* OFF */
    {HMI_LEVEL, HMI_LEVEL, 0u, false},      /* STARTING: yellow */
    {0u, HMI_LEVEL, 0u, false},             /* READY: green */
    {0u, HMI_LEVEL, HMI_LEVEL, false},      /* CONNECTED: cyan */
    {0u, 0u, HMI_LEVEL, true},              /* PAIRING: blinking blue */
    {HMI_LEVEL, HMI_LEVEL / 4u, 0u, false}, /* DEGRADED: orange */
    {HMI_LEVEL, 0u, 0u, false},             /* FAULT: red */
};

static led_strip_handle_t s_led;
static int s_boot_gpio = -1;
static cgw_indication_t s_ind;
static uint32_t s_press_ms;
static bool s_pressed;
static bool s_reset_sent;
static uint8_t s_qr[HMI_QR_BUF_LEN];
static uint8_t s_qr_tmp[HMI_QR_BUF_LEN];
static char s_line[HMI_LINE_LEN];

cgw_rc_t cgw_hmi_init(int led_gpio, int boot_gpio)
{
    const led_strip_config_t strip = {
        .strip_gpio_num = led_gpio,
        .max_leds = 1u,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {.invert_out = 0u},
    };
    const led_strip_rmt_config_t rmt = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000u,
        .mem_block_symbols = 0u,
        .flags = {.with_dma = 0u},
    };
    const gpio_config_t btn = {
        .pin_bit_mask = 1ULL << (uint32_t)boot_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = led_strip_new_rmt_device(&strip, &rmt, &s_led);

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "status LED unavailable: %s", esp_err_to_name(err));
        s_led = NULL;
    }
    if (gpio_config(&btn) == ESP_OK)
    {
        s_boot_gpio = boot_gpio;
    }
    else
    {
        err = ESP_FAIL;
    }
    return (err == ESP_OK) ? CGW_OK : CGW_E_IO;
}

void cgw_hmi_set_indication(cgw_indication_t ind)
{
    if ((size_t)ind < (sizeof(k_colors) / sizeof(k_colors[0])))
    {
        s_ind = ind;
    }
}

static void hmi_led_refresh(uint32_t now_ms)
{
    if (s_led != NULL)
    {
        const hmi_color_t *c = &k_colors[s_ind];
        bool on = !c->blink || (((now_ms / HMI_BLINK_MS) & 1u) == 0u);

        (void)led_strip_set_pixel(s_led, 0u, on ? c->r : 0u, on ? c->g : 0u, on ? c->b : 0u);
        (void)led_strip_refresh(s_led);
    }
}

/* @satisfies SWR-CGW-016 */
/* @satisfies SWR-CGW-019 */
cgw_hmi_btn_t cgw_hmi_poll(uint32_t now_ms)
{
    cgw_hmi_btn_t ev = CGW_HMI_BTN_NONE;
    bool down = (s_boot_gpio >= 0) && (now_ms >= LS_T_BOOT_BTN_IGNORE_MS) &&
                (gpio_get_level(s_boot_gpio) == 0);

    if (down && !s_pressed)
    {
        s_pressed = true;
        s_reset_sent = false;
        s_press_ms = now_ms;
    }
    else if (down && !s_reset_sent && ((now_ms - s_press_ms) >= LS_T_FACTORY_RESET_HOLD_MS))
    {
        s_reset_sent = true;
        ev = CGW_HMI_BTN_FACTORY_RESET;
    }
    else if (!down && s_pressed)
    {
        s_pressed = false;
        if (!s_reset_sent && ((now_ms - s_press_ms) >= LS_T_PAIR_BTN_HOLD_MS))
        {
            ev = CGW_HMI_BTN_PAIR;
        }
    }
    else
    {
        /* No change. */
    }
    hmi_led_refresh(now_ms);
    return ev;
}

static void hmi_print_row(int y, int size)
{
    size_t n = 0u;

    for (int x = -HMI_QR_QUIET; x < (size + HMI_QR_QUIET); x++)
    {
        bool dark = qrcodegen_getModule(s_qr, x, y);
        const char *cell = dark ? "  " : HMI_BLOCK;
        size_t cl = strlen(cell);

        (void)memcpy(&s_line[n], cell, cl);
        n += cl;
    }
    s_line[n] = '\n';
    n++;
    (void)fwrite(s_line, 1u, n, stdout);
}

/* @satisfies SWR-CGW-017 */
cgw_rc_t cgw_hmi_print_qr(const char *uri, size_t len)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((uri != NULL) && (len > 0u) &&
        qrcodegen_encodeText(uri, s_qr_tmp, s_qr, qrcodegen_Ecc_MEDIUM, 1, HMI_QR_VERSION_MAX,
                             qrcodegen_Mask_AUTO, true))
    {
        int size = qrcodegen_getSize(s_qr);

        (void)fputs("<<LS-SECRET-BEGIN>>\n", stdout);
        for (int y = -HMI_QR_QUIET; y < (size + HMI_QR_QUIET); y++)
        {
            hmi_print_row(y, size);
        }
        (void)fputs("<<LS-SECRET-END>>\n", stdout);
        (void)fflush(stdout);
        rc = CGW_OK;
    }
    mbedtls_platform_zeroize(s_qr, sizeof(s_qr));
    mbedtls_platform_zeroize(s_qr_tmp, sizeof(s_qr_tmp));
    mbedtls_platform_zeroize(s_line, sizeof(s_line));
    return rc;
}
