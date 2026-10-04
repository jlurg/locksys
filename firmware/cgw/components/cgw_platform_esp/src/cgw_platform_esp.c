/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_platform_esp/cgw_platform_esp.h"

#include <string.h>

#include "cgw_ports/cgw_clock_port.h"
#include "cgw_ports/cgw_system_port.h"
#include "cgw_ports/cgw_trace_port.h"
#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_rtc_time.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "ls_params_gen.h"
#include "mbedtls/sha256.h"
#include "sdkconfig.h"

#define PLAT_RMT_RES_HZ  (1000000u) /* 1 tick = 1 us */
#define PLAT_STORM_MAGIC (0x4C535753u)
#define PLAT_US_PER_MS   (1000u)

static const char *TAG = "cgw_plat";

/* TRACE0 pulse widths in us (LS-SAIC-001 section 3.5), indexed by cgw_trace_event_t. */
static const rmt_symbol_word_t k_trace_sym[CGW_TRACE_EV_COUNT] = {
    {.duration0 = 5u, .level0 = 1u, .duration1 = 1u, .level1 = 0u},
    {.duration0 = 20u, .level0 = 1u, .duration1 = 1u, .level1 = 0u},
    {.duration0 = 50u, .level0 = 1u, .duration1 = 1u, .level1 = 0u},
    {.duration0 = 100u, .level0 = 1u, .duration1 = 1u, .level1 = 0u},
    {.duration0 = 200u, .level0 = 1u, .duration1 = 1u, .level1 = 0u},
    {.duration0 = 500u, .level0 = 1u, .duration1 = 1u, .level1 = 0u},
};

static rmt_channel_handle_t s_trace0;
static rmt_encoder_handle_t s_trace_enc;
static int s_trace_pin[CGW_TRACE_PIN_COUNT] = {-1, -1, -1};
static volatile uint32_t s_alive_ms[CGW_ALIVE_COUNT];

/* Watchdog and panic reset record; survives software and watchdog resets. */
typedef struct
{
    uint32_t magic;
    uint32_t count;
    uint64_t t_first_us;
} plat_storm_t;

static RTC_NOINIT_ATTR plat_storm_t s_storm;

cgw_rc_t cgw_platform_ao_start(const cgw_ao_def_t *def, cgw_ao_t *ao)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((def != NULL) && (ao != NULL) && (def->stack != NULL) && (def->tcb != NULL))
    {
        ao->q = NULL;
        if ((def->q_len > 0u) && (def->q_storage != NULL) && (def->q_buf != NULL))
        {
            ao->q = xQueueCreateStatic(def->q_len, def->item_size, def->q_storage, def->q_buf);
        }
        ao->task = xTaskCreateStaticPinnedToCore(def->fn, def->name, def->stack_bytes, def->arg,
                                                 def->prio, def->stack, def->tcb, def->core);
        rc = (ao->task != NULL) ? CGW_OK : CGW_E_ARG;
    }
    return rc;
}

cgw_rc_t cgw_platform_ao_post(const cgw_ao_t *ao, const void *ev, uint32_t wait_ms)
{
    return (xQueueSendToBack(ao->q, ev, pdMS_TO_TICKS(wait_ms)) == pdTRUE) ? CGW_OK : CGW_E_FULL;
}

cgw_rc_t cgw_platform_ao_post_front(const cgw_ao_t *ao, const void *ev, uint32_t wait_ms)
{
    return (xQueueSendToFront(ao->q, ev, pdMS_TO_TICKS(wait_ms)) == pdTRUE) ? CGW_OK : CGW_E_FULL;
}

uint32_t cgw_clock_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / PLAT_US_PER_MS);
}

static cgw_rc_t plat_trace0_init(int pin)
{
    rmt_tx_channel_config_t ch_cfg = {
        .gpio_num = pin,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = PLAT_RMT_RES_HZ,
        .mem_block_symbols = 48u,
        .trans_queue_depth = 4u,
    };
    rmt_copy_encoder_config_t enc_cfg;
    esp_err_t err = rmt_new_tx_channel(&ch_cfg, &s_trace0);

    (void)memset(&enc_cfg, 0, sizeof(enc_cfg));

    if (err == ESP_OK)
    {
        err = rmt_new_copy_encoder(&enc_cfg, &s_trace_enc);
    }
    if (err == ESP_OK)
    {
        err = rmt_enable(s_trace0);
    }
    return (err == ESP_OK) ? CGW_OK : CGW_E_IO;
}

/* @satisfies SWR-CGW-066 */
cgw_rc_t cgw_platform_trace_init(const cgw_trace_cfg_t *cfg)
{
    cgw_rc_t rc = CGW_OK;
    const int pins[CGW_TRACE_PIN_COUNT] = {cfg->trace1, cfg->trace2, cfg->trace3};

    for (uint32_t i = 0u; i < (uint32_t)CGW_TRACE_PIN_COUNT; i++)
    {
        if (pins[i] >= 0)
        {
            gpio_config_t io = {
                .pin_bit_mask = 1ULL << (uint32_t)pins[i],
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };

            if ((gpio_config(&io) != ESP_OK) || (gpio_set_level(pins[i], 0u) != ESP_OK))
            {
                rc = CGW_E_IO;
            }
            s_trace_pin[i] = pins[i];
        }
    }
    if ((cfg->trace0 >= 0) && (plat_trace0_init(cfg->trace0) != CGW_OK))
    {
        rc = CGW_E_IO;
    }
    return rc;
}

void cgw_trace_event(cgw_trace_event_t ev)
{
    if ((s_trace0 != NULL) && ((uint32_t)ev < (uint32_t)CGW_TRACE_EV_COUNT))
    {
        rmt_transmit_config_t tx_cfg = {.loop_count = 0};

        (void)rmt_transmit(s_trace0, s_trace_enc, &k_trace_sym[ev], sizeof(k_trace_sym[ev]),
                           &tx_cfg);
    }
}

void cgw_trace_set(cgw_trace_pin_t pin, bool level)
{
    if (((uint32_t)pin < (uint32_t)CGW_TRACE_PIN_COUNT) && (s_trace_pin[pin] >= 0))
    {
        (void)gpio_set_level(s_trace_pin[pin], level ? 1u : 0u);
    }
}

void cgw_system_restart(void)
{
    ESP_LOGW(TAG, "restart requested");
    esp_restart();
}

cgw_reset_src_t cgw_platform_reset_source(void)
{
    cgw_reset_src_t src;

    switch (esp_reset_reason())
    {
        case ESP_RST_POWERON:
            src = CGW_RST_POWERON;
            break;
        case ESP_RST_EXT:
            src = CGW_RST_EXT;
            break;
        case ESP_RST_USB:
            src = CGW_RST_USB;
            break;
        case ESP_RST_JTAG:
            src = CGW_RST_JTAG;
            break;
        case ESP_RST_SW:
            src = CGW_RST_SW;
            break;
        case ESP_RST_PANIC:
            src = CGW_RST_PANIC;
            break;
        case ESP_RST_CPU_LOCKUP:
            src = CGW_RST_CPU_LOCKUP;
            break;
        case ESP_RST_INT_WDT:
            src = CGW_RST_INT_WDT;
            break;
        case ESP_RST_TASK_WDT:
            src = CGW_RST_TASK_WDT;
            break;
        case ESP_RST_WDT:
            src = CGW_RST_WDT;
            break;
        case ESP_RST_DEEPSLEEP:
            src = CGW_RST_DEEPSLEEP;
            break;
        case ESP_RST_BROWNOUT:
            src = CGW_RST_BROWNOUT;
            break;
        case ESP_RST_PWR_GLITCH:
            src = CGW_RST_PWR_GLITCH;
            break;
        default:
            src = CGW_RST_UNKNOWN;
            break;
    }
    return src;
}

uint8_t cgw_platform_reset_storm_count(cgw_reset_src_t src)
{
    uint64_t now_us = esp_rtc_get_time_us();
    bool storm_src = (src == CGW_RST_PANIC) || (src == CGW_RST_CPU_LOCKUP) ||
                     (src == CGW_RST_INT_WDT) || (src == CGW_RST_TASK_WDT) || (src == CGW_RST_WDT);

    if ((s_storm.magic != PLAT_STORM_MAGIC) || (src == CGW_RST_POWERON) ||
        (src == CGW_RST_BROWNOUT) ||
        ((now_us - s_storm.t_first_us) > ((uint64_t)LS_T_WDT_RESET_WINDOW_MS * PLAT_US_PER_MS)))
    {
        s_storm.magic = PLAT_STORM_MAGIC;
        s_storm.count = 0u;
        s_storm.t_first_us = now_us;
    }
    if (storm_src)
    {
        s_storm.count++;
    }
    return (s_storm.count > UINT8_MAX) ? UINT8_MAX : (uint8_t)s_storm.count;
}

void cgw_platform_alive_kick(cgw_alive_task_t task)
{
    if ((uint32_t)task < (uint32_t)CGW_ALIVE_COUNT)
    {
        s_alive_ms[task] = cgw_clock_now_ms();
    }
}

bool cgw_platform_alive_ok(uint32_t now_ms)
{
    bool ok = true;

    for (uint32_t i = 0u; i < (uint32_t)CGW_ALIVE_COUNT; i++)
    {
        if ((uint32_t)(now_ms - s_alive_ms[i]) > LS_T_CGW_ALIVE_DEADLINE_MS)
        {
            ok = false;
        }
    }
    return ok;
}

uint8_t cgw_platform_heap_free_pct(void)
{
    size_t total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t free_b = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    return (total > 0u) ? (uint8_t)((free_b * 100u) / total) : 0u;
}

cgw_rc_t cgw_platform_device_id(uint8_t *out)
{
    uint8_t mac[6];
    uint8_t hash[32];
    cgw_rc_t rc = CGW_E_IO;

    if ((esp_efuse_mac_get_default(mac) == ESP_OK) &&
        (mbedtls_sha256(mac, sizeof(mac), hash, 0) == 0))
    {
        (void)memcpy(out, hash, CGW_DEVICE_ID_LEN);
        rc = CGW_OK;
    }
    return rc;
}
