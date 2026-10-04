# LockSys System Requirements Specification

| Field | Value |
|---|---|
| Document ID | LS-SRS-001 |
| Version | 0.2 |
| Status | Draft for baseline |
| Owner | jlurg |
| Date | 2026-10-03 |
| Contract | LS-SAIC-001 v0.2 (`docs/02_system/LS-SAIC.md`) |
| Parents | LS-STK-001 (`docs/01_stakeholder/stakeholder_requirements.md`), safety goals SG-nn, cybersecurity goals CSG-nn |

---

## 1. Purpose and scope

This document specifies the system requirements (SYS-nnn) and the bench hardware requirements (HWR-nnn) of LockSys release v1.0. Every requirement is consistent with LS-SAIC-001 v0.2; parameters in backticks are keys of the parameter registry (LS-SAIC §5.0, `interfaces/params/timing.yaml`). Software requirements (SWR-DCU, SWR-CGW, SWR-APP, SWR-LIB) are derived in the node architecture documents and trace to the IDs below.

## 2. Conventions

| Column | Meaning |
|---|---|
| Alloc | A = APP, C = CGW, D = DCU, H = hardware |
| Ver | HIL = hardware-in-the-loop test; IT = integration test; UT = unit or widget test; R = review or inspection; M = manual bench test |
| Stage | A = release v1.0 (free-spinning encoder motor); B = virtual end positions; D = end-position switches; E = anti-pinch; LATER = beyond v1.0 |
| Tag | [SAF] safety-related (traces to SG-nn); [SEC] security-related (traces to CSG-nn) |
| Parents | Stakeholder requirements (STK-nnn) and safety or cybersecurity goals |

- Requirements are written with "shall". Acceptance criteria are the pass conditions of the verification named in Ver; tests reference the requirement with `@verifies` tags (`TST-<UT|IT|HIL|MAN>-<node>-nnn`).
- When the HIL uses the APP simulator instead of the APP, the APP allocations (send ≤ `t_app_send_max_ms`, render ≤ `t_app_render_max_ms`) are subtracted from end-to-end limits; the APP verifies its own allocations in its tests.
- Verification limits are registry keys `lim_*` (LS-SAIC §5.0.10).

---

## 3. System requirements

### 3.1 Door lock

| ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
|---|---|---|---|---|---|---|---|
| SYS-001 | The APP shall display the DoorLockState reported by the DCU, which the DCU derives from the lock position switch and never from the command. | Each DoorLockState value injected through the HIL (6 states × 10 runs) is displayed identically within 1.5 s; APP widget tests show that no state is inferred from a command. | A, C, D | HIL, UT | A | — | STK-001; SG-04 |
| SYS-002 | The APP shall provide a lock-state control that shows DoorLockState and, when tapped, requests a status refresh without actuating, and one lock/unlock control that requests UNLOCK when the fresh state is LOCKED and LOCK otherwise; UNLOCK shall require a hold-to-confirm of `t_app_unlock_confirm_ms`. | APP integration test against the CGW simulator: correct DoorCommand action for every state in 20/20 trials, UNLOCK only after the 800 ms hold; 0 DoorCommand frames for 20 taps of the lock-state control. HIL: DoorCommand → `DoorCmd_Req` mapping correct in 20/20 trials. | A, C | IT, HIL | A | — | STK-001, STK-002 |
| SYS-003 | On a door request the DCU shall drive the lock actuator for `t_lock_pulse_ms` in the requested direction and verify the result with the position switch. | Pulse 300 ± 5 ms in the requested direction (logic analyser on LOCK_INA/INB/PWM), result OK and DoorLockState from the switch in 20/20 trials. | D | HIL | A | — | STK-002, STK-008 |
| SYS-004 | If the position switch already shows the requested state, the DCU shall report OK without actuating. | No lock-bridge activity in 20 trials; `DoorSts_LastResult` = OK. | D | HIL | A | — | STK-002 |
| SYS-024 | The APP shall show the transitional door state ≤ 300 ms and the final state ≤ 1.0 s after the user commits a door command (≤ 2.0 s with one retry). | 50 trials per case with the APP simulator, from DoorCommand transmission to StatusUpdate receipt: transitional ≤ `lim_door_transition_appsim_ms` (264 ms), final ≤ `lim_door_final_appsim_ms` (964 ms), final with one retry ≤ `lim_door_final_retry_appsim_ms` (1964 ms). | A, C, D | HIL, IT | A | — | STK-001, STK-002 |
| SYS-027 | Only one door transaction shall be in flight: the CGW shall reject a further DoorCommand with REJECTED_BUSY, and repetitions of one CAN ReqId shall cause at most one actuation sequence. | A second DoorCommand while PENDING → `CommandAck(REJECTED_BUSY)` from the CGW; on CAN (restbus) three repetitions of one ReqId plus new ReqIds injected during execution → exactly one actuation sequence, `DoorSts_LastReqId` unchanged, the drop logged in `#LOG`. | C, D | HIL | A | — | STK-002 |
| SYS-034 | The DCU shall limit lock actuation to `n_lock_max_attempts` pulses of at most `t_lock_pulse_hard_max_ms` each per request and to `n_lock_rate_max` actuations per `t_lock_rate_window_ms`, and reject further requests with REJECTED_RATE_LIMIT. | 11 requests within 60 s → the 11th rejected and B1A22 set; a stuck switch → exactly 2 pulses of ≤ 500 ms; a pulse request beyond the cap (fault injection) is cut at 500 ms. | D | HIL | A | [SAF] | STK-002; SG-03 |

### 3.2 Window motion and supervision

| ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
|---|---|---|---|---|---|---|---|
| SYS-005 | The window motor shall run UP or DOWN only while the matching APP control is held, and only in the requested direction. | Hold 2 s → bridge energised 2 s ± 150 ms with the encoder count sign matching the direction, 20/20 trials per direction; 0 bridge activations without a hold in 100 trials. | A, C, D | HIL | A | [SAF] | STK-003, STK-008; SG-01 |
| SYS-006 | The window shall stop when it reaches an end position and shall reject motion into an active end position. Stage B uses virtual end positions from the encoder position; stage D uses end-position switches. | B: stop ≤ `lim_virtual_limit_stop_ms` (10 ms) after the position reaches 0 % or 100 %; a press into the active end position → REJECTED_INTERLOCK. D: bridge off ≤ `lim_physical_limit_stop_ms` (5 ms) after the switch opens; REJECTED_INTERLOCK into an active switch. | D, H | HIL | B, D | [SAF] | STK-004; SG-02 |
| SYS-007 | The DCU shall report the window state and the position (0–100 %, 255 = unknown). | A: every press shows STOPPED → MOVING_UP or MOVING_DOWN → STOPPED in DCU_WinSts and in the APP; `WinSts_PosPct` = 255 in 100 % of frames. B: 0 and 100 at the virtual end positions; intermediate values within ± 2 % of the travel. | D, C, A | HIL | A, B | — | STK-003, STK-004; SG-04 |
| SYS-030 | A window press shall stop after `t_win_max_run_ms` (8.0 s, calibratable) of running, with StopReason MAX_RUNTIME. | Hold 10 s → stop at 8.00 ± 0.05 s after drive-on, StopReason MAX_RUNTIME, no restart without a new press (20 trials). | D | HIL | A | [SAF] | STK-003; SG-01, SG-03 |
| SYS-031 | The DCU shall stop the window ≤ 100 ms after the filtered motor current exceeds `i_oc_backstop_ma` for `t_oc_backstop_ms` outside the start blanking `t_cs_blank_ms`, with StopReason OVERCURRENT and DTC B1A16. | HIL-SIM CS injection (DAC) applied ≥ 100 ms after drive-on: steps to threshold + 15 % → stop ≤ `lim_oc_stop_ms` after the step and B1A16; steps to threshold − 15 % → no stop. | D | HIL | A | [SAF] | STK-003; SG-02, SG-03 |
| SYS-032 | Both end positions active, or an active end position not cleared within `t_lim_leave_ms` when driving away, shall inhibit the window within 50 ms and set B1A12. | Emulated switches: inhibit ≤ 50 ms, B1A12, SAFE. | D | HIL | D | [SAF] | STK-004; SG-02 |
| SYS-033 | After every stop the window bridge shall brake for `t_win_brake_ms` and then stay off for at least `t_win_rev_dead_ms` before any new drive. | Logic analyser on WIN_INA/INB/PWM: INA = INB = 0 and PWM low continuously ≥ 150 ms between the end of the brake and the next drive-on, for reversals and same-direction restarts, 50 trials. | D | HIL | A | [SAF] | STK-003; SG-02 |
| SYS-035 | After any stop other than a release, and after any DCU or CGW reset, the window shall not move again without a new press. | Hold through a CGW reset, a DCU reset, a session pre-emption and each non-release stop cause: no restart in 20 trials each; a new press moves. | C, D | HIL | A | [SAF] | STK-003; SG-01 |
| SYS-036 | No actuator shall be energised for more than 10 µs during reset or INIT. | 100 power cycles, 100 NRST pulses and 20 watchdog resets captured at ≥ 100 MS/s on all bridge inputs: no pulse longer than `lim_reset_glitch_us`. | D, H | HIL | A | [SAF] | STK-008; SG-01 |
| SYS-042 | While the window bridge drives with a duty of at least `no_motion_duty_min_pct`, the DCU shall stop the motor when, after `t_start_grace_ms`, the encoder confirms less than `no_motion_min_pct` % of the expected motion over `no_motion_window_ms` (NO_MOTION), or when the encoder counts move opposite to the command (DIR_MISMATCH). | HIL-SIM: encoder edges stopped while driving → stop ≤ `lim_no_motion_stop_ms` (110 ms), StopReason STALL, WindowState BLOCKED, B1A11; A/B swapped → stop ≤ `t_start_grace_ms` + 110 ms after drive-on, StopReason DIR_MISMATCH, B1A15, window inhibited until UDS 0x14; no false trip in 100 nominal presses including starts and stops. REAL (attended): encoder unplugged during a press → stop ≤ 110 ms. | D | HIL | A | [SAF] | STK-003; SG-01, SG-02 |

### 3.3 Hold-to-run chain and communication safety

| ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
|---|---|---|---|---|---|---|---|
| SYS-020 | Releasing the window control shall stop the motor ≤ 150 ms after the release while the RTT is ≤ 200 ms. | 100 trials with the APP simulator: WindowStop transmission → bridge off ≤ `lim_release_stop_appsim_ms` (130 ms); component check CGW TRACE0 (WindowStop) → off ≤ `lim_release_stop_cgw_ms` (25 ms); degraded link (RTT 180–200 ms, WindowStop delayed 100 ms after the release marker) → release marker → off ≤ 130 ms without an RTT-gate trip. APP tests: pointer-up → WindowStop ≤ 20 ms. | A, C, D | HIL, IT | A | [SAF] | STK-003; SG-01 |
| SYS-021 | When keep-alives stop (lost WindowStop, WiFi loss, APP crash or background), the motor shall be off ≤ 400 ms after the last keep-alive received by the CGW. | 100 trials per cause; reference = CGW TRACE0 pulse of the last accepted WindowMove; max ≤ `lim_ka_loss_stop_ms`; StopReason HOLD_TIMEOUT; additional case: CGW core-task hang (fault injection) with the CAN path alive. | C, D | HIL | A | [SAF] | STK-003, STK-006; SG-01 |
| SYS-022 | The motor shall be off ≤ 130 ms after the last valid CGW_WinCmd when commands stop (CGW reset, CAN open or short). | Restbus stop, CAN open and short relay, CGW EN low: max ≤ `lim_can_loss_stop_ms`; StopReason CAN_TIMEOUT. | D | HIL | A | [SAF] | STK-003, STK-007; SG-01 |
| SYS-023 | A DCU software hang shall switch both bridges off ≤ 100 ms after the hang starts. | HIL build with fault injection: HANG → off ≤ 25 ms (hang monitor); HANG_IRQOFF and SKIP_CHECKPOINT → off ≤ 77 ms with ResetReason WATCHDOG; the stopping mechanism is recorded per trial. | D, H | HIL | A | [SAF] | STK-003; SG-01, SG-02 |
| SYS-037 | Corrupted, replayed, out-of-sequence or stale CGW_WinCmd frames shall never cause motion; `n_e2e_err_invalid` consecutive errors shall stop the window ≤ 70 ms and set a DTC. | Injection matrix (CRC, DataID, DLC, repeated counter, counter jump, frame sent under another ID), 100 frames per error type, idle and while moving → 0 motion starts; 3 consecutive errors → stop ≤ `lim_e2e_stop_ms` and U1A02; bus-off with a press just started → no start from frames sent after recovery. | C, D | HIL | A | [SAF] | STK-007; SG-01 |
| SYS-096 | The APP shall send WindowStop on every stop trigger of LS-SAIC §8.7 (release, pointer cancel, slide-off, second pointer, lifecycle change, link loss, rejection or latch acknowledgement, DCU-initiated stop). | Widget tests per trigger: WindowStop ≤ 20 ms after the trigger and no restart without a new press; integration test against the CGW simulator. | A | UT, IT | A | [SAF] | STK-003, STK-005; SG-01 |

### 3.4 Supply, temperature and integrity interlocks

| ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
|---|---|---|---|---|---|---|---|
| SYS-038 | A ROM CRC failure shall lead to SAFE ≤ 200 ms after reset with CAN status continuing; an HSE or CSS clock failure shall lead to SAFE ≤ 200 ms with CAN silent and exit only by power-on reset. | (a) Corrupt-CRC image → SAFE ≤ `lim_safe_entry_ms`, `DcuSts_Mode` SAFE, B1A50. (b) Fault injection CSS → SAFE, `$LSSTA` SAFE on HSI, no DCU frames on CAN, B1A53, U1B00 at the CGW; manual: power-up with the MCO disabled → SAFE ≤ 200 ms. | D | HIL, M | A | [SAF] | STK-014; SG-01, SG-02, SG-03 |
| SYS-039 | With the KL30 sense module fitted, the DCU shall start the window or the lock only with KL30 between 9.0 and 16.0 V, and shall stop the window when KL30 is below 8.0 V for 100 ms or above 16.5 V for 20 ms. | SCPI ramps with thresholds verified at nominal ± 0.3 V: undervoltage stop ≤ `t_vbat_uv_ms` + 11 ms; overvoltage stop ≤ `t_vbat_ov_ms` + 11 ms; starts rejected outside the start window; over-voltage tests ≤ 17.0 V. | D | HIL | A | [SAF] | STK-008; SG-01, SG-03 |
| SYS-040 | When the ECU temperature exceeds 85 °C, the DCU shall reject window starts and stop a running window until the temperature falls below 80 °C. | Emulated sensor ramps across both thresholds: StopReason OVERTEMP and B1A32 above 85 °C (2 samples); starts accepted again below 80 °C. | D | HIL | A | [SAF] | STK-008, STK-009; SG-03 |
| SYS-041 | While closing, the window shall detect an obstacle and reverse toward open. | Defined with stage E. | D | — | E | [SAF] | STK-004; SG-01 |
| SYS-043 | With the KL30 sense module fitted, the DCU shall measure KL30 within ± 0.2 V over 8.0–16.0 V after a 2-point calibration; VREFINT shall be used only for VDDA plausibility. | TST-MAN-SYS-010: PSU at 9.0, 12.0 and 16.0 V with a DMM at the module input; `DcuSts_Vbat` and DID 0xFD02 within ± `lim_kl30_accuracy_mv`. | D, H | M, HIL | A | [SAF] | STK-008; SG-01 |

### 3.5 Temperature measurement

| ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
|---|---|---|---|---|---|---|---|
| SYS-008 | The DCU shall sample the ECU temperature every 1000 ± 50 ms and report it with 0.01 °C resolution. | Emulated sensor values reproduced exactly with the rounding of LS-SAIC §5.4; real sensor within ± 0.3 °C of a reference at 20–30 °C; period 1000 ± `lim_temp_period_tol_ms` over 600 samples. | D, H | HIL, M | A | — | STK-009 |
| SYS-009 | The DCU shall send one `$LSTMP` sentence per sample over UART as defined in LS-SAIC §9. | 600 sentences: 0 checksum errors, 0 sequence gaps, period 1000 ± 50 ms. | D | HIL | A | — | STK-009 |
| SYS-010 | The temperature shall be available on CAN and in the APP. | The CAN value equals the UART value for the same sequence number; the APP shows the value within 3 s and greys a stale value (widget test). | D, C, A | HIL, UT | A | — | STK-009 |
| SYS-090 | The UART telemetry shall follow LS-SAIC §9 and shall add less than 50 µs of jitter to the 1 ms scheduler tick. | Parser checks over a 10 min capture (checksum, length, sequences, monotonic time); trace pins show tick jitter < `lim_uart_jitter_us` with telemetry active. | D | HIL | A | — | STK-009 |

### 3.6 Communication, status and timing

| ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
|---|---|---|---|---|---|---|---|
| SYS-025 | The CGW shall push a StatusUpdate ≤ 50 ms after a DCU status frame carrying a change, with snapshots at 1 Hz when idle and 10 Hz while the window moves; the APP shall mark status older than 3 s as stale. | Reference = first DCU status frame carrying the change (CAN capture): APP-simulator receipt ≤ `lim_status_push_ms` for 100 changes; snapshot periods within ± 10 %; APP widget tests for staleness. | C, A | HIL, UT | A | — | STK-001; SG-04 |
| SYS-026 | Cyclic CAN frames shall keep their periods within ± 10 % and the bus load shall stay ≤ 10 %. | 10 min logs while idle and during repeated window presses: period deviation ≤ `lim_can_cycle_tol_pct` for every cyclic frame; bus load ≤ `lim_bus_load_pct`. | C, D | HIL | A | — | STK-007 |
| SYS-062 | Both nodes shall send NodeSts every 100 ms and Version every 1 s; a CAN matrix major-version mismatch shall reject window and door commands with REJECTED_VERSION and set a DTC. | Restbus sends major version 2 → after 3 NodeSts frames REJECTED_VERSION and U1A03 (DCU) or U1B03 (CGW); periods within ± 10 %. | D, C | HIL | A | — | STK-007 |
| SYS-063 | After a bus-off, transmission shall stop, a DTC shall be set and communication shall resume ≤ 1 s after the fault is removed, without reset. | CANH–CANL short for 1 s: U1A01 and U1B01; transmission resumes ≤ `lim_busoff_resume_ms` after removal (expected ≤ 0.51 s). | D, C | HIL | A | — | STK-007 |
| SYS-072 | The DCU shall reach NORMAL ≤ 300 ms after power-up and the CGW shall start its SoftAP ≤ 3 s after power-up. | Power-cycle tests: DCU_NodeSts NORMAL ≤ `lim_init_normal_ms`; CGW TRACE0 500 µs pulse (`WIFI_EVENT_AP_START`) ≤ `lim_ap_up_ms`; host association and session times reported for information only. | D, C | HIL | A | — | STK-006, STK-008 |

### 3.7 Security

| ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
|---|---|---|---|---|---|---|---|
| SYS-050 | Only authenticated sessions shall be able to command; an authentication with a wrong key shall get REJECTED_AUTH and cause no actuation. | 100 wrong-key attempts → 0 actuations, AuthResult(REJECTED_AUTH), close 4002. | C | HIL | A | [SEC] | STK-006; CSG-01 |
| SYS-051 | Frames replayed from the same or an earlier session shall be rejected. | Capture and replay → 0 actuations; session closed with 1008. | C | HIL | A | [SEC] | STK-006; CSG-02 |
| SYS-052 | A second client shall get REJECTED_BUSY while a controller session is alive. | Two APP simulators: the second gets AuthResult(REJECTED_BUSY) and close 4003; an authentication with the controller's client_id pre-empts the old session with a STOP latch. | C | HIL | A | [SEC] | STK-006; CSG-01 |
| SYS-053 | In RC and RELEASE builds the SoftAP shall offer WPA3-SAE with PMF required and never open, WEP, WPA1 or TKIP security. | Beacon capture: AKM SAE only, MFPR = 1; RC and RELEASE images contain no transition-mode configuration. | C | M, R | A | [SEC] | STK-006; CSG-01 |
| SYS-054 | Pairing shall require holding BOOT for ≥ 5 s, the pairing window shall close after 120 s, and keys and the passphrase shall never be logged. | Pairing test; window closes after 120 s; a pending key without key confirmation is discarded; log scan finds 0 key or passphrase occurrences in stored logs, evidence and CI artefacts. | C | HIL, R | A | [SEC] | STK-006; CSG-03 |
| SYS-055 | Flooding or malformed frames shall close the session, stop the motor per SYS-021, and the CGW shall accept a new session ≤ 5 s later. | Fuzzing and flooding (40 frames/s) with the APP simulator: close 1008 when the rolling-window limit is exceeded; STOP; a new session is accepted ≤ 5 s later. | C | HIL | A | [SEC] | STK-006; CSG-02, CSG-04 |
| SYS-056 | After 3 failed authentications the CGW shall answer every further attempt made within 5 s of the previous failure with REJECTED_RATE_LIMIT. | Timing log: attempts within 5 s → AuthResult(REJECTED_RATE_LIMIT) and close 4006; an attempt after 5 s is verified normally. | C | HIL | A | [SEC] | STK-006; CSG-01 |
| SYS-057 | Holding BOOT for ≥ 10 s shall erase K_pair, generate a new passphrase, restart the SoftAP and require re-pairing. | TST-HIL-SYS-020 (d): the old key gets close 4005; the new passphrase differs from the old one; re-pairing succeeds. | C | HIL | A | [SEC] | STK-006; CSG-03 |

### 3.8 Diagnostics, resources and endurance

| ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
|---|---|---|---|---|---|---|---|
| SYS-060 | The DCU shall implement the UDS-lite services and DIDs of LS-SAIC §7.8 with the specified negative responses. | udsoncan test suite: every service, sub-function and DID; every listed NRC provoked once; the SAFE latch cleared by 0x11 0x01 only in the extended session. | D | HIL | A | — | STK-014 |
| SYS-061 | DTCs shall be stored with status bits 0, 2, 3 and 5 and shall be readable after the fault and after a software reset; persistence across power-off is LATER. | Each stage A DTC injected → UDS 0x19 0x02 lists it with the correct status; still listed after 0x11 0x01; `$LSDTC` lines; CGW DTCs visible as Notices and in `CgwSts_DtcCount`. | D, C | HIL | A | — | STK-014 |
| SYS-070 | The DCU shall stay at ≤ 60 % CPU load and ≤ 75 % stack use without a heap; the CGW shall keep ≥ 30 % free internal heap. | DIDs 0xFD06 and 0xFD07 and `DcuSts_CpuLoadMax` after the soak; map-file check finds no heap symbols; `CgwSts_HeapFreePct` ≥ `lim_cgw_heap_free_pct` at the end of the soak. | D, C | HIL, R | A | — | STK-014 |
| SYS-071 | An 8 h HIL-SIM soak with ≥ 3,000 window presses and ≥ 300 lock cycles shall end with 0 resets and 0 unexpected DTCs. The 24 h soak with 10,000 window and 1,000 lock cycles is LATER. | Weekly HIL-SIM soak and one per release candidate; about 200 attended HIL-REAL cycles per release candidate. | C, D, A | HIL | A | — | STK-014 |

### 3.9 Hardware and bench

| ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
|---|---|---|---|---|---|---|---|
| SYS-080 | The power path shall follow LS-SAIC §2.7, and a reversed KL30 with the PSU limited to 0.5 A shall cause no damage. | Design review of the bench against §2.7; limited reverse-polarity test at 0.5 A followed by a functional check. | H | R, M | A | — | STK-013, STK-016 |
| SYS-081 | Actuator current shall flow only through shield terminals, the fuse, WAGO connectors and 18 AWG wire; no breadboard shall be used. | Inspection checklist TST-MAN-SYS-015 with photos. | H | R | A | — | STK-016 |

### 3.10 APP

| ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
|---|---|---|---|---|---|---|---|
| SYS-095 | The APP shall disable its controls and state the reason when the session is not authenticated, the DCU is offline, a mode or inhibit flag does not allow the function, or the status is stale. | Widget tests and integration tests against the CGW simulator for each reason. | A | UT, IT | A | — | STK-001, STK-005; SG-04 |

---

## 4. Hardware requirements

| ID | Requirement | Verification | Parents |
|---|---|---|---|
| HWR-001 | The DCU hardware shall consist of the modules of LS-SAIC §2.1 connected with the pin allocation of LS-SAIC §3.1. | Inspection against the wiring list; BU-03 | SYS-005, SYS-081 |
| HWR-002 | The shield jumper JP9 shall not be fitted, and nothing shall be connected to the NUCLEO VIN or E5V pins. | Inspection | SYS-080 |
| HWR-003 | KL30 shall be supplied by a bench PSU limited to 3 A (5 A only for attended lock stall tests) through a 7.5 A blade fuse; the PSU output state at power-on shall be OFF. | Inspection; BU-12 | SYS-080 |
| HWR-004 | The CAN bus shall be terminated by the two node boards (60 Ω ± 5 % measured unpowered), with the USB-CAN adapter mid-bus and its termination off. | DMM measurement; BU-05 | SYS-026, SYS-063 |
| HWR-005 | The encoder shall be powered from the NUCLEO 3.3 V supply and connected to PA15 (A) and PB3 (B). | Inspection; BU-06 | SYS-042 |
| HWR-006 | The TMP117 module shall be connected to I2C1 (PB8/PB9) at address 0x48 and placed close to the motor driver shield. | Inspection; BU-10 | SYS-008, SYS-040 |
| HWR-007 | The lock position switch shall connect PB12 to GND; its polarity shall be calibrated (`lock_fb_locked_level`). | BU-08 | SYS-001, SYS-003 |
| HWR-008 | The KL30 sense module (ratio 1/5) shall be connected to PA4 whenever SYS-039 or SYS-043 is verified. | Inspection; BU-09 | SYS-039, SYS-043 |
| HWR-009 | Motor and actuator return current shall flow only through the shield GND terminal; logic grounds shall join through the USB grounds; the USB-CAN adapter shall be galvanically isolated. | Inspection | SYS-080, SYS-081 |
| HWR-010 | The window motor shall be fixed in its bracket, with nothing on the shaft that can wind up cables or clothing. | Inspection | STK-017 |
| HWR-011 | The lock EN/DIAG line shall reach PB4 (option B, trace cut) or PA6 (option A, injection checked). | BU-02 | SYS-003, SYS-034 |

---

## 5. Traceability summary

### 5.1 Stakeholder requirements → system requirements

| STK | SYS |
|---|---|
| STK-001 | SYS-001, SYS-002, SYS-024, SYS-025, SYS-095 |
| STK-002 | SYS-002, SYS-003, SYS-004, SYS-024, SYS-027, SYS-034 |
| STK-003 | SYS-005, SYS-007, SYS-020, SYS-021, SYS-022, SYS-023, SYS-030, SYS-031, SYS-033, SYS-035, SYS-042, SYS-096 |
| STK-004 | SYS-006, SYS-007, SYS-032, SYS-041 |
| STK-005 | SYS-095, SYS-096 |
| STK-006 | SYS-021, SYS-050, SYS-051, SYS-052, SYS-053, SYS-054, SYS-055, SYS-056, SYS-057, SYS-072 |
| STK-007 | SYS-022, SYS-026, SYS-037, SYS-062, SYS-063 |
| STK-008 | SYS-003, SYS-005, SYS-036, SYS-039, SYS-040, SYS-043, SYS-072 |
| STK-009 | SYS-008, SYS-009, SYS-010, SYS-040, SYS-090 |
| STK-013 | SYS-080 |
| STK-014 | SYS-038, SYS-060, SYS-061, SYS-070, SYS-071 |
| STK-016 | SYS-080, SYS-081 |
| STK-017 | HWR-010 |

STK-010, STK-011, STK-012, STK-015, STK-018 and STK-019 are process and design constraints verified by review of the corresponding documents (LS-STK-001 §4).

### 5.2 Goals → system requirements

| Goal | SYS |
|---|---|
| SG-01 | SYS-005, SYS-020, SYS-021, SYS-022, SYS-023, SYS-030, SYS-035, SYS-036, SYS-037, SYS-038, SYS-039, SYS-041, SYS-042, SYS-043, SYS-096 |
| SG-02 | SYS-006, SYS-023, SYS-031, SYS-032, SYS-033, SYS-038, SYS-042 |
| SG-03 | SYS-030, SYS-031, SYS-034, SYS-038, SYS-039, SYS-040 |
| SG-04 | SYS-001, SYS-007, SYS-025, SYS-095 |
| CSG-01 | SYS-050, SYS-052, SYS-053, SYS-056 |
| CSG-02 | SYS-051, SYS-055 |
| CSG-03 | SYS-054, SYS-057 |
| CSG-04 | SYS-055 |
| CSG-05 | LS-SEC-001 release-hardening step (no SYS requirement in v1.0) |

---

## 6. Changes from LS-SAIC v0.1 §10

| Change | Requirements | Amendment |
|---|---|---|
| New: single door transaction in flight | SYS-027 | CA-SYS-19 |
| New: window motion plausibility (NO_MOTION, DIR_MISMATCH) | SYS-042 | CA-SYS-07 |
| New: KL30 measurement accuracy (proposed as SYS-042 by the HIL design; renumbered because the approved plan assigns SYS-042 to motion plausibility) | SYS-043 | CA-SYS-31 |
| New: factory reset | SYS-057 | CA-SYS-32 |
| Moved to stage B/D: end positions; position accuracy moved to stage B | SYS-006, SYS-007, SYS-032 | CA-SYS-01 |
| Redefined for stage A: current-based stall replaced by the over-current backstop; stall now detected by SYS-042 | SYS-031 | CA-SYS-01, CA-DCU-22 |
| Maximum run time verified without end positions | SYS-030 | CA-SYS-01 |
| Two door controls | SYS-002 | CA-SYS-28 |
| Measurement references and APP-simulator limits | SYS-020, SYS-021, SYS-024, SYS-025 | CA-SYS-27, CA-SYS-39 |
| Dead time after the brake | SYS-033 | CA-SYS-34 |
| ROM CRC and clock failure split | SYS-038 | CA-SYS-35 |
| Over-voltage filter and verification tolerance | SYS-039 | CA-SYS-36 |
| Bus-off slow phase 500 ms | SYS-063 | CA-SYS-17 |
| CGW heap observable on CAN | SYS-070 | CA-SYS-21 |
| Soak duration and cadence | SYS-071 | CA-SYS-38 |
| "AP up" definition | SYS-072 | CA-SYS-38 |
| WPA3-only in RC and RELEASE | SYS-053 | CA-SYS-41 |
| Secret-scan acceptance | SYS-054 | CA-SYS-42 |
| DTCs readable after a software reset (no NvM) | SYS-061 | CA-SYS-43 |
| Modules only, no breadboard | SYS-081 | CA-SYS-44 |
| Reset glitch verification without discrete pull-downs | SYS-036 | CA-SYS-45 |
| Bus-load measurement idle and in motion; NvM exclusion removed | SYS-026 | CA-SYS-40 |

---

## 7. References

- LS-SAIC-001 v0.2, `docs/02_system/LS-SAIC.md`
- LS-STK-001, `docs/01_stakeholder/stakeholder_requirements.md`
- Amendment register, `docs/02_system/amendment_register.md`
- LS-SAF-001 (`docs/05_safety/safety_concept.md`), LS-SEC-001 (`docs/06_security/security_concept.md`), LS-VER-001 (`docs/07_verification/verification_strategy.md`), LS-HIL-001 (`docs/07_verification/hil_architecture.md`)
