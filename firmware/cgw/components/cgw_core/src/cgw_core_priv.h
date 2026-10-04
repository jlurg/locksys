/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_core_priv.h
 * @brief Functions shared by the core sources (APP side and vehicle side). Not part of the API.
 */

#ifndef CGW_CORE_PRIV_H
#define CGW_CORE_PRIV_H

#include "cgw_core/cgw_core.h"

/**
 * @brief Fill the system view for the arbiter.
 *
 * @param[in]  core   Core state.
 * @param[in]  now_ms Current time (RTT gate).
 * @param[out] env    System view.
 */
void cgw_core_env(const cgw_core_t *core, uint32_t now_ms, cgw_arb_env_t *env);

/**
 * @brief Execute the actions requested by the arbiter.
 *
 * @param[in,out] core Core state.
 * @param[in]     out  Arbiter actions.
 */
void cgw_core_apply(cgw_core_t *core, const cgw_arb_out_t *out);

/**
 * @brief Latch STOP for the active press and execute the resulting actions.
 *
 * @param[in,out] core   Core state.
 * @param[in]     reason Latch reason.
 */
void cgw_core_latch(cgw_core_t *core, cgw_latch_reason_t reason);

/**
 * @brief Send a Notice to the controller session, if any.
 *
 * @param[in,out] core     Core state.
 * @param[in]     code     Notice code or 24-bit DTC value.
 * @param[in]     severity Ls_FaultSeverityType.
 */
void cgw_core_notice(cgw_core_t *core, uint32_t code, uint8_t severity);

/**
 * @brief Report a DTC test result; a testFailed change sends a Notice with the DTC value.
 *
 * @param[in,out] core   Core state.
 * @param[in]     dtc    DTC.
 * @param[in]     failed Test failed.
 * @param[in]     now_ms Current time.
 */
void cgw_core_report(cgw_core_t *core, cgw_dtc_t dtc, bool failed, uint32_t now_ms);

/**
 * @brief Handle a WebSocket lifecycle event (TCP_OPEN, WS_OPEN, WS_CLOSE).
 *
 * @param[in,out] core Core state.
 * @param[in]     ev   Event.
 */
void cgw_core_app_link_event(cgw_core_t *core, const cgw_core_event_t *ev);

/**
 * @brief Handle a received WebSocket message.
 *
 * @param[in,out] core Core state.
 * @param[in]     ev   WS_FRAME event.
 */
void cgw_core_app_frame(cgw_core_t *core, const cgw_core_event_t *ev);

/**
 * @brief Close a connection; latches STOP when it was the controller.
 *
 * @param[in,out] core            Core state.
 * @param[in]     conn            Connection.
 * @param[in]     code            Close code.
 * @param[in]     controller_lost The controller session ends.
 */
void cgw_core_app_close(cgw_core_t *core, cgw_conn_t conn, uint16_t code, bool controller_lost);

/**
 * @brief APP-side part of the time event: session timeouts, Ping, status push.
 *
 * @param[in,out] core   Core state.
 * @param[in]     now_ms Current time.
 */
void cgw_core_app_tick(cgw_core_t *core, uint32_t now_ms);

/**
 * @brief Commit the pending pairing after key confirmation by @p conn.
 *
 * @param[in,out] core Core state.
 * @param[in]     conn Connection authenticated with the pending key.
 * @param[in]     now_ms Current time.
 */
void cgw_core_app_commit_pairing(cgw_core_t *core, cgw_conn_t conn, uint32_t now_ms);

#endif /* CGW_CORE_PRIV_H */
