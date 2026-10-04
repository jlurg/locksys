# LockSys Contract Amendment Register

| Field | Value |
|---|---|
| Document ID | LS-SAIC-001-AR |
| Version | 0.2 |
| Status | Draft for baseline |
| Owner | jlurg |
| Date | 2026-10-03 |
| Applies to | LS-SAIC-001 v0.1 → v0.2, LS-SRS-001 v0.2 |

---

## 1. Purpose and scope

This register records every amendment proposed against LS-SAIC-001 v0.1 by the node designs, their reviews, the cross-checks, the fact-checks, the consolidation pass and the approved stage A scope, together with the decision and the final wording now contained in LS-SAIC-001 v0.2. Each amendment has one new identifier `CA-<NODE>-nn`; the NODE is the area that owns the change (SYS = system contract and requirements, DCU, CGW, APP, HIL, GOV = repository, process and CI).

## 2. Conventions

| Decision | Meaning |
|---|---|
| ACCEPTED | Adopted as proposed |
| MODIFIED | Adopted with changes; the final wording states the change and any rejected part |
| DEFERRED | Accepted in principle for a later stage or LATER; not part of v1.0 |
| REJECTED | Not adopted; the reason is given |

Source keys:

| Key | Source |
|---|---|
| DCU CA-nn | DCU design amendments CA-01…CA-25 and their review revisions |
| CGW Ann | CGW design amendments A1…A25 and their review revisions |
| APP CA-nn | APP design amendments CA-APP-01…16, cited here as APP CA-01…CA-16 to avoid confusion with the register IDs |
| HIL CA-nn | HIL design amendments CA-01…CA-24 |
| GOV CA-nn | Repository and governance design amendments CA-01…CA-16 |
| AM-nn, Ln | Consolidated amendments AM-01…AM-53 and lead decisions L1–L5 of the consolidation pass (the recommended option of each lead decision is taken) |
| CAN X-nn, PROTO X-nn, HIL X-nn, GOV X-nn | Findings of the four cross-checks |
| FC-TC #n, FC-GOV #n | Fact-check findings (toolchains, governance) |
| VS | IAR Visual State integration study |
| PLAN §n | Approved plan (stage A scope, module hardware, decisions table) |
| SPEC §n | M0 implementation specification |

Final wording is normative only through the LS-SAIC section given in the last column.

---

## 3. System contract (CA-SYS)

| ID | Title | Sources | Decision | Final wording | LS-SAIC § |
|---|---|---|---|---|---|
| CA-SYS-01 | Stage A window scope | PLAN §7.3, PLAN §9, SPEC §1, HIL X-12 | ACCEPTED | The window of v1.0 is a free-spinning JGB37-520 encoder motor without mechanics or end positions; UP/DOWN only while held. End positions move to stage B (virtual, encoder) and stage D (switches): SYS-006 and SYS-032 become B/D, SYS-007 position accuracy becomes B, `WinSts_PosPct` = 255 in stage A, `WinSts_LimUp`/`LimDn` are reserved. SYS-031 becomes the current backstop; stall detection moves to SYS-042. Stage tags A/B/C/D/E/LATER are introduced. | 0.4, 5.2, 5.3, 10 |
| CA-SYS-02 | Module-based bench hardware | PLAN §7.4, SPEC §2, module hardware study | ACCEPTED | The v0.1 BOM, power, protection and signal-conditioning tables are replaced: Pololu shield #2507 on the NUCLEO headers, Waveshare SN65HVD230 boards at the bus ends with the USB-CAN adapter mid-bus (termination off), Adafruit TMP117 #4821 with STEMMA QT cable #4209, optional 0–25 V sense module on PA4, PSU 12 V limited to 3 A, 7.5 A blade fuse, no breadboard. The v0.1 P-FET, TVS, bulk capacitors, discrete pull-downs and limit switches are removed (TVS/bulk LATER, switches stage D). | 2 |
| CA-SYS-03 | DCU pin and resource allocation | SPEC §2, module hardware study, CAN X-15 | MODIFIED | Pin table of SPEC §2 with `AFIO->MAPR = 0x02000D02`; TIM2 = encoder (PA15/PB3), TIM3 = window PWM (PC7), TIM4 = lock PWM (PB6), TIM1 without pins; the ADC trigger source is left to the DCU design because TIM3 is no longer free (DCU CA-14 superseded in this part); unused CAN filter entries hold duplicates of valid IDs. | 3.1, 3.2 |
| CA-SYS-04 | No transceiver standby control in the MVP | SPEC §1, PLAN §7.4, PLAN §10 | ACCEPTED | The module exposes no Rs pin; a powered transceiver is always active. Fail-silence at reset relies on the controller staying in initialisation mode and on the open-input recessive behaviour of the transceiver (BU-05). Standby control is LATER (CGW GPIO6 reserved). | 1.4, 2.6, 7.5 |
| CA-SYS-05 | No NvM in the MVP | SPEC §1, PLAN §2.1, PLAN §10, MVP feasibility finding 1 | ACCEPTED | DTC memory lives in noinit RAM protected by CRC-32 (survives software and watchdog resets, lost at power-on reset); calibration values are build-time constants; the flash region 0x0801F000–0x0801FFFF stays reserved for a later NvM. | 3.2, 5.0, 13.1 |
| CA-SYS-06 | Interface artefact paths and shared libraries | AM-01, GOV CA-01, APP CA-08, PROTO X-13, GOV X-12, GOV X-13, GOV X-14, GOV X-15, SPEC §4 | MODIFIED | Proto stays at `interfaces/proto/locksys/app/v1/` (the rename to `interfaces/app_protocol/` is rejected: it breaks three consumers and gains nothing). Shared C is `libs/ls_e2e` and `libs/ls_common` (the separate `ls_crc` library is rejected; LsCrc8 lives in `ls_common`). Shared vectors in `interfaces/vectors/{e2e_v1,app_session_v1,crypto_kat}.json`. Vendored code under the root `third_party/`. Generated outputs go into the consumer directories of SPEC §4. | 1.5 |
| CA-SYS-07 | Encoder motion supervision | PLAN §7.3, motor study (JGB37-520) | ACCEPTED | New SYS-042 (NO_MOTION, DIR_MISMATCH), new enumeration EncoderStatus, new WindowStopReason value DIR_MISMATCH = 16 with the field widened to 5 bits, new parameters `enc_cpr`, `enc_dir_invert`, `enc_free_cps`, `t_start_grace_ms`, `no_motion_window_ms`, `no_motion_min_pct`, `no_motion_duty_min_pct`, `t_dir_mismatch_ms`, `dir_mismatch_min_counts`, `i_oc_backstop_ma`, `t_softstart_ms`. | 5.0, 5.3, 6.4 |
| CA-SYS-08 | Motion telemetry frame DCU_WinMotion | PLAN §7.3, contract task | ACCEPTED | New E2E-protected frame DCU_WinMotion 0x201, DLC 8, 50 ms, DataID 0x2004: EncSts (3 bits), Speed (signed 16 bits, 0.1 rpm), DutyPct (8 bits), PosCounts (signed 24 bits, wrapping). Chosen instead of extending DCU_WinSts, which has only five free bits. | 7.3, 7.4 |
| CA-SYS-09 | DCU_WinSts layout v0.2 | CAN X-02, PLAN §7.3 | ACCEPTED | StopReason 24\|5; LimUp 29 and LimDn 30 (reserved in stage A); WinResult 56\|4 (CommandResult); FltDirMismatch 60; reserved bits 15, 31, 61–63. Added to matrix 1.0 because it is not yet baselined. | 7.4 |
| CA-SYS-10 | DCU rejection latch and decision feedback | CAN X-02, HIL CA-10, AM-16 | ACCEPTED | A start refused by the DCU latches the PressId and reports the code in `WinSts_WinResult` with `WinSts_PressIdEcho`; the CGW latches its active press and acknowledges with that result when PressIdEcho matches and WinResult is not UNSPECIFIED, ACCEPTED or OK. | 5.2, 7.5 |
| CA-SYS-11 | Stale-frame defence | CAN X-01 | ACCEPTED | Both proposals are adopted. CGW: on bus-off, before recovery, every CGW_WinCmd slot still owned by the driver is rewritten to STOP with HoldAge 255 and a new counter; unreleased slots are reclaimed 50 ms after recovery. DCU: after any WinCmd INVALID state, RX timeout or reset, a start requires a VALID WinCmd with Req = STOP first (re-arm). | 5.2, 7.5 |
| CA-SYS-12 | PressId acceptance | DCU CA-18, AM-17, CAN X-11 | MODIFIED | One `lastLatchedPressId`, initialised with the first PressId received with E2E OK after reset (no separate start-up variable). A start needs PressId ≠ 0 and ≠ `lastLatchedPressId`; every stop of a press and every refused start latches it. | 5.2 |
| CA-SYS-13 | PressIdEcho definition | CAN X-10 | ACCEPTED | Active PressId while moving, otherwise the last latched PressId; after reset the first PressId received with E2E OK. | 7.4 |
| CA-SYS-14 | E2E receiver details | CAN X-12 | MODIFIED | All frames are checked in arrival order; Δ > MaxDelta resynchronises the reference; the sender assigns the counter at mailbox load or driver enqueue; a DLC mismatch is a CRC error. Added DBC attributes `LsE2eMode` (None, Cyclic, Event) and `LsE2eMaxDelta`. | 7.2, 7.9 |
| CA-SYS-15 | CAN start-up ordering | DCU CA-19, AM-24, CAN X-07 | MODIFIED | Controller stays in initialisation until the node can transmit valid frames; DCU CAN starts at scheduler start with NodeMode INIT, first DCU_NodeSts ≤ 100 ms after reset. The Rs ordering rule ("standby off ≥ 1 ms before enable; never normal mode while in standby") applies only once standby control exists (LATER). | 6.1, 7.5 |
| CA-SYS-16 | CAN matrix version handling | CAN X-08 | ACCEPTED | `n_ver_debounce` = 3 on both nodes; an unknown peer version (no NodeSts yet) is treated as not matching for commands. | 7.5 |
| CA-SYS-17 | Bus-off recovery | CGW A20, HIL CA-19, AM-29, CAN X-03, HIL X-04, CAN X-17 | ACCEPTED | Both nodes: 5 attempts every 100 ms, then every 500 ms; counter reset and DTC healing after 10 s error-free; rejoin needs 128 × 11 recessive bits; worst-case resumption ≤ 0.51 s (SYS-063). Parameters `t_busoff_fast_ms`, `n_busoff_fast`, `t_busoff_slow_ms`, `t_busoff_heal_ms`. | 7.6, 5.0.6 |
| CA-SYS-18 | HoldAge after a non-release latch | CAN X-13 | ACCEPTED | After any non-release STOP latch the CGW sends HoldAge raw 255, so the DCU reports HOLD_TIMEOUT instead of RELEASED. | 5.2, 7.4 |
| CA-SYS-19 | Door transaction rules and SYS-027 | DCU CA-13, CGW A11, HIL CA-11, AM-15, CAN X-04, CAN X-16, HIL X-07 | ACCEPTED | One transaction in flight, owned by the CGW; only the CGW generates REJECTED_BUSY. The DCU sets `LastReqId` with `LastResult` = ACCEPTED in the same DoorSts when execution starts, then OK, FAILED_x or REJECTED_x; it ignores and logs new ReqIds while executing; Req not LOCK/UNLOCK → REJECTED_INVALID. New SYS-027: "one in flight; a second gets REJECTED_BUSY from the CGW; repetitions of one ReqId cause at most one actuation sequence". | 5.1, 7.4 |
| CA-SYS-20 | Per-DTC inhibit policy | CAN X-05 | ACCEPTED | Function inhibits are defined per DTC (target none/window/window_dir/lock/window_lock; release heal/timeout/test_after/clear) instead of per fault class, so healing actuations stay possible. New signals `DcuSts_WinInhibit` and `DcuSts_LockInhibit` report the effective inhibits. | 6.3, 13.2 |
| CA-SYS-21 | CGW heap on CAN and SYS-070 | HIL CA-12, AM-30, HIL X-20 | ACCEPTED | `CgwSts_HeapFreePct` 48\|8 = ⌊100 × free internal heap / total⌋, updated every 1 s; it is the SYS-070 CGW criterion on RELEASE builds. In matrix 1.0 (pre-baseline). | 7.4 |
| CA-SYS-22 | Interface versions before the baseline | Consolidation summary | ACCEPTED | CAN matrix 1.0, APP protocol 1.0 and UART telemetry 1 absorb the additive v0.2 changes without a version change; after the baseline every change is versioned. | 0.3, 14.3 |
| CA-SYS-23 | StatusUpdate additions | APP CA-06, AM-35, PROTO X-17, PROTO X-22, PLAN §7.3 | MODIFIED | Fields 17 `door_fault_flags`, 18 `window_fault_flags` (bit 8 = FltDirMismatch), 19 `pairing_active`, 20 `window_speed_rpm_x10`, 21 `window_encoder_status`, 22 `window_inhibited`, 23 `door_inhibited`. `status_age_ms` = maximum age of DCU_WinSts, DCU_DoorSts and DCU_NodeSts (NodeSts added per PROTO X-17). `last_door_request_id`/`last_door_result` are 0/UNSPECIFIED until the session sends its first DoorCommand. | 8.6, 8.8 |
| CA-SYS-24 | Result mapping tables and notice catalogue | AM-16, AM-38, APP CA-05, APP CA-11, PROTO X-11 | ACCEPTED | Normative WindowStopReason → CommandResult table and CGW latch reason → CommandResult and notice code table. `Notice.code` ≥ 0x010000 = 24-bit DTC; lower codes are non-DTC notices from `interfaces/dtc/dtc_catalog.yaml` (section `notices`). | 6.5, 8.9 |
| CA-SYS-25 | CAN → StatusUpdate value mapping | CGW A12, AM-20 | ACCEPTED | TempSts −32768 → `temperature_cdeg` 0 with the meaning in `temp_status`; Vbat raw 255 → `vbat_dv` 0; stale window/door → UNKNOWN, position 255; the full mapping table includes the new fields. | 8.8 |
| CA-SYS-26 | Parameter registry | DCU CA-04, APP CA-12, CGW A6, CGW A7, CGW A22, HIL CA-16, AM-13, CAN X-03, CAN X-08, PLAN §7.3 | MODIFIED | One registry with machine-friendly keys (unit suffixes, owner, stage, calibration flag), the source of `interfaces/params/timing.yaml`. CAN frame timing stays only in the DBC attributes. Stage A drops `I_OPEN_WIN`, `I_OC_WIN` and `T_NVM_QUIET`, moves `T_LIM_LEAVE`/`T_BOTHLIM` to stage D and `T_WIN_TRAVEL_DEF` to stage B (`win_travel_counts`); the over-voltage filter is 20 ms (10 ms rejected, see CA-SYS-36). | 5.0 |
| CA-SYS-27 | Latency measurement references | HIL CA-13, APP CA-09, AM-14, HIL X-14, HIL X-21, HIL X-35, PROTO X-21 | MODIFIED | With the APP simulator the APP allocations are subtracted: SYS-020 transmission → off ≤ 130 ms plus the component check CGW TRACE0 → off ≤ 25 ms and a degraded-link sub-case; SYS-021 referenced to the CGW TRACE0 pulse of the last keep-alive and extended with a CGW core-hang case; SYS-024 ≤ 264 / 964 / 1964 ms from DoorCommand transmission. The 284 ms variant of PROTO X-21 is rejected because the requirement reference is the user commit, so both APP allocations apply. | 5.0.10, SRS §3 |
| CA-SYS-28 | Two door controls (SYS-002) | APP CA-13, AM-41, L4, HIL X-29 | ACCEPTED | A lock-state control (shows the state; a tap refreshes and never actuates) and one lock/unlock control (UNLOCK when the fresh state is LOCKED, otherwise LOCK; UNLOCK with an 800 ms hold-to-confirm). Acceptance moves to an APP integration test plus a HIL mapping check. | 8.7, SRS §3.1 |
| CA-SYS-29 | Temperature rounding | HIL CA-18, AM-22 | ACCEPTED | `cdeg = sign(raw) × ⌊(abs(raw) × 78125 + 50000) / 100000⌋`; CAN carries cdeg; UART prints cdeg / 100 with two decimals. | 5.4, 7.4 |
| CA-SYS-30 | UART telemetry 1 changes | PLAN §7.3, AM-03, HIL X-08 | ACCEPTED | `$LSSTA` gains a signed speed field; new `$LSMOT` sentence (state, duty, speed, position, encoder status) at 10 Hz while driving; inbound `!LSFI` grammar for DEV/HIL builds; parser regex covers `$LS…`, `#LOG` and `!LSFI`; the NvM erase log event is dropped (no NvM). | 9 |
| CA-SYS-31 | KL30 measurement accuracy (SYS-043) | DCU CA-22, HIL CA-22, AM-42 | MODIFIED | New SYS-043 (AM-42 proposed SYS-042, which the approved plan assigns to motion plausibility): ± 0.2 V over 8.0–16.0 V after a 2-point calibration at 9.00 V and 16.00 V; the range ends at 16 V because the 1/5 module saturates near 16.5 V; calibration values are build-time constants (no NvM); VREFINT only for VDDA plausibility. | 5.0.4, SRS §3.4 |
| CA-SYS-32 | Factory reset (SYS-057) | HIL CA-23, AM-44, HIL X-34 | ACCEPTED | New SYS-057: BOOT ≥ 10 s erases K_pair, generates a new passphrase, restarts the AP and requires re-pairing; verified by TST-HIL-SYS-020; the CGW software requirement for the factory reset traces to SYS-057. | 8.5, SRS §3.7 |
| CA-SYS-33 | Status push timing | CGW A13, AM-20, contract consolidation | MODIFIED | "Coalesced over 50 ms" = minimum gap 40 ms plus the 10 ms tick. Added: changes of DoorLockState, the last door result, WindowState and the stop reason are pushed at the next tick without the gap, which keeps the SYS-024 worst case at 267 ms. | 5.1, 5.5 |
| CA-SYS-34 | Dead time after the brake (SYS-033) | DCU CA-05, AM-23, HIL X-18 | ACCEPTED | Dead time = bridge off ≥ 150 ms after the 100 ms brake, before any restart; acceptance checks INA = INB = 0 and PWM low continuously ≥ 150 ms. | 5.2, SRS §3.2 |
| CA-SYS-35 | ROM CRC and clock failure (SYS-038, SS-DCU) | DCU CA-01, HIL CA-14, AM-11, CAN X-14, HIL X-17 | MODIFIED | SYS-038 split: ROM CRC → SAFE ≤ 200 ms with CAN status continuing; HSE/CSS failure → SAFE ≤ 200 ms with CAN silent and exit only by power-on reset. SS-DCU gains the clock-failure exception. CAN silence is achieved by keeping bxCAN in initialisation mode (no standby pin, CA-SYS-04). Tests add the CSS fault injection and a start-up without MCO. | 6.1, 11.2, SRS §3.4 |
| CA-SYS-36 | Over-voltage reaction (SYS-039) | AM-43, AM-13, HIL CA-16, HIL X-11 | MODIFIED | Stop if KL30 > 16.5 V for 20 ms (`t_vbat_ov_ms`); acceptance off ≤ filter + 11 ms read from the registry; thresholds verified at nominal ± 0.3 V; the 10 ms filter proposed by the HIL design is rejected in favour of the DCU value. SYS-039 applies when the KL30 sense module is fitted. | 5.0.4, SRS §3.4 |
| CA-SYS-37 | Safety mechanism list | PLAN §1, PLAN §7.3, VS | MODIFIED | SM IDs are kept stable: SM-06 becomes end-position protection (B/D); SM-07 becomes the current backstop plus EN/DIAG; SM-15 relies on the VNH5019 inputs and safe initialisation (no discrete pull-downs, no Rs pull-up); SM-17 notes that TIM3/TIM4 have no break input; new SM-18 (encoder plausibility) and SM-19 (model-independent safety layer). | 11.3 |
| CA-SYS-38 | Soak and start-up verification (SYS-071, SYS-072) | GOV CA-12, AM-45, HIL CA-20, AM-46, PLAN §7, MVP feasibility finding 15 | MODIFIED | SYS-071: 8 h HIL-SIM soak (≥ 3,000 window and ≥ 300 lock cycles), weekly and once per release candidate, plus about 200 attended REAL cycles per release candidate; the 24 h / 10,000-cycle soak is LATER. SYS-072: "AP up" = `WIFI_EVENT_AP_START`, seen as the CGW TRACE0 500 µs pulse; association times are informative. | SRS §3.6, §3.8 |
| CA-SYS-39 | Status push measurement reference (SYS-025) | HIL X-15 | ACCEPTED | The reference is the first DCU status frame carrying the change; plant-to-APP latency is reported separately. | SRS §3.6 |
| CA-SYS-40 | CAN timing verification (SYS-026) | DCU CA-16, AM-28, HIL X-16 | MODIFIED | The NvM-erase exclusion is removed (no NvM in the MVP); bus load and cycle timing are measured idle and during repeated window presses. | SRS §3.6 |
| CA-SYS-41 | SoftAP security requirement (SYS-053) | APP CA-14, AM-40, L3, PROTO X-08 | ACCEPTED | SYS-053 is unchanged for RC and RELEASE builds (WPA3-SAE only, PMF required); transition mode exists only in DEV builds; iOS uses a manual join if the programmatic WPA3 join fails. | 8.1, SRS §3.7 |
| CA-SYS-42 | Secret-scan acceptance (SYS-054) | GOV CA-03, HIL CA-21, AM-37 | ACCEPTED | SYS-054 acceptance: 0 key or passphrase occurrences in stored logs, evidence and CI artefacts. | SRS §3.7 |
| CA-SYS-43 | DTC readout without NvM (SYS-061) | SPEC §1, CA-SYS-05 | ACCEPTED | DTCs are readable after the fault and after a software reset; persistence across power-off is LATER. | 13.1, SRS §3.8 |
| CA-SYS-44 | Modules only (SYS-081) | PLAN §7.4, HIL X-28 | ACCEPTED | No breadboard; actuator current only through shield terminals, fuse, WAGO connectors and 18 AWG wire; inspection checklist TST-MAN-SYS-015. | 2, SRS §3.9 |
| CA-SYS-45 | Reset glitch verification (SYS-036) | Module hardware study, HIL X-27 | ACCEPTED | The shield has no discrete pull-downs; SYS-036 is verified over 100 power cycles, 100 NRST pulses and 20 watchdog resets at ≥ 100 MS/s. | 2.9, SRS §3.2 |

---

## 4. DCU (CA-DCU)

| ID | Title | Sources | Decision | Final wording | LS-SAIC § |
|---|---|---|---|---|---|
| CA-DCU-01 | HSE/CSS failure behaviour | DCU CA-01, HIL CA-14, AM-11, CAN X-14 | MODIFIED | On HSE or CSS failure the DCU runs the HSI64 profile in SAFE with bxCAN held in initialisation mode (no transmission, no acknowledgement), reports SAFE and B1A53 over UART and leaves SAFE only by power-on reset. Rationale: HSI −1.1…+1.8 % at 25 °C and −2…+2.5 % over temperature exceed the 0.485 % tolerance. The v0.1 "Rs high" element is dropped (no standby pin). | 4, 6.1 |
| CA-DCU-02 | TMP117 register access | DCU CA-02, AM-09 | ACCEPTED | Pointer write, STOP, then a new START (no repeated START) at 100 kHz, 88 kHz fallback; the sensor retains the pointer. Errata reference ES096 §2.8.5. | 5.4 |
| CA-DCU-03 | TMP117 one-shot acquisition | DCU CA-03, DCU CA-21, AM-21 | ACCEPTED | One-shot (MOD = 11, AVG = 8) every 1000 ms; completion via Data_Ready (expected ≤ 140 ms, timeout 200 ms); first write ≥ 1.5 ms after power-up; identity compared on 16 bits; factory default 0x0220 confirmed but still written. A missing Data_Ready is a failed sample (SENSOR_FAULT after 3); STALE is produced only by receivers. | 5.4 |
| CA-DCU-04 | Stage A calibration parameters | DCU CA-04 | MODIFIED | Kept: lock over-current 5.5 A / 20 ms, minimum stroke 150 ms, pulse hard cap 500 ms, over-voltage filter 20 ms, blanking 100 ms (now `t_cs_blank_ms` for the current backstop). Replaced: window stall 4.0 A and over-current 6.5 A by the 2.5 A / 50 ms backstop plus encoder supervision. Deferred: open-load threshold (LATER), limit timing (stage D), travel time (stage B). Dropped: NvM quiet time. | 5.0 |
| CA-DCU-05 | SAFE latch | DCU CA-06, AM-25, L5 | ACCEPTED | The latch survives watchdog and software resets (noinit); it is cleared only by a power-on reset or by UDS 0x11 0x01 received in the extended session (0x10 0x03); 0x11 0x01 in the default session is a plain reset. The extended session stays available in SAFE. | 6.1 |
| CA-DCU-06 | Reset reason rules | DCU CA-07, HIL CA-15, AM-27 | ACCEPTED | The DCU never reports BROWNOUT; flag priority IWDG > WWDG > SFT > LPWR > POR > PIN; a PVD event is recorded in noinit RAM. | 6.1 |
| CA-DCU-07 | Fault-injection channel | DCU CA-09, HIL CA-03, AM-03, HIL X-08, HIL X-09, GOV CA-10 | MODIFIED | DEV/HIL builds only: `!LSFI,<cmd>[,<arg>]*CS` on USART2 RX (the `$LSFIC` syntax is rejected because `$` belongs to the DCU → host grammar). Commands: HANG, HANG_IRQOFF, SKIP_CHECKPOINT, HARDFAULT, STACK, ROMCRC, E2E_TX, CLRRST, CSS, SEED. Symbol prefixes `Fi_` (DCU) and `cgw_fi_` (CGW); CI fails if they appear in an RC or RELEASE ELF (replaces the `FaultInj_` check). | 9.6 |
| CA-DCU-08 | No MPU assertion | DCU CA-10, AM-09 | ACCEPTED | The STM32F103xB has no MPU (`__MPU_PRESENT 0U`); the v0.1 assertion on `MPU->TYPE` is removed; the stack is placed at the bottom of SRAM so an overflow faults. | 3.2, 11.3 |
| CA-DCU-09 | ROM CRC parameters | DCU CA-11, AM-47, GOV CA-10, FC-TC #3 | ACCEPTED | `ielftool --fill "0xFF;0x08000000-0x0801EFFB" --checksum "ls_rom_crc:4,crc32:Li,0xFFFFFFFF;0x08000000-0x0801EFFB"`; CRC word at 0x0801EFFC; 0x0801F000–0x0801FFFF never filled; the release artefact is the post-ielftool image and CI recomputes the CRC; on-target confirmation pending (BU-13). Closes O8. | 3.2, 15.2 |
| CA-DCU-10 | CMSIS versions | DCU CA-12, GOV X-11 | ACCEPTED | CMSIS-Core 5.9.0 and cmsis_device_f1 v4.3.5 vendored under `third_party/` (Apache-2.0). | 1.5, 4 |
| CA-DCU-11 | Resource reservations | DCU CA-14, AM-09 | MODIFIED | LOCK_FB is polled (EXTI12 optional); ADC IN17 (VREFINT) only for VDDA plausibility. The TIM3 ADC trigger is superseded because TIM3 now drives the window PWM; the trigger source is a DCU design choice (TIM1 has no pin outputs). | 3.1, 3.2 |
| CA-DCU-12 | Watchdog-reset counter scope | DCU CA-15, AM-26, HIL CA-06 | ACCEPTED | The "≥ 3 watchdog or fault resets in 10 min" counter is valid per power cycle (SB45 ties VBAT to VDD); DEV/HIL builds clear it with `!LSFI,CLRRST`. | 6.1 |
| CA-DCU-13 | Runtime NvM erase and watchdog extension | DCU CA-16, AM-28 | DEFERRED | No NvM in the MVP (CA-SYS-05); the erase slicing, the extended IWDG setting and the related timing exception return with NvM (LATER). | — |
| CA-DCU-14 | Errata references | DCU CA-17, DCU CA-23, AM-09, FC-TC #8a, FC-TC #8b | MODIFIED | References use ES096 Rev 15 (supersedes DocID14574 Rev 13; renumbered sections mapped by title); ES0340 (Rev 17, high density) does not apply. Applicability: §2.1.3 (no `LDR SP`), §2.2.9 (marking check), §2.2.12 n/a, §2.3.1 n/a, §2.3.7/§2.3.8 n/a with PB5 as GPIO and TIM3 full remap, §2.3.9, §2.5.1, §2.6.4 n/a, §2.8.1–§2.8.7 handled, §2.11.1. | 3.3 |
| CA-DCU-15 | I2C bus recovery | DCU CA-20, AM-09 | ACCEPTED | Up to 9 SCL pulses and STOP; if SDA stays low, SCL low ≥ 45 ms (non-blocking); the ES096 §2.8.7 sequence; full re-initialisation after any peripheral software reset. | 5.4 |
| CA-DCU-16 | KL30 scaling and calibration | DCU CA-22, HIL CA-22 | MODIFIED | Module ratio 1/5; per-board gain and offset (`kl30_gain_x1000`, `kl30_offset_mv`) from a 2-point bench calibration as build-time constants (no NvM); an ADC reading at full scale counts as over-voltage; the sense module is optional (`kl30_sense_fitted`), and without it Vbat is INVALID and the supply interlock inactive. | 2.7, 5.0.4 |
| CA-DCU-17 | End-position switch timing clarification | DCU CA-24, AM-23 | DEFERRED | Moves to stage D with the switches: the ≤ 50 µs reflex target is measured from the MCU pin threshold; SYS-006 (D) stays ≤ 5 ms. | 5.2 |
| CA-DCU-18 | INIT exit and "Vbat valid" | CAN X-06 | ACCEPTED | "Vbat valid" = measurement available (ADC fresh, VDDA plausible), not in-window; KL30 out of window → B1A40 → DEGRADED; the INIT deadline leads to SAFE only for failed critical self-tests, otherwise to DEGRADED. | 6.1 |
| CA-DCU-19 | U1A00 detection | CAN X-09 | ACCEPTED | U1A00 = CGW_NodeSts missing > 500 ms or CGW_WinCmd missing > 1 s, confirmed after 1 s. | 13.2 |
| CA-DCU-20 | HardFault DTC B1A54 | HIL CA-24, AM-50, HIL X-13 | ACCEPTED | B1A54-48 "Software exception": WARNING, CRITICAL at the shared reset limit (with B1A52); fault PC and CFSR readable in DID 0xFD05. B1A51 now covers only the stack canary. | 13.2 |
| CA-DCU-21 | State-machine engine error DTC B1A55 | VS | ACCEPTED | DTC_SWC_FSM_ERROR = B1A55-48, CRITICAL: on any Visual State engine error code the adapter brakes or switches off the affected actuator without the engine, reports FAULT, sets B1A55 and requests SAFE. | 13.2, 11.3 |
| CA-DCU-22 | Stage A DTC set | PLAN §7.3, motor study | ACCEPTED | B1A11-71 redefined as NO_MOTION (stall, open motor circuit or encoder failure); new B1A15-64 DIR_MISMATCH (window inhibited until UDS 0x14 or power-on reset) and B1A16-19 current backstop (5 s window inhibit); B1A14 (current-based open load) LATER; B1A12 stage D; B1A13 without inhibit in stage A. | 13.2 |
| CA-DCU-23 | Visual State state machines | PLAN §2.1.1, VS, SPEC §1 | ACCEPTED | WinCtrl, DoorCtrl and ModeMgr are IAR Visual State models (Classic Coder, Adaptive API, readable C, no heap, no function pointers; generated code in `firmware/dcu/gen_vs/`); the safety layer stays hand-written below the models; DoorCtrl has a bounded-exit guard `t_lock_pulse_guard_ms`. | 1.2, 11.3 |
| CA-DCU-24 | Actuation serialisation | LS-SAIC v0.1 A5, HIL cross-check electrical summary | ACCEPTED | With the 3 A supply, window and lock never actuate together (`act_serialise`): a door request while the window drives → REJECTED_INTERLOCK; a window start during a lock actuation → REJECTED_INTERLOCK with the press latched. | 5.1, 5.2 |
| CA-DCU-25 | UDS-lite scope and DIDs | DCU CA-08, AM-31, MVP feasibility finding 2, PLAN §1, VS | MODIFIED | MVP services 0x10 01/03, 0x11 01, 0x14 FFFFFF, 0x19 02, 0x22, 0x3E. 0x19 04 and 0x19 0A are LATER (DCU CA-08 deferred). DIDs F18C, F195, FD00–FD09 with defined layouts; FD09 = state-machine transition coverage bitmap and trace ring. | 7.8 |
| CA-DCU-26 | Lock EN/DIAG pin options | Module hardware study, PLAN §7.4 | ACCEPTED | Option A: PA6 (D12) with ≈ 0.2 mA clamp injection; option B (recommended): cut the shield D12 trace and jumper to PB4 (D5); selected by build-time pin configuration after BU-02. | 3.1 |
| CA-DCU-27 | VNH5019 shield drive rules | Module hardware study, SPEC §2 | ACCEPTED | Coast = PWM 0; brake = INA = INB = 0 with PWM 100 %; EN/DIAG is a floating fault input only, never driven high and never used to disable; faults cleared by toggling INA/INB under the DTC policy; initialisation order PWM low, INA = INB = 0, EN/DIAG input, then MAPR, then timers. | 2.2 |
| CA-DCU-28 | Soft start and run duty | PLAN §7.3, motor study | ACCEPTED | Linear duty ramp over `t_softstart_ms` (200 ms) at every start to `win_duty_run_pct` (100 %); limits inrush on the 3 A supply. | 5.2 |

---

## 5. CGW (CA-CGW)

| ID | Title | Sources | Decision | Final wording | LS-SAIC § |
|---|---|---|---|---|---|
| CA-CGW-01 | TWAI bit timing | CGW A1, AM-10, CAN X-17, FC-TC #7a, FC-TC #7b, FC-TC #7c | ACCEPTED | Advanced timing `brp = 10, prop_seg = 0, tseg_1 = 13, tseg_2 = 2, sjw = 2, ssp_offset = 0` (exact field set of `twai_timing_advanced_config_t` in v5.5.5); never legacy macros or basic-mode timing; node driver `twai_new_node_onchip()`; `twai_node_recover()` is asynchronous. Closes O7. | 4 |
| CA-CGW-02 | ESP-IDF version | CGW A2, GOV CA-06, APP CA-02, AM-12, L1, PROTO X-04, GOV X-01, FC-GOV #2 | MODIFIED | Pin ESP-IDF v5.5.5 by image digest (option B of L1). The floating "≥ v5.5.3" proposal is rejected. Workarounds W1–W5 are no longer mandatory; `lru_purge_enable = false`, the application timeouts and the no-PONG rule remain design rules. AR2026-003 is a functional advisory covering SoftAP. | 4 |
| CA-CGW-03 | SoftAP security and DHCP options | CGW A3, CGW A19, HIL CA-08, HIL CA-09, APP CA-02, AM-32, L3, PROTO X-08 | MODIFIED | `sae_pwe_h2e = WPA3_SAE_PWE_BOTH`; DHCP offers without router option (`CONFIG_LS_SOFTAP_OFFER_ROUTER`, default n) and DNS option (`CONFIG_LWIP_DHCPS_ADD_DNS=n`); WPA3-only in RC and RELEASE; the transition profile (PMF capable, not required, transition disable 0, `sec=wpa2wpa3`) exists only in DEV builds. H2E-only is a later hardening option. | 8.1 |
| CA-CGW-04 | Counter seeding after a CGW reset | CGW A4 | ACCEPTED | PressId seeded with `WinSts_PressIdEcho + 1`, ReqId with `DoorSts_LastReqId + 1` (skip 0; random if nothing received); door commands get FAILED_COMM until the first DCU_DoorSts. | 5.1, 5.2 |
| CA-CGW-05 | Secret console output and HIL pairing | CGW A5, GOV CA-03, HIL CA-21, AM-37, HIL X-02 | MODIFIED | One marker pair `<<LS-SECRET-BEGIN>>` / `<<LS-SECRET-END>>` (the HIL `#LSQR-*` markers are rejected); every log consumer redacts the block and masks `locksys://pair`, `p=`, `k=`; console captures are never streamed; HIL secrets use a redacting type, no `--showlocals`; routine HIL pairing by an NVS image built from the secret store. | 8.5 |
| CA-CGW-06 | New-press admission | CGW A6, CGW A7, AM-18, PROTO X-09 | ACCEPTED | First WindowMove `hold_ms` ≤ 300 ms; a press starts only with an RTT ≤ 200 ms sampled within 2 s; idle Ping at 1 Hz and one Ping right after AuthResult(OK). | 8.7, 8.10 |
| CA-CGW-07 | Handshake timing and unauthenticated connections | CGW A8, CGW A22, AM-33, PROTO X-07 | ACCEPTED | Upgrade plus a valid ClientAuth within 2000 ms of TCP accept; at most one unauthenticated connection; plain HTTP to `/ws/v1` → 400 and close; a peer closing before the upgrade is not an authentication failure. | 8.2, 8.3, 8.4 |
| CA-CGW-08 | Single controller and pre-emption | CGW A9, APP CA-04, AM-34 | ACCEPTED | Same `client_id` pre-empts at once with a STOP latch for the old press; a different `client_id` gets REJECTED_BUSY and 4003 while the controller is alive; a stale controller (≥ 3 s) is closed with 4004; a WebSocket close or TCP FIN frees the session at once. | 8.4 |
| CA-CGW-09 | STOP-latch triggers and window acknowledgements | CGW A10, APP CA-05, AM-16 | ACCEPTED | Triggers: keep-alive timeout, RTT gate, direction change, session loss or pre-emption, station disconnect, bus-off, DCU lost, mode change, DCU rejection or stop, CGW 8.2 s backstop. `ref_id` = `press_id`; one acknowledgement for the first WindowMove, at most one more when the press ends other than by release. | 5.2, 8.7 |
| CA-CGW-10 | CGW DTC set | CGW A14, AM-50 | ACCEPTED | Adds U1B02-82, B1B11-96, B1B14-47, B1B15-44, B1B16-96; severities: U1B00/U1B01/U1B03/B1B10/B1B15 DEGRADED, U1B02 WARNING, B1B11/B1B16 CRITICAL, B1B12/B1B13/B1B14 WARNING (CRITICAL on a reset storm). | 13.3 |
| CA-CGW-11 | Partition table offset | CGW A15, AM-49 | ACCEPTED | `CONFIG_PARTITION_TABLE_OFFSET=0x11000` (smallest offset fitting a 64 KB Secure Boot v2 bootloader and its signature); `efuse_em` and `nvs_keys` partitions reserved now. | 12 |
| CA-CGW-12 | nanopb vendoring | CGW A16, GOV CA-02, FC-TC #6j | ACCEPTED | nanopb 0.4.9.2 runtime and generator from the GitHub release with a SHA-256 pin (PyPI only offers 0.4.9.1). | 1.4 |
| CA-CGW-13 | HTTP server LRU purge | CGW A17, AM-33 | ACCEPTED | `lru_purge_enable = false`, `max_open_sockets` = 3. | 8.2 |
| CA-CGW-14 | Pairing QR library, payload and rendering | CGW A18, APP CA-01, AM-36, PROTO X-02, HIL X-03 | MODIFIED | Nayuki qrcodegen v1.8.0 vendored (replaces `espressif/qrcode` 0.2.0, which logs the URI). Payload `v, id (16 uppercase hex), s, p (base32, no padding), k (base64url, no padding), b (BSSID), sec (wpa3 or wpa2wpa3)`, `h` only in simulator builds; not an OS-registered scheme. Added: fixed console rendering (two characters per module, dark = spaces, light = full blocks, 4-module quiet zone) so the HIL decoder matches the printer. | 8.5 |
| CA-CGW-15 | Adapter naming | CGW A21, AM-51 | ACCEPTED | CGW adapters are `*_esp.c`, or `*_<backend>.c` when host-portable (e.g. `cgw_crypto_psa.c`). | 14.2 |
| CA-CGW-16 | Keep-alive age reference | CGW A23, AM-19 | ACCEPTED | The keep-alive age is measured from the frame receipt time at WebSocket handler entry. | 5.2 |
| CA-CGW-17 | WebSocket control frames | CGW A24, PROTO X-15 | ACCEPTED | Clients never send WebSocket PONG; the CGW never sends WebSocket PING; a client PING is tolerated; liveness uses protobuf Ping/Pong only. | 8.2 |
| CA-CGW-18 | ResetReason after an EN reset | CGW A25, AM-27 | ACCEPTED | On ESP32-S3 an EN-pin reset reports POWER_ON; a USB reset reports PIN; HIL tests never expect PIN after an EN reset. | 6.2 |
| CA-CGW-19 | Session-scoped caches | PROTO X-01 | ACCEPTED | The press_id monotonicity check and the door request cache are keyed by session and cleared when a new session authenticates; a DoorCommand of a new session while an earlier transaction is PENDING gets REJECTED_BUSY. | 8.4, 8.7 |
| CA-CGW-20 | Close code 4006 | PROTO X-03 | ACCEPTED | 4006 = handshake timeout or throttled authentication; a throttled attempt gets AuthResult(REJECTED_RATE_LIMIT) at once; 4002 only after AuthResult(REJECTED_AUTH). | 8.3, 8.4 |
| CA-CGW-21 | Pairing commit point | PROTO X-06 | ACCEPTED | The CGW commits the pending K_pair on the first valid tagged APP → CGW frame of a session authenticated with it (key confirmation); the window stays open until then; a pending-key authentication pre-empts the controller with a STOP latch. | 8.5 |
| CA-CGW-22 | AuthResult session parameters | PROTO X-16 | ACCEPTED | `session_timeout_ms` = 3000, `keepalive_period_ms` = 100, `keepalive_timeout_ms` = 350, taken from the registry; the APP compares them with its own values. | 8.3 |
| CA-CGW-23 | Unknown message members | PROTO X-18 | ACCEPTED | An unknown Body member is ignored and counted; 1008 is reserved for integrity violations and known types in the wrong direction. | 8.2, 8.11 |
| CA-CGW-24 | device_id, Pong and SessionClose | PROTO X-20 | ACCEPTED | `device_id` = first 8 bytes of SHA-256 of the eFuse base MAC; both ends answer Ping with Pong; SessionClose is sent best effort before policy closes of an authenticated session, not before 1008. | 8.3, 8.4, 8.7 |
| CA-CGW-25 | CGW TRACE0 semantics | HIL CA-02, AM-04, AM-46, L2, HIL X-01 | MODIFIED | Option C of L2: TRACE0 is an RMT width-coded pulse in all builds (5/20/50/100/200/500 µs). Modified: the pulse is emitted as soon as the frame type is known, ≤ 0.5 ms after handler entry, because the width depends on the decoded type; the offset is part of the measurement uncertainty. TRACE1–3 keep the CGW semantics. | 3.5 |
| CA-CGW-26 | GPIO reservations | CAN X-18 | ACCEPTED | GPIO18 reserved for the self-test loopback (not on the bench); GPIO6 reserved for standby control (LATER). | 3.4 |
| CA-CGW-27 | Acceptance filter | Contract consolidation (CA-SYS-08) | ACCEPTED | The CGW acceptance filter passes 0x200–0x23F and {0x510, 0x590}, so the new DCU_WinMotion 0x201 is received; a software whitelist checks again. | 7.1 |

---

## 6. APP (CA-APP)

| ID | Title | Sources | Decision | Final wording | LS-SAIC § |
|---|---|---|---|---|---|
| CA-APP-01 | Failed AuthResult framing | APP CA-03, AM-34 | ACCEPTED | A failed AuthResult uses counter 0 and an empty tag, followed by the matching close code; the APP ignores `server_proof` when the result is not OK. | 8.3 |
| CA-APP-02 | No buffering, per-session IDs | APP CA-07, AM-34 | ACCEPTED | Commands are never buffered or replayed across disconnects or sessions; `press_id` and `request_id` restart at 1 per session; WindowStop on connectivity loss is best effort. | 8.4 |
| CA-APP-03 | APP platform facts | APP CA-10, AM-39 | ACCEPTED | Dart-owned sockets are not subject to cleartext or ATS policy; Android 17 `ACCESS_LOCAL_NETWORK` applies from targetSdk 37 (the APP stays on 36); the iOS local-network prompt may deny the first attempt; a Network Security Config replaces `usesCleartextTraffic`. | 8.12 |
| CA-APP-04 | Frame size and compression | APP CA-15, AM-33 | ACCEPTED | Both ends enforce 256 B per frame (CGW closes 1009, APP closes 1002); permessage-deflate is never negotiated. | 8.2 |
| CA-APP-05 | Rate limit definition | APP CA-16, AM-33, PROTO X-10 | ACCEPTED | More than 30 frames in any rolling 1000 ms window, every frame type counted (rolling window chosen over a token bucket). | 8.4 |
| CA-APP-06 | APP stop triggers and notices | PROTO X-11, PROTO X-18 | ACCEPTED | A Notice alone never stops a press; the APP latches on a non-ACCEPTED CommandAck, SessionClose, and a DCU-initiated stop seen in StatusUpdate (moving → non-moving with a reason other than NONE or RELEASED), whatever the reason value. | 8.7 |
| CA-APP-07 | Local-network probe and connect retry | PROTO X-07 | ACCEPTED | The probe is a TCP connect followed by an immediate close; the APP retries a refused WebSocket connect once after 250 ms before backing off. | 8.12 |
| CA-APP-08 | Pairing record retention | PROTO X-06 | ACCEPTED | The APP keeps its previous pairing record until the first successful authentication with the new key and may fall back once to the other key during the pairing window. | 8.5 |
| CA-APP-09 | iOS WPA3 decision gate | APP CA-14, AM-40, L3 | ACCEPTED | TST-MAN-APP-003 in WP0 on iOS ≥ 18 and Android 10/12+ (including behaviour without a router option); if iOS cannot join programmatically, a manual join in Settings is used (fallback 1); fallback 2 (transition mode) exists only in DEV builds. | 8.1 |

---

## 7. HIL (CA-HIL)

| ID | Title | Sources | Decision | Final wording | LS-SAIC § |
|---|---|---|---|---|---|
| CA-HIL-01 | Logic-analyser channel map | HIL CA-01, AM-04 | MODIFIED | 16-channel map adapted to the stage A pins (SYNC, window and lock bridge inputs, LOCK_FB, ENC_A/B, CAN RX, UART TX, I2C, DCU TRACE0 on PC0, CGW TRACE0) and four 8-channel profiles; a test whose profile is not wired is reported as skipped. | 2.10 |
| CA-HIL-02 | MVP bench without interface board | HIL CA-04, HIL CA-05, AM-05, MVP feasibility finding 7, PLAN §7, HIL X-24, HIL X-30 | DEFERRED | The relay matrix (K1–K14), contactor, e-stop chain and interface board are LATER. MVP: direct wiring to the shield top sockets and morpho pins, real or emulated TMP117 connected exclusively, optional relay module for the CAN short, PSU output off as emergency stop; stimulus outputs are high impedance whenever DCU 3V3 is absent. | 2.10 |
| CA-HIL-03 | DCU power on the HIL bench | HIL CA-06, AM-06, HIL X-10 | DEFERRED | The E5V supply from a PSU channel (with the back-feed acceptance check) is LATER; in the MVP the NUCLEO runs from USB, a power-on reset is a USB power cycle, and tests clear the SAFE latch with UDS 10 03 + 11 01. | 2.10 |
| CA-HIL-04 | Bench supply safety | HIL CA-16, GOV CA-15, AM-07, PLAN §7.4 | MODIFIED | KL30 current limit 3 A (5 A only for attended lock stall tests), OVP 18.0 V where supported, over-voltage tests ≤ 17.0 V, power-on output OFF, KL30 off at the end of every HIL job. The 5-minute idle-off task is LATER. | 2.7 |
| CA-HIL-05 | CGW BOOT automation | HIL CA-07, AM-08, CGW A5 | ACCEPTED | Stimulus open-drain line on GPIO0: high impedance during EN reset and flashing, never low when EN is released or at power-up. | 2.10 |
| CA-HIL-06 | Bench equipment open points | HIL CA-17, AM-53, PLAN §7.1, PLAN §7.2 | MODIFIED | O1: USB-CAN requirements plus the PCAN-USB FD recommendation, purchase pending. O2: NUCLEO-144 family recommended as stimulus MCU (the Pico 2 proposal is not adopted), confirmed at M5. O3: owned Saleae, 16 channels preferred, 8-channel profiles. O4: LAN SCPI preferred. | 15.2 |
| CA-HIL-07 | HIL-SIM plant models | PLAN §7, AM-21, HIL X-05 | ACCEPTED | The stimulus generates encoder A/B from the bridge inputs (with stall and reversed-direction injection), injects CS at the A0/A1 sockets while the bridges are unpowered (the shield's 10 kΩ series resistor isolates the VNH5019 CS outputs), emulates the lock switch and EN/DIAG, and emulates the TMP117 including one-shot timing, Data_Ready read-to-clear and pointer retention. | 2.10 |
| CA-HIL-08 | Evidence build | HIL X-32 | ACCEPTED | The DCU HIL build configuration (same sources and optimisation as RELEASE, trace pins and fault injection enabled) is the evidence image for SYS-020…SYS-023; tests needing trace or fault injection are tagged and excluded on RELEASE images. | 3.5 |

---

## 8. Repository, process and CI (CA-GOV)

| ID | Title | Sources | Decision | Final wording | LS-SAIC § |
|---|---|---|---|---|---|
| CA-GOV-01 | Code generation on the Lab Host | GOV CA-14, AM-02 | ACCEPTED | "IAR project builds need no code-generation tooling; the Lab Host provides Python 3.13 (uv) for CI gates and the stdlib-only version-header generator." | 1.4 |
| CA-GOV-02 | Versioning and release evidence | GOV CA-04, GOV CA-08, AM-52, GOV X-17 | ACCEPTED | `VERSION` is the single source; RC tags on `release/*` and `hotfix/*` heads, final tags on `main`; BuildType DEV/RC/RELEASE by tag; one version-header generator for all builds; release evidence attached to the immutable GitHub Release. | 14.3 |
| CA-GOV-03 | Trace tags | GOV CA-05, GOV X-26 | ACCEPTED | `@satisfies` is the only implementation tag; one tag per line with full IDs. | 14.1 |
| CA-GOV-04 | Self-hosted runner controls | GOV CA-07, GOV CA-13, DCU CA-25, AM-48, GOV X-04, GOV X-28, HIL X-37, FC-GOV #7 | MODIFIED | Self-hosted jobs only on `push`, `schedule` and `workflow_dispatch` (DCU CA-25's `merge_group` dropped); fail-closed host pre-job hook on repository, event and actor allowlist; PR creation limited to collaborators (`pull_request_creation_policy`), workflow approval for all external contributors (`approval_policy`); self-hosted jobs `contents: read`; SHA pinning including nested actions; immutable releases; bot changes only after a maintainer re-run. The signed-head check is DEFERRED (LATER). | 12 |
| CA-GOV-05 | HIL runner session | GOV CA-09 | ACCEPTED | The HIL runner uses an interactive session (Logic 2 automation); the headless API is LATER; open point O12. | 15.2 |
| CA-GOV-06 | Lab Host routing | GOV CA-11 | ACCEPTED | Ethernet keeps the default route; the APP simulator binds to the SoftAP interface (assumption A6). | 15.1 |
| CA-GOV-07 | Process open points | GOV CA-16, AM-53 | ACCEPTED | O12 single interactive session vs the HIL runner, O13 Dependabot under collaborator-only PR creation, O14 actionlint `concurrency.queue`. | 15.2 |
| CA-GOV-08 | Identifier digit counts | GOV X-24 | ACCEPTED | Three digits for STK, SYS, HWR, SWR and CSR; two for HAZ, SG, SM, AS, TS and CSG; the trace tool checks them per prefix. | 14.1 |
| CA-GOV-09 | Python protobuf version | PROTO X-05, GOV X-07, HIL X-31, SPEC §3 | ACCEPTED | One Python protobuf 6.33.x pin for the whole uv workspace (constrained by logic2-automation 1.0.11); Python stubs produced by a protoc 33.x release. | 1.4 |
| CA-GOV-10 | Deviation identifiers | GOV X-19 | ACCEPTED | `DEV-{DCU,CGW,LIB}-nnn` plus project permits `DP-nn` in one registry. | 14.1 |
| CA-GOV-11 | esptool command line | FC-TC #6p | ACCEPTED | Inside the ESP-IDF v5.5 environment esptool 4.12 syntax (`esptool.py write_flash`, `default_reset`) or `idf.py flash`; the HIL uses esptool 5.4.0 through its Python API. | 4 |
| CA-GOV-12 | IAR toolchain baseline | FC-TC #1a, FC-TC #1b, FC-TC #2a, FC-TC #2b, GOV X-18 | ACCEPTED | EWARM 9.70 baseline, 10.10 only if the licence allows; C-STAT command lines differ per version (`-cstat_analyze` exists only in 9.70); C-STAT covers selected MISRA rules, so the guideline enforcement plan assigns the rest; one pin file `tools/versions.env`. | 4, 15.2 |
| CA-GOV-13 | Naming conventions | AM-51, SPEC §5 | ACCEPTED | DCU prefix lists of SPEC §5 (including `Enc_`, `WinPos_`, `TBase_`, `Fi_`), shared-library names (`LsCrc8_`, `LsEvSet_`, `LsTmr_`, `LsRing_`), Visual State generated-code location and API prefix. | 14.2 |

---

## 9. Source index

Every source item and the register entry that carries its main change. An entry may also cite a source whose other aspects are carried by a different entry.

| Source | Register entry |
|---|---|
| DCU CA-01 … CA-25 | CA-01 → CA-DCU-01; CA-02 → CA-DCU-02; CA-03 → CA-DCU-03; CA-04 → CA-DCU-04; CA-05 → CA-SYS-34; CA-06 → CA-DCU-05; CA-07 → CA-DCU-06; CA-08 → CA-DCU-25; CA-09 → CA-DCU-07; CA-10 → CA-DCU-08; CA-11 → CA-DCU-09; CA-12 → CA-DCU-10; CA-13 → CA-SYS-19; CA-14 → CA-DCU-11; CA-15 → CA-DCU-12; CA-16 → CA-DCU-13; CA-17 → CA-DCU-14; CA-18 → CA-SYS-12; CA-19 → CA-SYS-15; CA-20 → CA-DCU-15; CA-21 → CA-DCU-03; CA-22 → CA-DCU-16; CA-23 → CA-DCU-14; CA-24 → CA-DCU-17; CA-25 → CA-GOV-04 |
| CGW A1 … A25 | A1 → CA-CGW-01; A2 → CA-CGW-02; A3 → CA-CGW-03; A4 → CA-CGW-04; A5 → CA-CGW-05 (stimulus GPIO0 line: CA-HIL-05); A6, A7 → CA-CGW-06; A8 → CA-CGW-07; A9 → CA-CGW-08; A10 → CA-CGW-09; A11 → CA-SYS-19; A12 → CA-SYS-25; A13 → CA-SYS-33; A14 → CA-CGW-10; A15 → CA-CGW-11; A16 → CA-CGW-12; A17 → CA-CGW-13; A18 → CA-CGW-14; A19 → CA-CGW-03; A20 → CA-SYS-17; A21 → CA-CGW-15; A22 → CA-CGW-07; A23 → CA-CGW-16; A24 → CA-CGW-17; A25 → CA-CGW-18 |
| APP CA-01 … CA-16 | 01 → CA-CGW-14; 02 → CA-CGW-03; 03 → CA-APP-01; 04 → CA-CGW-08; 05 → CA-CGW-09; 06 → CA-SYS-23; 07 → CA-APP-02; 08 → CA-SYS-06; 09 → CA-SYS-27; 10 → CA-APP-03; 11 → CA-SYS-24; 12 → CA-SYS-26; 13 → CA-SYS-28; 14 → CA-APP-09; 15 → CA-APP-04; 16 → CA-APP-05 |
| HIL CA-01 … CA-24 | 01 → CA-HIL-01; 02 → CA-CGW-25; 03 → CA-DCU-07; 04 → CA-HIL-02; 05 → CA-HIL-02; 06 → CA-HIL-03; 07 → CA-HIL-05; 08 → CA-CGW-03; 09 → CA-CGW-03; 10 → CA-SYS-10; 11 → CA-SYS-19; 12 → CA-SYS-21; 13 → CA-SYS-27; 14 → CA-DCU-01; 15 → CA-DCU-06; 16 → CA-HIL-04; 17 → CA-HIL-06; 18 → CA-SYS-29; 19 → CA-SYS-17; 20 → CA-SYS-38; 21 → CA-CGW-05; 22 → CA-SYS-31; 23 → CA-SYS-32; 24 → CA-DCU-20 |
| GOV CA-01 … CA-16 | 01 → CA-SYS-06; 02 → CA-CGW-12; 03 → CA-CGW-05; 04 → CA-GOV-02; 05 → CA-GOV-03; 06 → CA-CGW-02; 07 → CA-GOV-04; 08 → CA-GOV-02; 09 → CA-GOV-05; 10 → CA-DCU-09 (symbol check: CA-DCU-07); 11 → CA-GOV-06; 12 → CA-SYS-38; 13 → CA-GOV-04; 14 → CA-GOV-01; 15 → CA-HIL-04; 16 → CA-GOV-07 |
| AM-01 … AM-53 | 01 → CA-SYS-06; 02 → CA-GOV-01; 03 → CA-DCU-07; 04 → CA-HIL-01, CA-CGW-25; 05 → CA-HIL-02; 06 → CA-HIL-03; 07 → CA-HIL-04; 08 → CA-HIL-05; 09 → CA-DCU-11, CA-DCU-02, CA-DCU-14, CA-DCU-15, CA-DCU-08; 10 → CA-CGW-01; 11 → CA-DCU-01, CA-SYS-35; 12 → CA-CGW-02; 13 → CA-SYS-26; 14 → CA-SYS-27; 15 → CA-SYS-19; 16 → CA-CGW-09, CA-SYS-24, CA-SYS-10; 17 → CA-SYS-12; 18 → CA-CGW-06; 19 → CA-CGW-16; 20 → CA-SYS-25, CA-SYS-33; 21 → CA-DCU-03; 22 → CA-SYS-29; 23 → CA-SYS-34, CA-DCU-17; 24 → CA-SYS-15; 25 → CA-DCU-05; 26 → CA-DCU-12; 27 → CA-DCU-06, CA-CGW-18; 28 → CA-DCU-13, CA-SYS-40; 29 → CA-SYS-17; 30 → CA-SYS-21; 31 → CA-DCU-25; 32 → CA-CGW-03; 33 → CA-CGW-07, CA-CGW-13, CA-APP-04, CA-APP-05; 34 → CA-CGW-08, CA-APP-01, CA-APP-02; 35 → CA-SYS-23; 36 → CA-CGW-14; 37 → CA-CGW-05, CA-SYS-42; 38 → CA-SYS-24; 39 → CA-APP-03; 40 → CA-APP-09, CA-SYS-41; 41 → CA-SYS-28; 42 → CA-SYS-31; 43 → CA-SYS-36; 44 → CA-SYS-32; 45 → CA-SYS-38; 46 → CA-SYS-38, CA-CGW-25; 47 → CA-DCU-09; 48 → CA-GOV-04; 49 → CA-CGW-11; 50 → CA-DCU-20, CA-CGW-10; 51 → CA-CGW-15, CA-GOV-13; 52 → CA-GOV-02; 53 → CA-HIL-06, CA-GOV-07 |
| L1 … L5 | L1 → CA-CGW-02 (option B); L2 → CA-CGW-25 (option C); L3 → CA-APP-09, CA-CGW-03, CA-SYS-41 (gate, fallback 1); L4 → CA-SYS-28 (two controls); L5 → CA-DCU-05 (clear only after 10 03) |
| CAN X-01 … X-18 | 01 → CA-SYS-11; 02 → CA-SYS-09, CA-SYS-10; 03 → CA-SYS-17; 04 → CA-SYS-19; 05 → CA-SYS-20; 06 → CA-DCU-18; 07 → CA-SYS-15; 08 → CA-SYS-16; 09 → CA-DCU-19; 10 → CA-SYS-13; 11 → CA-SYS-12; 12 → CA-SYS-14; 13 → CA-SYS-18; 14 → CA-DCU-01; 15 → CA-SYS-03; 16 → CA-SYS-19; 17 → CA-CGW-01; 18 → CA-CGW-26 |
| PROTO X-01 … X-22 | 01 → CA-CGW-19; 02 → CA-CGW-14; 03 → CA-CGW-20; 04 → CA-CGW-02; 05 → CA-GOV-09; 06 → CA-CGW-21, CA-APP-08; 07 → CA-CGW-07, CA-APP-07; 08 → CA-CGW-03; 09 → CA-CGW-06; 10 → CA-APP-05; 11 → CA-APP-06, CA-SYS-24; 12 → design level (§10); 13 → CA-SYS-06; 14 → design level (§10); 15 → CA-CGW-17; 16 → CA-CGW-22; 17 → CA-SYS-23; 18 → CA-CGW-23, CA-APP-06; 19 → design level (§10); 20 → CA-CGW-24; 21 → CA-SYS-27; 22 → CA-SYS-23 |
| HIL X-01 … X-37 | 01 → CA-CGW-25; 02 → CA-CGW-05; 03 → CA-CGW-14; 04 → CA-SYS-17; 05 → CA-HIL-07; 06 → design level; 07 → CA-SYS-19; 08, 09 → CA-DCU-07; 10 → CA-HIL-03; 11 → CA-SYS-36; 12 → CA-SYS-01; 13 → CA-DCU-20; 14 → CA-SYS-27; 15 → CA-SYS-39; 16 → CA-SYS-40; 17 → CA-SYS-35; 18 → CA-SYS-34; 19 → design level; 20 → CA-SYS-21; 21 → CA-SYS-27; 22, 23 → design level; 24 → CA-HIL-02; 25, 26 → design level (LATER hardware); 27 → CA-SYS-45; 28 → CA-SYS-44; 29 → CA-SYS-28; 30 → CA-HIL-02; 31 → CA-GOV-09; 32 → CA-HIL-08; 34 → CA-SYS-32; 35 → CA-SYS-27; 36 → design level; 37 → CA-GOV-04 |
| GOV X-01 … X-37 | 01 → CA-CGW-02; 04, 28 → CA-GOV-04; 07 → CA-GOV-09; 11 → CA-DCU-10; 12, 13, 14, 15 → CA-SYS-06; 17 → CA-GOV-02; 18 → CA-GOV-12; 19 → CA-GOV-10; 24 → CA-GOV-08; 26 → CA-GOV-03; all others → design level (§10) |
| Fact-checks | FC-TC #1a, #1b, #2a, #2b → CA-GOV-12; #3 → CA-DCU-09; #6j → CA-CGW-12; #6p → CA-GOV-11; #7a–#7c → CA-CGW-01; #8a, #8b → CA-DCU-14; FC-GOV #2 → CA-CGW-02; FC-GOV #7 → CA-GOV-04; other fact-check items confirm pins and settings without a contract change |
| Visual State study | DTC_SWC_FSM_ERROR → CA-DCU-21; DID FD09 → CA-DCU-25; modelling scope → CA-DCU-23; SWR-053/054 → DCU software requirements (no contract change) |
| Stage A plan and spec | CA-SYS-01, CA-SYS-02, CA-SYS-03, CA-SYS-04, CA-SYS-05, CA-SYS-07, CA-SYS-08, CA-DCU-22, CA-DCU-24, CA-DCU-26, CA-DCU-27, CA-DCU-28 |

---

## 10. Design-level findings without a contract change

These cross-check findings concern node designs, CI or tooling only; they are owned by the respective design documents and are listed here for completeness.

| Finding | Owner | Disposition |
|---|---|---|
| PROTO X-12 (APP CI uses third-party Flutter action; separate app workflow) | CI | Flutter installed from the pinned archive; APP jobs inside `ci.yml` (SPEC §6) |
| PROTO X-14 (one conformance suite for the CGW simulator and the real CGW) | HIL, tools | HIL and simulator design |
| PROTO X-19 (vector coverage: handshake frames, StatusUpdate, nanopb fixed-length behaviour, hex labels) | Interfaces | `interfaces/vectors/app_session_v1.json`; hex labels given in LS-SAIC §8.3 |
| HIL X-06, HIL X-19, HIL X-36; GOV X-02, X-03, X-05, X-06, X-08, X-09, X-10, X-16, X-20, X-21, X-22, X-23, X-25, X-27, X-29, X-30, X-31, X-32, X-33, X-34, X-35, X-36, X-37 | CI, process, tooling | Resolved by SPEC §3–§8 (single pin file, uv workspace, unified tree, workflow and check names, runner labels, triggers) and the process documents |
| HIL X-22, HIL X-23, HIL X-25, HIL X-26 | HIL | Stimulus implementation detail, logic-analyser staleness gate, relay and contactor hardware (LATER with the interface board) |

---

## 11. References

- LS-SAIC-001 v0.2 (`docs/02_system/LS-SAIC.md`), LS-SRS-001 v0.2 (`docs/02_system/system_requirements.md`)
- LS-SAIC-001 v0.1 (superseded draft)
