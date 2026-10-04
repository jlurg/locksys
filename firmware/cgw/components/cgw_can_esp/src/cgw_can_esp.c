/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_can_esp/cgw_can_esp.h"

#include <stdbool.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "ls_can_matrix_gen.h"

#define CAN_TX_MSGS        (4u)
#define CAN_SLOTS_PER_MSG  (2u)
#define CAN_TX_QUEUE_DEPTH (8u)

static const char *TAG = "cgw_can";

typedef struct
{
    twai_frame_t frame;
    uint8_t buf[CGW_CAN_DLC_MAX];
    volatile bool busy;
    bool suspect;
} can_slot_t;

static const uint16_t k_tx_ids[CAN_TX_MSGS] = {
    (uint16_t)LS_CAN_CGW_WIN_CMD_ID, (uint16_t)LS_CAN_CGW_DOOR_CMD_ID,
    (uint16_t)LS_CAN_CGW_NODE_STS_ID, (uint16_t)LS_CAN_CGW_VERSION_ID};

static DRAM_ATTR QueueHandle_t s_q;
static DRAM_ATTR uint32_t s_rx_overflow;
static twai_node_handle_t s_node;
static can_slot_t s_slots[CAN_TX_MSGS][CAN_SLOTS_PER_MSG];

static bool IRAM_ATTR can_on_rx(twai_node_handle_t h, const twai_rx_done_event_data_t *ed,
                                void *ctx)
{
    uint8_t buf[CGW_CAN_DLC_MAX] = {0};
    twai_frame_t f = {.buffer = buf, .buffer_len = sizeof(buf)};
    BaseType_t woken = pdFALSE;

    (void)ed;
    (void)ctx;
    if ((twai_node_receive_from_isr(h, &f) == ESP_OK) && (f.header.ide == 0u) &&
        (f.header.rtr == 0u))
    {
        cgw_canio_event_t ev;

        ev.type = (uint8_t)CGW_CANIO_EV_RX;
        ev.id = (uint16_t)f.header.id;
        ev.dlc = (uint8_t)((f.header.dlc < CGW_CAN_DLC_MAX) ? f.header.dlc : CGW_CAN_DLC_MAX);
        for (uint32_t i = 0u; i < CGW_CAN_DLC_MAX; i++)
        {
            ev.u.data[i] = buf[i];
        }
        if (xQueueSendFromISR(s_q, &ev, &woken) != pdTRUE)
        {
            s_rx_overflow++;
        }
    }
    return woken == pdTRUE;
}

static bool IRAM_ATTR can_on_tx_done(twai_node_handle_t h, const twai_tx_done_event_data_t *ed,
                                     void *ctx)
{
    cgw_canio_event_t ev;
    BaseType_t woken = pdFALSE;

    (void)h;
    (void)ctx;
    ev.type = (uint8_t)CGW_CANIO_EV_TX_DONE;
    ev.u.frame = ed->done_tx_frame;
    (void)xQueueSendFromISR(s_q, &ev, &woken);
    return woken == pdTRUE;
}

static bool IRAM_ATTR can_on_state(twai_node_handle_t h, const twai_state_change_event_data_t *ed,
                                   void *ctx)
{
    cgw_canio_event_t ev;
    BaseType_t woken = pdFALSE;

    (void)h;
    (void)ctx;
    ev.type = (uint8_t)CGW_CANIO_EV_BUS;
    ev.u.state = (uint32_t)ed->new_sta;
    (void)xQueueSendFromISR(s_q, &ev, &woken);
    return woken == pdTRUE;
}

static int can_msg_index(uint16_t id)
{
    int idx = -1;

    for (uint32_t i = 0u; (i < CAN_TX_MSGS) && (idx < 0); i++)
    {
        if (k_tx_ids[i] == id)
        {
            idx = (int)i;
        }
    }
    return idx;
}

/* @satisfies SWR-CGW-040 */
cgw_rc_t cgw_can_esp_start(const cgw_can_esp_cfg_t *cfg)
{
    const twai_onchip_node_config_t node_cfg = {
        .io_cfg = {.tx = (gpio_num_t)cfg->tx_gpio,
                   .rx = (gpio_num_t)cfg->rx_gpio,
                   .quanta_clk_out = GPIO_NUM_NC,
                   .bus_off_indicator = GPIO_NUM_NC},
        .clk_src = TWAI_CLK_SRC_DEFAULT,
        .bit_timing = {.bitrate = LS_CAN_BAUDRATE},
        .fail_retry_cnt = -1, /* any other value is single-shot on ESP32-S3 */
        .tx_queue_depth = CAN_TX_QUEUE_DEPTH,
        .flags = {.no_receive_rtr = 1u},
    };
    /* 80 MHz / 10 = 125 ns tq; 1 + 13 + 2 = 16 tq = 2 us; sample point 87.5 % (LS-SAIC-001 4). */
    const twai_timing_advanced_config_t timing = {
        .brp = 10u, .prop_seg = 0u, .tseg_1 = 13u, .tseg_2 = 2u, .sjw = 2u, .ssp_offset = 0u};
    /* Filter 1: 0x200-0x23F (DCU status incl. DCU_WinMotion 0x201); filter 2: 0x510 and 0x590. */
    const twai_mask_filter_config_t filter =
        twai_make_dual_filter(0x200u, 0x7C0u, 0x510u, 0x77Fu, false);
    const twai_event_callbacks_t cbs = {
        .on_tx_done = can_on_tx_done,
        .on_rx_done = can_on_rx,
        .on_state_change = can_on_state,
        .on_error = NULL,
    };
    esp_err_t err;

    s_q = cfg->q;
    for (uint32_t m = 0u; m < CAN_TX_MSGS; m++)
    {
        for (uint32_t s = 0u; s < CAN_SLOTS_PER_MSG; s++)
        {
            s_slots[m][s].frame.buffer = s_slots[m][s].buf;
        }
    }
    err = twai_new_node_onchip(&node_cfg, &s_node);
    if (err == ESP_OK)
    {
        err = twai_node_reconfig_timing(s_node, &timing, NULL);
    }
    if (err == ESP_OK)
    {
        err = twai_node_config_mask_filter(s_node, 0u, &filter);
    }
    if (err == ESP_OK)
    {
        err = twai_node_register_event_callbacks(s_node, &cbs, NULL);
    }
    if (err == ESP_OK)
    {
        err = twai_node_enable(s_node);
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "TWAI start failed: %s", esp_err_to_name(err));
    }
    return (err == ESP_OK) ? CGW_OK : CGW_E_IO;
}

/* @satisfies SWR-CGW-045 */
cgw_rc_t cgw_can_tx(uint16_t id, const uint8_t *data, uint8_t dlc)
{
    int m = can_msg_index(id);
    cgw_rc_t rc = CGW_E_ARG;

    if ((m >= 0) && (data != NULL) && (dlc <= CGW_CAN_DLC_MAX) && (s_node != NULL))
    {
        can_slot_t *slot = NULL;

        for (uint32_t s = 0u; (s < CAN_SLOTS_PER_MSG) && (slot == NULL); s++)
        {
            if (!s_slots[m][s].busy)
            {
                slot = &s_slots[m][s];
            }
        }
        rc = CGW_E_FULL;
        if (slot != NULL)
        {
            esp_err_t err;

            (void)memcpy(slot->buf, data, dlc);
            (void)memset(&slot->frame.header, 0, sizeof(slot->frame.header));
            slot->frame.header.id = id;
            slot->frame.header.dlc = dlc;
            slot->frame.buffer_len = dlc;
            slot->suspect = false;
            slot->busy = true;
            err = twai_node_transmit(s_node, &slot->frame, 0);
            if (err != ESP_OK)
            {
                slot->busy = false;
                rc = (err == ESP_ERR_INVALID_STATE) ? CGW_E_STATE : CGW_E_FULL;
            }
            else
            {
                rc = CGW_OK;
            }
        }
    }
    return rc;
}

cgw_rc_t cgw_can_recover(void)
{
    return ((s_node != NULL) && (twai_node_recover(s_node) == ESP_OK)) ? CGW_OK : CGW_E_STATE;
}

/* @satisfies SWR-CGW-046 */
uint8_t cgw_can_rewrite_busy(uint16_t id, const uint8_t frames[][CGW_CAN_DLC_MAX], uint8_t n_frames,
                             uint8_t dlc)
{
    int m = can_msg_index(id);
    uint8_t n = 0u;

    if ((m >= 0) && (frames != NULL) && (dlc <= CGW_CAN_DLC_MAX))
    {
        for (uint32_t s = 0u; (s < CAN_SLOTS_PER_MSG) && (n < n_frames); s++)
        {
            if (s_slots[m][s].busy)
            {
                (void)memcpy(s_slots[m][s].buf, frames[n], dlc);
                n++;
            }
        }
    }
    return n;
}

uint8_t cgw_can_reclaim_suspect(void)
{
    uint8_t n = 0u;

    for (uint32_t m = 0u; m < CAN_TX_MSGS; m++)
    {
        for (uint32_t s = 0u; s < CAN_SLOTS_PER_MSG; s++)
        {
            if (s_slots[m][s].suspect && s_slots[m][s].busy)
            {
                s_slots[m][s].busy = false;
                n++;
            }
            s_slots[m][s].suspect = false;
        }
    }
    return n;
}

void cgw_can_esp_on_tx_done(const void *frame)
{
    for (uint32_t m = 0u; m < CAN_TX_MSGS; m++)
    {
        for (uint32_t s = 0u; s < CAN_SLOTS_PER_MSG; s++)
        {
            if ((const void *)&s_slots[m][s].frame == frame)
            {
                s_slots[m][s].busy = false;
                s_slots[m][s].suspect = false;
            }
        }
    }
}

cgw_bus_state_t cgw_can_esp_on_state(uint32_t state)
{
    cgw_bus_state_t bus;

    switch ((twai_error_state_t)state)
    {
        case TWAI_ERROR_ACTIVE:
            bus = CGW_BUS_ACTIVE;
            break;
        case TWAI_ERROR_WARNING:
            bus = CGW_BUS_WARNING;
            break;
        case TWAI_ERROR_PASSIVE:
            bus = CGW_BUS_PASSIVE;
            break;
        default:
            bus = CGW_BUS_OFF;
            break;
    }
    if (bus == CGW_BUS_OFF)
    {
        for (uint32_t m = 0u; m < CAN_TX_MSGS; m++)
        {
            for (uint32_t s = 0u; s < CAN_SLOTS_PER_MSG; s++)
            {
                s_slots[m][s].suspect = s_slots[m][s].busy;
            }
        }
    }
    return bus;
}

uint32_t cgw_can_esp_rx_overflows(void)
{
    return s_rx_overflow;
}
