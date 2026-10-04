# LockSys CGW Software Requirements

| Field | Value |
|---|---|
| Document ID | LS-CGW-SRS-001 |
| Version | 0.1 |
| Status | Draft |
| Owner | jlurg |
| Parents | LS-SRS-001 ([system requirements](../../02_system/system_requirements.md)), LS-SAF-001 safety mechanisms, LS-SEC-001 cybersecurity requirements |
| Architecture | LS-CGW-SAD-001 ([CGW software architecture](architecture.md)) |

## 1. Purpose and scope

This document specifies the software requirements of the CGW firmware (`SWR-CGW-nnn`), derived from the system requirements allocated to the CGW (allocation C in LS-SRS-001), the safety mechanisms allocated to the CGW (SM-02 to SM-05, SM-16) and the cybersecurity requirements CSR-nnn of LS-SEC-001. All requirements are stage A and implemented in milestone M3 unless stated otherwise.

Parameters in backticks are keys of `interfaces/params/timing.yaml` (LS-SAIC-001 §5.0). Frame, signal, message, close-code, DTC and notice names refer to [LS-IF-001](../../03_interfaces/can_matrix.md), [LS-IF-002](../../03_interfaces/app_protocol.md) and [LS-IF-004](../../03_interfaces/dtc_catalog.md).

## 2. Conventions

| Column | Meaning |
|---|---|
| ID | `SWR-CGW-nnn`; numbers are never reused |
| Requirement | "The CGW shall …"; normative |
| Parents | SYS requirements; SM and CSR entries where the requirement implements them |
| Ver | UT host unit test; IT integration test against the APP simulator; TT on-target test; HIL hardware-in-the-loop test; M manual bench procedure; R review or inspection; CI automated check in a CI job |
| Tag | [SAF] safety-related; [SEC] security-related |
| MS | Milestone of implementation and verification |

Implementations carry `/* @satisfies SWR-CGW-nnn */`; host tests carry `/* @verifies SWR-CGW-nnn */`.

## 3. Requirements

### 3.1 SoftAP and transport

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-CGW-001 | The CGW shall run a SoftAP `LockSys-XXXX` with WPA3-SAE only, PMF required, `sae_pwe_h2e` = BOTH and CCMP in RC and RELEASE builds, at most `n_wifi_clients_max` stations, 192.168.4.1/24 with a DHCP server that offers no router and no DNS option, and IPv6 disabled. | SYS-053; CSR-001, CSR-003 | M, TT | [SEC] | M3 |
| SWR-CGW-002 | The CGW shall generate the 20-character base32 passphrase from the hardware RNG at first boot and at factory reset, store it in NVS, keep the Wi-Fi driver configuration in RAM only, and never log it. | SYS-053, SYS-054; CSR-002 | UT, R | [SEC] | M3 |
| SWR-CGW-003 | The CGW shall serve only `/ws/v1` with subprotocol `locksys.v1` matched exactly, answer a plain HTTP request to it with 400, accept binary frames only (text, continuation or fragmented → 1003; larger than `ws_frame_max_bytes` → 1009), never negotiate permessage-deflate, allow at most `n_ws_sockets_max` sockets and at most one unauthenticated connection, and keep LRU purge disabled. | SYS-055; CSR-003, CSR-008 | IT, HIL | [SEC] | M3 |
| SWR-CGW-004 | The CGW shall start the SoftAP ≤ `t_ap_start_to_ms` after reset, retry up to 3 times, and set B1B10 and DEGRADED if it does not start. | SYS-072 | HIL | — | M3 |
| SWR-CGW-043 | The CGW shall send WebSocket frames only through the `ws_tx` ring drained by at most one outstanding httpd work item, retry a failed queue attempt at the next tick, and never drop a queued frame silently. | SYS-025 | UT, IT | — | M3 |

### 3.2 Session and security

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-CGW-005 | The CGW shall perform the handshake of LS-SAIC-001 §8.3 with constant-time proof verification and K_sess derived with HKDF-SHA256, and shall require the WebSocket upgrade and a valid ClientAuth within `t_cgw_handshake_to_ms` of TCP accept, otherwise close with 4006 (upgraded) or close the TCP connection. | SYS-050; CSR-004, CSR-005, CSR-008 | UT, HIL | [SEC] | M3 |
| SWR-CGW-006 | The CGW shall verify the 16-byte tag over the raw body before decoding it, require strictly increasing counters per direction starting at 1, and on a bad tag, a non-increasing counter, counter 0 in a session, a malformed protobuf or a wrong-direction message close with 1008 and latch STOP. | SYS-051, SYS-055; CSR-006, CSR-009 | UT, HIL | [SEC] | M3 |
| SWR-CGW-007 | The CGW shall answer a failed authentication with AuthResult (counter 0, empty tag) and close with the code of LS-SAIC-001 §8.3 (REJECTED_AUTH → 4002, REJECTED_BUSY → 4003, REJECTED_VERSION → 4001, REJECTED_RATE_LIMIT → 4006), with no actuation. | SYS-050 | UT, HIL | [SEC] | M3 |
| SWR-CGW-008 | After `n_auth_fail_throttle` failed authentications the CGW shall answer every attempt within `t_auth_throttle_ms` of the previous failure with REJECTED_RATE_LIMIT and close 4006, decay the counter after `t_auth_fail_decay_ms` without failures, and count security events. | SYS-056; CSR-007 | UT, HIL | [SEC] | M3 |
| SWR-CGW-009 | The CGW shall allow one controller session: a valid authentication with the controller's `client_id` pre-empts the old session (SessionClose, 4004, STOP latch); a different `client_id` gets REJECTED_BUSY and 4003 while the controller has sent a valid frame within `t_session_to_ms`, otherwise the controller is closed with 4004 and the new client accepted. | SYS-052; CSR-008 | UT, HIL | [SEC] | M3 |
| SWR-CGW-010 | The CGW shall close a session after `t_session_to_ms` without a valid frame with SessionClose and 4004, and latch STOP for its press. | SYS-021; SM-02 | UT, HIL | [SAF] | M3 |
| SWR-CGW-011 | The CGW shall close a session that sends more than `n_rate_limit_frames` frames in any rolling `t_rate_window_ms` (all frame types counted) with 1008 and latch STOP, and accept a new session ≤ 5 s later. | SYS-055; CSR-008 | UT, HIL | [SEC] | M3 |
| SWR-CGW-012 | The CGW shall keep K_sess only as a volatile PSA key, destroy it at session close, zeroise the pending K_pair and the QR buffers after use, and log keys only as 4-byte fingerprints. | SYS-054; CSR-005, CSR-012 | R, UT | [SEC] | M3 |
| SWR-CGW-013 | The CGW shall run the HMAC-SHA256 and HKDF known-answer tests of `interfaces/vectors/crypto_kat.json` before starting its tasks and enter SAFE with B1B16 on a failure. | SYS-050; CSR-005 | UT, TT | [SEC] | M3 |
| SWR-CGW-014 | The CGW shall use hardware random numbers for nonces, K_pair and `session_id` only after `WIFI_EVENT_AP_START`, and for the first-boot passphrase only inside the bootloader random-enable bracket. | SYS-050, SYS-054; CSR-002, CSR-010 | R, UT | [SEC] | M3 |
| SWR-CGW-015 | The CGW shall scope `press_id` monotonicity and the door request cache to the session and clear both when a new session authenticates. | SYS-051, SYS-035 | UT | [SEC] | M3 |

### 3.3 Pairing and factory reset

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-CGW-016 | A BOOT hold of `t_pair_btn_hold_ms`, ignored for `t_boot_btn_ignore_ms` after boot and accepted only while no press is active, shall open a pairing window of `t_pairing_window_ms` with a pending 32-byte K_pair from the hardware RNG. | SYS-054; CSR-010 | UT, HIL | [SEC] | M3 |
| SWR-CGW-017 | The CGW shall print the pairing payload of LS-SAIC-001 §8.5 as a QR code encoded with the vendored qrcodegen with static buffers, by raw stdout writes between `<<LS-SECRET-BEGIN>>` and `<<LS-SECRET-END>>`, and never pass it through a logger. | SYS-054; CSR-012 | R, HIL | [SEC] | M3 |
| SWR-CGW-018 | The CGW shall commit the pending K_pair, its generation and the `client_id` to NVS only on the first valid tagged APP frame of a session authenticated with the pending key, then close sessions using the old key with 4005 and close the window; on expiry it shall zeroise the pending key and keep the previous pairing. | SYS-054; CSR-010 | UT, HIL | [SEC] | M3 |
| SWR-CGW-019 | A BOOT hold of `t_factory_reset_hold_ms` shall latch STOP, erase K_pair and the pairing record, generate a new passphrase, restart the SoftAP and require re-pairing. | SYS-057; CSR-011 | HIL | [SEC] | M3 |

### 3.4 Window arbitration

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-CGW-020 | The CGW shall admit a new window press only if every check of LS-SAIC-001 §8.7 holds, answer exactly one CommandAck(WINDOW, press_id) with ACCEPTED or the code of the first failing check, and never move after a rejection. | SYS-005, SYS-035; SM-02 | UT, HIL | [SAF] | M3 |
| SWR-CGW-021 | The CGW shall refresh the keep-alive reference time with the receipt time captured at WebSocket handler entry of each authenticated WindowMove with the current `press_id` and direction. | SYS-021; SM-02 | UT, HIL | [SAF] | M3 |
| SWR-CGW-022 | The CGW shall latch STOP for the active press on keep-alive age > `t_cgw_ka_to_ms`, RTT > `t_rtt_max_ms` or no Pong within `t_pong_to_ms`, direction change, session loss or pre-emption, controller station disconnect, bus-off, DCU lost, CGW or DCU mode change, a DCU rejection or stop reported in DCU_WinSts, and `t_cgw_max_run_backstop_ms`, and send one further CommandAck with the mapped result and the Notice of LS-SAIC-001 §8.9 for every non-release latch. | SYS-021, SYS-035; SM-02, SM-05 | UT, HIL | [SAF] | M3 |
| SWR-CGW-023 | The CGW shall send a Ping right after AuthResult(OK), every `t_ping_motion_ms` during a press and every `t_ping_idle_ms` otherwise, and measure RTT from its own Pings. | SYS-020; SM-02 | UT, HIL | [SAF] | M3 |
| SWR-CGW-024 | On a WindowStop of the active press the CGW shall put a STOP CGW_WinCmd on the bus ≤ 10 ms after the frame is received. | SYS-020 | HIL | [SAF] | M3 |
| SWR-CGW-025 | The CGW shall allocate the CAN PressId 1…255, wrapping, skipping 0 and the current `WinSts_PressIdEcho`, seeded after reset with `WinSts_PressIdEcho + 1` or a random value if no DCU_WinSts has been received. | SYS-035; SM-05 | UT | [SAF] | M3 |
| SWR-CGW-026 | A newer `press_id` while a press is active shall release the old press; WindowMove frames of a latched press shall be ignored, and only a newer `press_id` shall move again. | SYS-035; SM-05 | UT, HIL | [SAF] | M3 |
| SWR-CGW-027 | An immediate STOP latch and session close shall follow `WIFI_EVENT_AP_STADISCONNECTED` for the controller's station. | SYS-021; SM-02 | HIL | [SAF] | M3 |

### 3.5 Door transactions

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-CGW-030 | The CGW shall keep one door transaction in flight system-wide and answer a DoorCommand received while a transaction is PENDING, also from another session, with REJECTED_BUSY. | SYS-027 | UT, HIL | — | M3 |
| SWR-CGW-031 | The CGW shall check a DoorCommand per LS-SAIC-001 §8.7, acknowledge a valid one with ACCEPTED at once, allocate the next CAN ReqId (1…255, skipping 0; seeded from `DoorSts_LastReqId + 1` or a random value) and send CGW_DoorCmd three times at 0, 20 and 40 ms; until the first DCU_DoorSts after reset door commands get FAILED_COMM. | SYS-024, SYS-027 | UT, HIL | — | M3 |
| SWR-CGW-032 | The CGW shall complete the transaction when `DoorSts_LastReqId` equals the ReqId and `DoorSts_LastResult` is neither UNSPECIFIED nor ACCEPTED, send DoorCommandResult, report FAILED_TIMEOUT after `t_cgw_door_result_to_ms`, and never retry automatically. | SYS-024 | UT, HIL | — | M3 |
| SWR-CGW-033 | A repeated `request_id` of the current session shall be re-acknowledged without a new CAN request while in flight and answered with the cached result within `t_cgw_door_cache_ms` after completion. | SYS-027 | UT | — | M3 |

### 3.6 CAN communication

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-CGW-040 | The CGW shall run TWAI with the advanced timing {brp 10, prop_seg 0, tseg_1 13, tseg_2 2, sjw 2, ssp_offset 0} applied before enabling the node, `fail_retry_cnt` = −1, and the acceptance filter 0x200–0x23F and {0x510, 0x590} plus a software whitelist. | SYS-026 | TT, HIL | — | M3 |
| SWR-CGW-041 | The CGW shall send CGW_WinCmd every 20 ms and on a `Req` change with a minimum gap of 5 ms, computing `Req` and `HoldAge` in `can_io` at every transmission from the intent and the keep-alive age, independently of `core`; the first frame after reset shall be STOP with PressId 0 and HoldAge 255. | SYS-021, SYS-022, SYS-026; SM-03, SM-16 | UT, HIL | [SAF] | M3 |
| SWR-CGW-042 | The CGW shall protect every application frame it sends with `ls_e2e` and increment the alive counter only on a successful enqueue. | SYS-037; SM-04 | UT | [SAF] | M3 |
| SWR-CGW-044 | The CGW shall check every received DCU frame in arrival order with `ls_e2e`, apply the RX timeouts of the DBC, never present data of a frame that is not OK with the receiver VALID as fresh, and set U1B02-82 or U1B02-83 on `n_e2e_err_invalid` consecutive errors. | SYS-025, SYS-037; SM-04 | UT, HIL | [SAF] | M3 |
| SWR-CGW-045 | Each transmitted message shall use two static slots that always hold a complete E2E-protected frame; `can_io` shall be the only caller of `twai_node_transmit()`. | SYS-037, SYS-063; SM-04 | UT, R | [SAF] | M3 |
| SWR-CGW-046 | On bus-off the CGW shall rewrite every CGW_WinCmd slot still owned by the driver to a complete STOP frame with HoldAge 255 and a new alive counter before starting recovery, latch STOP, set U1B01 and DEGRADED, recover with `n_busoff_fast` attempts every `t_busoff_fast_ms` and then every `t_busoff_slow_ms`, and reclaim slots without a completion callback 50 ms after recovery, so that communication resumes ≤ 1 s after the fault is removed. | SYS-063, SYS-037; SM-04 | UT, HIL | [SAF] | M3 |
| SWR-CGW-047 | The CGW shall send CGW_NodeSts every 100 ms and CGW_Version every 1000 ms, the first CGW_NodeSts ≤ 100 ms after the TWAI node is enabled. | SYS-062, SYS-072 | HIL | — | M3 |
| SWR-CGW-048 | After `n_ver_debounce` consecutive DCU_NodeSts with a CAN matrix major version ≠ 1 the CGW shall set U1B03, enter DEGRADED and reject window and door commands with REJECTED_VERSION; minor differences shall only be logged. | SYS-062 | UT, HIL | — | M3 |
| SWR-CGW-049 | When DCU_NodeSts is missing for 500 ms the CGW shall mark the DCU not alive, latch STOP, reject commands with FAILED_COMM, and confirm U1B00 after `t_comm_dtc_confirm_ms`. | SYS-021, SYS-062 | UT, HIL | [SAF] | M3 |

### 3.7 Status

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-CGW-050 | The CGW shall build StatusUpdate fields 1–23 with the sources and stale or invalid rules of LS-SAIC-001 §8.8. | SYS-001, SYS-007, SYS-010, SYS-025 | UT, HIL | — | M3 |
| SWR-CGW-051 | The CGW shall push DoorLockState, last door result, WindowState and stop-reason changes at the next tick, coalesce other changes with `t_status_push_gap_ms`, send snapshots every `t_status_push_idle_ms` or `t_status_push_motion_ms`, and answer StatusRequest at the next tick, so that a change is pushed ≤ 50 ms after the CAN frame carrying it. | SYS-025, SYS-024 | UT, HIL | — | M3 |
| SWR-CGW-052 | The CGW shall forward the temperature unmodified in cdeg, push it on a change ≥ `temp_push_delta_cdeg` or a status change, and report STALE after `t_temp_stale_ms` without DCU_TempSts. | SYS-010 | UT, HIL | — | M3 |
| SWR-CGW-053 | The CGW shall send a Notice with the 24-bit DTC value on every testFailed change of a CGW DTC, and the non-DTC notices of LS-SAIC-001 §8.9 when their condition occurs. | SYS-061 | UT, HIL | — | M3 |

### 3.8 Modes, diagnostics and supervision

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-CGW-060 | The CGW shall implement the modes INIT, NORMAL, DEGRADED and SAFE of LS-SAIC-001 §6.2; in SAFE CGW_WinCmd shall stay STOP, every command shall be rejected, and the mode shall be left only by reset. | SYS-062; SM-16 | UT | [SAF] | M3 |
| SWR-CGW-061 | The CGW shall keep its DTCs with status bits 0, 2, 3 and 5 in RAM, report the confirmed count in CGW_NodeSts and StatusUpdate, and keep reset counters in `RTC_NOINIT` across non-power-on resets. | SYS-061 | UT, HIL | — | M3 |
| SWR-CGW-062 | The CGW shall map `esp_reset_reason()` to ResetReason and DTCs per LS-SAIC-001 §6.2 (an EN reset reports POWER_ON). | SYS-061 | UT, HIL | — | M3 |
| SWR-CGW-063 | The CGW shall run the task watchdog with `t_cgw_task_wdt_ms` and panic, the interrupt watchdog with `t_cgw_int_wdt_ms` on both CPUs, the brown-out detector, and alive supervision of `core` and `can_io` with `t_cgw_alive_deadline_ms`; `n_wdt_reset_safe` watchdog or panic resets within `t_wdt_reset_window_ms` shall lead to SAFE. | SYS-070; SM-16 | TT, HIL | [SAF] | M3 |
| SWR-CGW-064 | The CGW shall store a core dump in flash on a panic, report it with B1B14 at the next start and keep it until it is collected. | SYS-071 | HIL | — | M3 |
| SWR-CGW-065 | Project components shall not allocate heap memory after initialisation; the CGW shall keep ≥ 30 % free internal heap and ≥ 25 % stack headroom per task during the soak test. | SYS-070, SYS-071 | UT, HIL | — | M3 |
| SWR-CGW-066 | The CGW shall drive the trace outputs of LS-SAIC-001 §3.5 in all builds, with TRACE0 as RMT width-coded event pulses ≤ 0.5 ms after WebSocket handler entry. | SYS-020, SYS-021 | HIL | — | M3 |
| SWR-CGW-067 | Fault injection shall exist only in DEV builds; RC and RELEASE images shall contain no `cgw_fi_` symbol and no console REPL. | CSR-015 | R, CI | [SEC] | M3 |
| SWR-CGW-068 | NVS writes shall happen only while no press is active; an NVS or key-store error shall erase the namespace, set B1B15, leave the CGW unpaired and close sessions with 4005. | SYS-054, SYS-070 | UT, TT | — | M3 |

### 3.9 Software structure

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-CGW-070 | CAN and protocol code shall be generated from `interfaces/can/locksys.dbc` and `interfaces/proto/locksys/app/v1/` and committed; regeneration shall produce no difference. | LS-SAIC-001 §1.4 | R, CI | — | M0 |
| SWR-CGW-071 | Portable core components shall not include ESP-IDF, FreeRTOS, driver or NVS headers and shall build and run on the host. | — | R, CI | — | M3 |
| SWR-CGW-072 | Protocol decoding shall use static allocation only (nanopb without callbacks or malloc, nesting ≤ 4) and validate enumerations and ranges after decoding; the frame and body decoders shall be fuzzed on the host. | SYS-055; CSR-009 | UT, R | [SEC] | M3 |

### 3.10 Later

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-CGW-090 | The CGW shall control the transceiver standby pin (GPIO6) and release standby ≥ 1 ms before enabling the TWAI node. | LS-SAIC-001 §7.5 | HIL | — | LATER |
| SWR-CGW-091 | The CGW shall provide UDS-lite on 0x7B0/0x7B8 with DIDs for security-event and health counters. | — | HIL | — | LATER |
| SWR-CGW-092 | Release hardening: Secure Boot v2, flash encryption, HMAC-based NVS encryption and JTAG disable on a designated board. | CSR-016 | R | [SEC] | LATER |

## 4. Traceability

| SYS | SWR-CGW |
|---|---|
| SYS-001 | SWR-CGW-050 |
| SYS-005 | SWR-CGW-020 |
| SYS-007 | SWR-CGW-050 |
| SYS-010 | SWR-CGW-050, SWR-CGW-052 |
| SYS-020 | SWR-CGW-023, SWR-CGW-024, SWR-CGW-066 |
| SYS-021 | SWR-CGW-010, SWR-CGW-021, SWR-CGW-022, SWR-CGW-027, SWR-CGW-041, SWR-CGW-049, SWR-CGW-066 |
| SYS-022 | SWR-CGW-041 |
| SYS-024 | SWR-CGW-031, SWR-CGW-032, SWR-CGW-051 |
| SYS-025 | SWR-CGW-043, SWR-CGW-044, SWR-CGW-050, SWR-CGW-051 |
| SYS-026 | SWR-CGW-040, SWR-CGW-041 |
| SYS-027 | SWR-CGW-030, SWR-CGW-031, SWR-CGW-033 |
| SYS-035 | SWR-CGW-015, SWR-CGW-020, SWR-CGW-022, SWR-CGW-025, SWR-CGW-026 |
| SYS-037 | SWR-CGW-042, SWR-CGW-044, SWR-CGW-045, SWR-CGW-046 |
| SYS-050 | SWR-CGW-005, SWR-CGW-007, SWR-CGW-013, SWR-CGW-014 |
| SYS-051 | SWR-CGW-006, SWR-CGW-015 |
| SYS-052 | SWR-CGW-009 |
| SYS-053 | SWR-CGW-001, SWR-CGW-002 |
| SYS-054 | SWR-CGW-002, SWR-CGW-012, SWR-CGW-014, SWR-CGW-016, SWR-CGW-017, SWR-CGW-018, SWR-CGW-068 |
| SYS-055 | SWR-CGW-003, SWR-CGW-006, SWR-CGW-011, SWR-CGW-072 |
| SYS-056 | SWR-CGW-008 |
| SYS-057 | SWR-CGW-019 |
| SYS-061 | SWR-CGW-053, SWR-CGW-061, SWR-CGW-062 |
| SYS-062 | SWR-CGW-047, SWR-CGW-048, SWR-CGW-049, SWR-CGW-060 |
| SYS-063 | SWR-CGW-045, SWR-CGW-046 |
| SYS-070 | SWR-CGW-063, SWR-CGW-065, SWR-CGW-068 |
| SYS-071 | SWR-CGW-064, SWR-CGW-065 |
| SYS-072 | SWR-CGW-004, SWR-CGW-047 |

| SM | SWR-CGW |
|---|---|
| SM-02 | SWR-CGW-010, SWR-CGW-020, SWR-CGW-021, SWR-CGW-022, SWR-CGW-023, SWR-CGW-027 |
| SM-03 | SWR-CGW-041 |
| SM-04 | SWR-CGW-042, SWR-CGW-044, SWR-CGW-045, SWR-CGW-046 |
| SM-05 | SWR-CGW-022, SWR-CGW-025, SWR-CGW-026 |
| SM-16 | SWR-CGW-041, SWR-CGW-060, SWR-CGW-063 |

## 5. Rationale

- **Numbering by area with gaps.** Requirements are grouped so that additions stay inside their area; numbers are never reused.
- **Parameters by key.** A change in `timing.yaml` does not change a requirement; verification limits come from the same file.
- **Independent STOP path as its own requirement.** SWR-CGW-041 holds even if `core` hangs, which is the CGW contribution to SYS-021 under SM-16.

## 6. References

- [LS-SRS-001 System requirements](../../02_system/system_requirements.md), [LS-SAIC-001](../../02_system/LS-SAIC.md)
- [CGW software architecture](architecture.md) (LS-CGW-SAD-001)
- [Safety concept](../../05_safety/safety_concept.md) (LS-SAF-001), [security concept](../../06_security/security_concept.md) (LS-SEC-001)
- [Verification strategy](../../07_verification/verification_strategy.md) (LS-VER-001), [HIL test catalogue](../../07_verification/hil_test_catalog.md)
- [LS-IF-001 CAN matrix](../../03_interfaces/can_matrix.md), [LS-IF-002 APP protocol](../../03_interfaces/app_protocol.md), [LS-IF-004 DTC catalogue](../../03_interfaces/dtc_catalog.md)
