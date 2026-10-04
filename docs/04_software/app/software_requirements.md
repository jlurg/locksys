# LockSys APP Software Requirements

| Field | Value |
|---|---|
| Document ID | LS-APP-SRS-001 |
| Version | 0.1 |
| Status | Draft |
| Owner | jlurg |
| Parents | LS-SRS-001 ([system requirements](../../02_system/system_requirements.md)), LS-STK-001 ([stakeholder requirements](../../01_stakeholder/stakeholder_requirements.md)), LS-SAF-001 safety mechanisms, LS-SEC-001 cybersecurity requirements |
| Architecture | LS-APP-SAD-001 ([APP software architecture](architecture.md)) |

## 1. Purpose and scope

This document specifies the software requirements of the mobile application (`SWR-APP-nnn`), derived from the system requirements allocated to the APP (allocation A in LS-SRS-001), from the stakeholder requirements on the user interface and platforms, from safety mechanism SM-01 and from the cybersecurity requirements of LS-SEC-001. All requirements are stage A and implemented in milestone M4 unless stated otherwise.

Parameters in backticks are keys of `interfaces/params/timing.yaml`; message, field, close-code and enumeration names refer to [LS-IF-002](../../03_interfaces/app_protocol.md).

## 2. Conventions

| Column | Meaning |
|---|---|
| ID | `SWR-APP-nnn`; numbers are never reused |
| Requirement | "The APP shall …"; normative |
| Parents | SYS or STK requirements; SM and CSR entries where implemented |
| Ver | UT unit test; UT(w) widget test; IT integration test against `cgw_sim`; M manual procedure (TST-MAN-APP-nnn); R review or inspection |
| Tag | [SAF] safety-related; [SEC] security-related |
| MS | Milestone |

Implementations carry `// @satisfies SWR-APP-nnn`; tests carry `// @verifies SWR-APP-nnn` on the line above the test.

## 3. Requirements

### 3.1 Pairing and secrets

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-APP-001 | The APP shall accept pairing data only from its in-app QR scanner and register no URL scheme or deep link. | SYS-054; CSR-010 | R, UT | [SEC] | M4 |
| SWR-APP-002 | The APP shall validate the pairing payload of LS-SAIC-001 §8.5 (`v`, `id`, `s`, `p`, `k`, `b`, `sec`; `h` only in simulator builds; duplicates rejected; ≤ 256 characters) and reject it without side effects on any violation. | SYS-054; CSR-010 | UT | [SEC] | M4 |
| SWR-APP-003 | The APP shall persist a new pairing only after AuthResult OK with a verified `server_proof` and tag, with `device_id` equal to the payload `id` and the pairing window open, and otherwise keep the previous pairing. | SYS-054; CSR-004, CSR-010 | UT, IT | [SEC] | M4 |
| SWR-APP-004 | The APP shall store the pairing record only in OS secure storage (iOS Keychain `unlocked_this_device`, Android Keystore-wrapped), excluded from cloud backup and device transfer, wipe it on a decryption failure and on the first run after installation. | CSR-014 | R, M | [SEC] | M4 |
| SWR-APP-005 | The APP shall never log, display, export or transmit K_pair, K_sess, the passphrase, proofs or tags beyond protocol use, and shall protect pairing screens (Android `FLAG_SECURE`, iOS privacy cover while not resumed); the only exception is the iOS manual-join screen, which reveals the passphrase only while "Show" is held and never through the clipboard. | SYS-054; CSR-012, CSR-014 | UT, R | [SEC] | M4 |
| SWR-APP-006 | The APP shall let the user unpair after confirmation, erasing the secure record and closing the session. | CSR-014 | UT(w) | [SEC] | M4 |

### 3.2 Network join and transport

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-APP-010 | On Android the APP shall join the SoftAP with a `WifiNetworkSpecifier` (exact SSID and BSSID; `wpa3` → WPA3 passphrase; `wpa2wpa3` → WPA3 if supported, else WPA2), bind the process to that network, and unbind on release or loss. | SYS-053; STK-005 | IT, M | — | M4 |
| SWR-APP-011 | On iOS the APP shall provide the manual join in Settings (SSID shown, passphrase revealed only while held) and, if TST-MAN-APP-003 passes, the programmatic join with `NEHotspotConfiguration` (`joinOnce`, `alreadyAssociated` treated as success); in both cases it shall trigger and handle the Local Network prompt with a probe and report Blocked when access is denied. | SYS-053; STK-005 | M | — | M4 |
| SWR-APP-012 | The APP shall connect to `ws://192.168.4.1:80/ws/v1` with subprotocol `locksys.v1` (abort if the server does not select it), a `t_app_ws_connect_to_ms` timeout, permessage-deflate off, binary frames only, ≤ `ws_frame_max_bytes` per outbound frame, and close with 1002 on a larger inbound frame. | SYS-050, SYS-055 | UT, IT | — | M4 |
| SWR-APP-013 | The APP shall run on Android ≥ 10 and iOS ≥ 15 and explain missing WPA3-SAE support before joining. | SYS-053; STK-005 | M | — | M4 |

### 3.3 Session

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-APP-020 | The APP shall perform the handshake of LS-SAIC-001 §8.3: pin `device_id`, verify `server_proof` in constant time before trusting server data, and derive K_sess with HKDF-SHA256. | SYS-050; CSR-004, CSR-005 | UT, IT | [SEC] | M4 |
| SWR-APP-021 | The APP shall tag every session frame, verify each inbound tag over the received body bytes, enforce strictly increasing counters per direction starting at 1, and on a violation close with 1008, stop any press and reconnect. | SYS-051; CSR-006 | UT, IT | [SEC] | M4 |
| SWR-APP-022 | The APP shall keep K_sess in RAM only, zeroise it and the K_pair copy at session end, end the session when the application is paused, and use `Random.secure()` for nonces and `client_id`. | CSR-005 | R, UT | [SEC] | M4 |
| SWR-APP-023 | The APP shall treat a protocol major ≠ 1, a subprotocol mismatch or close 4001 as Blocked ("update required") and show the APP, protocol, CGW firmware and CAN matrix versions. | SYS-062 | UT | — | M4 |
| SWR-APP-024 | The APP shall implement the connection state machine of LS-APP-SAD-001 §7.1 with backoff min(`t_app_reconnect_max_ms`, `t_app_reconnect_min_ms` × 2^(n−1)) × U(0.8, 1.2), ≥ `t_app_busy_backoff_ms` after 4003, ≥ `t_auth_throttle_ms` after 4006, ≥ 2 s after 1008, no retry after 4001, 4002 or 4005, and no attempt unless resumed; lifecycle `inactive` and `hidden` shall not abort connecting, authenticating or a session. | SYS-052, SYS-056 | UT | — | M4 |
| SWR-APP-025 | The APP shall send Ping every `t_ping_idle_ms` when idle, answer a CGW Ping synchronously, and declare the link lost after `t_session_to_ms` without a valid inbound frame. | SYS-021, SYS-025 | UT, IT | — | M4 |
| SWR-APP-026 | The APP shall keep outbound traffic ≤ 20 frames per second on average and never above `n_rate_limit_frames` in any rolling `t_rate_window_ms` (all frames counted; WindowStop and Pong never throttled). | SYS-055 | UT | — | M4 |
| SWR-APP-027 | The APP shall send commands only while authenticated, never queue or replay them across disconnects or sessions, and restart `press_id` and `request_id` at 1 per session. | SYS-035, SYS-051 | UT | [SAF] | M4 |

### 3.4 Window hold-to-run

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-APP-030 | The APP shall send `WindowMove(press_id, direction, hold_ms = 0)` ≤ `t_app_send_max_ms` after a pointer down on an enabled window zone, then every `t_app_ka_ms` with the same `press_id` and `hold_ms` = time since press start while the pointer is held. | SYS-005; SM-01 | UT, IT | [SAF] | M4 |
| SWR-APP-031 | The APP shall send `WindowStop` and cancel the keep-alive ≤ `t_app_send_max_ms` after any stop trigger T1–T12 of LS-APP-SAD-001 §5.3. | SYS-020, SYS-096; SM-01 | UT, UT(w) | [SAF] | M4 |
| SWR-APP-032 | After any stop the APP shall start no motion until all window pointers are up, `t_app_press_gap_min_ms` has elapsed and a new press begins; re-entering a zone shall never resume a press. | SYS-035; SM-01, SM-05 | UT(w) | [SAF] | M4 |
| SWR-APP-033 | The APP shall start window motion only from physical pointer contact, never from semantic, keyboard or programmatic actions. | SYS-005 | UT(w) | [SAF] | M4 |
| SWR-APP-034 | The window control shall be one switch with two hold zones (CLOSE, OPEN) in a fixed, non-scrolling panel outside system gesture insets and view padding, in portrait and landscape, and no route or dialog shall be pushed while a press is active. | SYS-005, SYS-096 | UT(w) | [SAF] | M4 |
| SWR-APP-035 | The APP shall show the window state, the position (0–100 %, 255 as unknown) and the stop reason from the status, and show motion only from MOVING_UP or MOVING_DOWN. | SYS-007 | UT(w) | — | M4 |
| SWR-APP-036 | The APP shall require acknowledgement of the remote-closing safety notice before the first window use. | STK-003; SG-01 | R, UT(w) | [SAF] | M4 |

### 3.5 Door lock

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-APP-040 | The APP shall provide a lock-state button that shows DoorLockState only from fresh status or results (icon and text for all six states, plus age and staleness), never inferred from a command; a tap shall request a status refresh at most once per second and never actuate. | SYS-001, SYS-002 | UT(w) | — | M4 |
| SWR-APP-041 | The APP shall provide one lock/unlock button that requests UNLOCK after a hold-to-confirm of `t_app_unlock_confirm_ms` when the fresh state is LOCKED (a confirmation dialog for screen readers) and LOCK on a tap otherwise, and is disabled while a command is pending, the state is transitional or stale, `door_inhibited` is set or controls are unavailable. | SYS-002 | UT(w) | — | M4 |
| SWR-APP-042 | The APP shall allow one door command in flight with a unique `request_id`, show the pending state ≤ 100 ms after commit, accept a matching StatusUpdate as completion only after the CommandAck of that request, after `t_app_door_result_to_ms` without a result send StatusRequest and show the current state, and never retry automatically. | SYS-024, SYS-027 | UT, IT | — | M4 |
| SWR-APP-043 | The APP shall map every CommandResult to a user message per LS-APP-SAD-001 §6 for door and window commands. | SYS-095 | UT(w) | — | M4 |

### 3.6 Status, faults and availability

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-APP-050 | The APP shall disable its controls and state the reason when the session is not authenticated, `dcu_alive` is false, the effective status age exceeds `t_app_status_stale_ms`, `dcu_mode` is not NORMAL or DEGRADED, `cgw_mode` is not NORMAL, or the function's inhibit flag is set. | SYS-095, SYS-025 | UT, UT(w) | — | M4 |
| SWR-APP-051 | The APP shall compute the effective status age as (monotonic now − receive time) + `status_age_ms` and mark status older than `t_app_status_stale_ms` as stale. | SYS-025; SG-04 | UT | — | M4 |
| SWR-APP-052 | The APP shall show the temperature with 0.1 °C resolution when VALID, the TempStatus name otherwise, and grey it when stale. | SYS-010 | UT(w) | — | M4 |
| SWR-APP-053 | The APP shall show a fault indicator (icon and text) when a mode is DEGRADED or SAFE, a DTC count is above 0, or a Notice of severity WARNING or higher arrives. | SYS-061 | UT(w) | — | M4 |
| SWR-APP-054 | The APP shall provide the Diagnostics screen of LS-APP-SAD-001 §10 (SCR-07). | SYS-062 | UT(w) | — | M4 |

### 3.7 Usability and accessibility

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-APP-060 | The APP shall give distinct, user-disableable haptic feedback for press accepted, release, and forced stop or rejection. | STK-003 | M | — | M4 |
| SWR-APP-061 | The APP shall meet 48 dp / 44 pt tap targets (hold zones ≥ 96 × 96 dp), label every control, meet WCAG AA contrast, never convey state by colour alone, and keep full function at 200 % text scale. | STK-005 | UT(w), M | — | M4 |

### 3.8 Software structure

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-APP-070 | Protocol classes, parameters and DTC and notice constants shall be generated from `interfaces/` into `packages/locksys_protocol/lib/src/gen/` and committed; regeneration shall produce no difference. | LS-SAIC-001 §1.4 | R | — | M0 |
| SWR-APP-071 | The import rules of LS-APP-SAD-001 §3.2 shall hold and be checked by an architecture test. | — | UT | — | M4 |
| SWR-APP-072 | All [SAF] window logic shall reside in `HoldToRunController`, and the stop path from a trigger to `WindowStop` on the socket shall contain no asynchronous gap. | SYS-096; SM-01 | UT, R | [SAF] | M4 |

### 3.9 Later

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-APP-090 | The APP shall request `ACCESS_LOCAL_NETWORK` at run time once it targets SDK ≥ 37. | STK-006 | M | — | LATER |
| SWR-APP-091 | The APP shall offer an optional biometric or device-credential gate before unlock. | CSR-014 | M | [SEC] | LATER |

## 4. Traceability

| SYS / STK | SWR-APP |
|---|---|
| SYS-001 | SWR-APP-040 |
| SYS-002 | SWR-APP-040, SWR-APP-041 |
| SYS-005 | SWR-APP-030, SWR-APP-033, SWR-APP-034 |
| SYS-007 | SWR-APP-035 |
| SYS-010 | SWR-APP-052 |
| SYS-020 | SWR-APP-031 |
| SYS-021 | SWR-APP-025 |
| SYS-024 | SWR-APP-042 |
| SYS-025 | SWR-APP-025, SWR-APP-050, SWR-APP-051 |
| SYS-027 | SWR-APP-042 |
| SYS-035 | SWR-APP-027, SWR-APP-032 |
| SYS-050 | SWR-APP-012, SWR-APP-020 |
| SYS-051 | SWR-APP-021, SWR-APP-027 |
| SYS-052 | SWR-APP-024 |
| SYS-053 | SWR-APP-010, SWR-APP-011, SWR-APP-013 |
| SYS-054 | SWR-APP-001, SWR-APP-002, SWR-APP-003, SWR-APP-005 |
| SYS-055 | SWR-APP-012, SWR-APP-026 |
| SYS-056 | SWR-APP-024 |
| SYS-061 | SWR-APP-053 |
| SYS-062 | SWR-APP-023, SWR-APP-054 |
| SYS-071 | Soak run with the APP simulator (no APP-specific requirement) |
| SYS-095 | SWR-APP-043, SWR-APP-050 |
| SYS-096 | SWR-APP-031, SWR-APP-034, SWR-APP-072 |
| STK-003 | SWR-APP-036, SWR-APP-060 |
| STK-005 | SWR-APP-010, SWR-APP-011, SWR-APP-013, SWR-APP-061 |
| SM-01 | SWR-APP-030, SWR-APP-031, SWR-APP-032, SWR-APP-072 |

## 5. Rationale

- **Timing requirements by parameter key.** The APP allocations of the end-to-end chains (`t_app_send_max_ms`, `t_app_render_max_ms`) are subtracted from the system limits when the HIL uses the APP simulator; the APP verifies them in its own tests (LS-SRS-001 §2).
- **No safety claim on the APP.** The [SAF] tags mark the APP contribution to SM-01; SG-01 holds without it because the CGW and the DCU supervise the keep-alive independently.

## 6. References

- [LS-SRS-001](../../02_system/system_requirements.md), [LS-STK-001](../../01_stakeholder/stakeholder_requirements.md), [LS-SAIC-001](../../02_system/LS-SAIC.md) §8
- [APP software architecture](architecture.md) (LS-APP-SAD-001), [LS-IF-002 APP protocol](../../03_interfaces/app_protocol.md)
- [Safety concept](../../05_safety/safety_concept.md) (LS-SAF-001), [security concept](../../06_security/security_concept.md) (LS-SEC-001), [verification strategy](../../07_verification/verification_strategy.md) (LS-VER-001)
