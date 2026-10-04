/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_can_esp.h
 * @brief TWAI adapter (LS-SAIC-001 sections 4 and 7): on-chip node with the advanced 500 kbit/s
 *        timing, dual acceptance filter, static TX slots and IRAM callbacks that only post to
 *        the can_io queue.
 *
 * Implements cgw_can_port.h. can_io is the only caller of the port functions and the single
 * producer of twai_node_transmit().
 */

#ifndef CGW_CAN_ESP_H
#define CGW_CAN_ESP_H

#include <stdint.h>

#include "cgw_ports/cgw_can_port.h"
#include "cgw_ports/cgw_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief can_io queue event types. */
    typedef enum
    {
        CGW_CANIO_EV_RX = 0,  /**< frame received (ISR) */
        CGW_CANIO_EV_BUS,     /**< controller state change (ISR); u.state = twai_error_state_t */
        CGW_CANIO_EV_TX_DONE, /**< transmission finished (ISR); u.frame = slot frame */
        CGW_CANIO_EV_INTENT,  /**< window intent from the core */
        CGW_CANIO_EV_DOOR,    /**< door request from the core */
        CGW_CANIO_EV_NODE     /**< node information from the core */
    } cgw_canio_ev_t;

    /** @brief can_io queue item. */
    typedef struct
    {
        union
        {
            uint8_t data[CGW_CAN_DLC_MAX]; /**< CGW_CANIO_EV_RX payload */
            cgw_win_intent_t intent;       /**< CGW_CANIO_EV_INTENT */
            cgw_node_info_t node;          /**< CGW_CANIO_EV_NODE */
            struct
            {
                uint8_t req_id;  /**< CAN ReqId */
                uint8_t request; /**< Ls_DoorRequestType */
            } door;              /**< CGW_CANIO_EV_DOOR */
            uint32_t state;      /**< CGW_CANIO_EV_BUS */
            const void *frame;   /**< CGW_CANIO_EV_TX_DONE */
        } u;
        uint16_t id;  /**< CGW_CANIO_EV_RX identifier */
        uint8_t dlc;  /**< CGW_CANIO_EV_RX length */
        uint8_t type; /**< cgw_canio_ev_t */
    } cgw_canio_event_t;

    /** @brief Adapter configuration. */
    typedef struct
    {
        QueueHandle_t q; /**< can_io queue for the ISR events */
        int tx_gpio;     /**< TWAI_TX (GPIO4) */
        int rx_gpio;     /**< TWAI_RX (GPIO5) */
    } cgw_can_esp_cfg_t;

    /**
 * @brief Create, configure and enable the TWAI node. Call from the can_io task so the interrupt
 *        is allocated on its CPU.
 *
 * @param[in] cfg Configuration.
 * @retval CGW_OK   Node enabled; the controller is error-active.
 * @retval CGW_E_IO Driver error (B1B11, SAFE).
 */
    cgw_rc_t cgw_can_esp_start(const cgw_can_esp_cfg_t *cfg);

    /**
 * @brief Release the slot of a completed transmission.
 *
 * @param[in] frame Frame pointer reported by CGW_CANIO_EV_TX_DONE.
 */
    void cgw_can_esp_on_tx_done(const void *frame);

    /**
 * @brief Translate a controller state; at bus-off the busy slots are marked for the reclaim.
 *
 * @param[in] state twai_error_state_t from CGW_CANIO_EV_BUS.
 * @return Portable bus state.
 */
    cgw_bus_state_t cgw_can_esp_on_state(uint32_t state);

    /**
 * @brief Number of received frames lost because the can_io queue was full.
 *
 * @return Count.
 */
    uint32_t cgw_can_esp_rx_overflows(void);

#ifdef __cplusplus
}
#endif

#endif /* CGW_CAN_ESP_H */
