/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_wiring.h
 * @brief Composition root: static active objects (core, can_io, sys), their task bodies and the
 *        bindings of the can_io and console ports (LS-CGW-SAD-001 section 4).
 */

#ifndef CGW_WIRING_H
#define CGW_WIRING_H

#include <stdbool.h>
#include <stdint.h>

#include "cgw_core/cgw_core.h"
#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief Initialise the core state and start the core, can_io and sys tasks.
 *
 * @param[in] id  Identity.
 * @param[in] rec Committed pairing, or NULL.
 * @retval CGW_OK    Tasks running.
 * @retval CGW_E_ARG A task could not be created.
 */
    cgw_rc_t cgw_wiring_start(const cgw_core_identity_t *id, const cgw_pairing_record_t *rec);

    /**
 * @brief Post an event to the core queue.
 *
 * @param[in] ev      Event; copied.
 * @param[in] wait_ms Bounded wait: 20 ms for control events, 0 for data events.
 * @retval CGW_OK     Posted.
 * @retval CGW_E_FULL Queue full (counted).
 */
    cgw_rc_t cgw_wiring_post_core(const cgw_core_event_t *ev, uint32_t wait_ms);

    /**
 * @brief Report a DTC test result to the core.
 *
 * @param[in] dtc    DTC.
 * @param[in] failed Test failed.
 */
    void cgw_wiring_report_dtc(cgw_dtc_t dtc, bool failed);

    /**
 * @brief Start the SoftAP and the WebSocket server; their events are posted to the core.
 *
 * @param[in] id Identity (SSID and passphrase).
 * @retval CGW_OK   Started.
 * @retval CGW_E_IO Start failed.
 */
    cgw_rc_t cgw_wiring_start_network(const cgw_core_identity_t *id);

#ifdef __cplusplus
}
#endif

#endif /* CGW_WIRING_H */
