/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file fake_ports.h
 * @brief Host-test fakes of the CGW ports (link, can_io, CAN, console, trace, RNG, key store,
 *        system). State is global and cleared by fake_ports_reset().
 */

#ifndef FAKE_PORTS_H
#define FAKE_PORTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cgw_ports/cgw_can_port.h"
#include "cgw_ports/cgw_keystore_port.h"
#include "cgw_ports/cgw_trace_port.h"
#include "cgw_ports/cgw_types.h"

#define FAKE_LINK_MAX  (32u)
#define FAKE_CAN_MAX   (64u)
#define FAKE_CLOSE_MAX (8u)

/** @brief One message queued through cgw_link_send(). */
typedef struct
{
    cgw_conn_t conn;
    uint8_t data[CGW_WS_FRAME_MAX];
    size_t len;
    cgw_prio_t prio;
} fake_link_msg_t;

/** @brief One frame handed to cgw_can_tx(). */
typedef struct
{
    uint16_t id;
    uint8_t data[CGW_CAN_DLC_MAX];
    uint8_t dlc;
} fake_can_frame_t;

/** @brief Recorded port activity and configurable results. */
typedef struct
{
    fake_link_msg_t link[FAKE_LINK_MAX];
    uint32_t link_count;
    cgw_rc_t link_rc;
    cgw_conn_t closed[FAKE_CLOSE_MAX];
    uint16_t close_code[FAKE_CLOSE_MAX];
    uint32_t close_count;
    cgw_conn_t authenticated;
    cgw_conn_t peer_conn;
    uint32_t flushes;

    cgw_win_intent_t intent;
    uint32_t intent_count;
    uint8_t door_req_id;
    uint8_t door_request;
    uint32_t door_count;
    cgw_node_info_t node;
    uint32_t node_count;
    cgw_rc_t canio_rc;

    fake_can_frame_t can[FAKE_CAN_MAX];
    uint32_t can_count;
    cgw_rc_t can_rc;
    uint8_t can_busy;
    uint8_t can_rewritten[2][CGW_CAN_DLC_MAX];
    uint32_t can_recover;
    uint8_t can_reclaim;

    char qr[CGW_WS_FRAME_MAX + 1u];
    size_t qr_len;
    uint32_t qr_count;
    cgw_indication_t indication;

    uint32_t trace_events[CGW_TRACE_EV_COUNT];
    bool trace_level[CGW_TRACE_PIN_COUNT];
    uint32_t trace_pulses[CGW_TRACE_PIN_COUNT];

    bool rng_enabled;
    uint8_t rng[64];
    size_t rng_len;
    size_t rng_pos;

    cgw_pairing_record_t ks_rec;
    bool ks_has_rec;
    char ks_pass[CGW_PASSPHRASE_LEN + 1u];
    bool ks_has_pass;
    cgw_rc_t ks_rc;
    uint32_t ks_erase;

    uint32_t restarts;
} fake_ports_t;

/** @brief Fake state. */
extern fake_ports_t fake;

/** @brief Clear every fake: RNG enabled with an incrementing byte pattern, all results CGW_OK. */
void fake_ports_reset(void);

/**
 * @brief Script the bytes returned by cgw_rng_fill(); the script repeats.
 *
 * @param[in] bytes Bytes.
 * @param[in] len   Number of bytes (1 .. 64).
 */
void fake_rng_script(const uint8_t *bytes, size_t len);

/**
 * @brief Decode a hex string into bytes.
 *
 * @param[in]  hex Hex digits.
 * @param[out] out Destination.
 * @param[in]  max Capacity of @p out.
 * @return Number of bytes written.
 */
size_t fake_hex(const char *hex, uint8_t *out, size_t max);

#endif /* FAKE_PORTS_H */
