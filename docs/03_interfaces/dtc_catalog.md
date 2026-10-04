# LockSys DTC and Notice Catalogue

| Field | Value |
|---|---|
| Document ID | LS-IF-004 |
| Version | 0.1 |
| Status | Draft |
| Owner | jlurg |
| Source of truth | `interfaces/dtc/dtc_catalog.yaml` |
| Normative text | LS-SAIC-001 §6.3, §8.9, §13 ([LS-SAIC](../02_system/LS-SAIC.md)) |

## 1. Purpose and scope

This document renders the diagnostic trouble code (DTC) and notice catalogue of LockSys: the encoding, the status byte and the storage policy, every DCU and CGW DTC with its detection, reaction, severity, function inhibit and healing condition, the non-DTC notice codes, and the readout paths.

- `interfaces/dtc/dtc_catalog.yaml` is the single source of truth. It generates the DCU event table (`firmware/dcu/gen/`), the CGW tables and the Dart and Python constants. LS-SAIC-001 §13 holds the same content as normative text. This document is informative and is corrected when it differs from them.
- Scope: DTCs of the DCU (B1A, U1A) and the CGW (B1B, U1B), and the notice codes sent to the APP.

## 2. Encoding

- A DTC is 3 bytes: the SAE J2012 two-byte code followed by the failure type byte (FTB).
- In the two-byte code, bits 15–14 encode the letter (P = 00, C = 01, B = 10, U = 11), bits 13–12 the first digit, and bits 11–8, 7–4 and 3–0 the remaining hexadecimal digits.
- Examples: B1A11-71 = 0x9A1171; U1B00-87 = 0xDB0087.
- DCU codes use B1Axx and U1Axx; CGW codes use B1Bxx and U1Bxx.
- FTB values follow common SAE J2012-DA usage. The standard is not available to the project, so FTB values are UNVERIFIED (open point O9).

## 3. Status, storage and aging

| Item | Rule |
|---|---|
| Status byte (ISO 14229-1) | Bit 0 testFailed, bit 2 pendingDTC, bit 3 confirmedDTC, bit 5 testFailedSinceLastClear; availability mask 0x2D |
| Confirmation | A DTC is confirmed on its first qualified failure unless stated otherwise |
| Aging | A confirmed DTC is cleared after `n_dtc_aging_cycles` (40) operation cycles without testFailed; an operation cycle runs from start-up to the next reset |
| DCU fault memory | `n_dtc_entries` (16) entries in `noinit` RAM, protected by CRC-32; survives software and watchdog resets; lost at a power-on reset. Persistence in NvM and snapshot records are LATER. |
| CGW fault memory | RAM and `RTC_NOINIT` counters; persistence LATER |
| Clearing | UDS 0x14 with group 0xFFFFFF clears all DCU DTCs and the fault memory |

## 4. Severity, mode and function inhibit

| Severity | Effect on NodeMode |
|---|---|
| INFO | None |
| WARNING | None; may carry a function inhibit |
| DEGRADED | DEGRADED while the DTC is active |
| CRITICAL | SAFE (latched) |

Function inhibits are defined per DTC, not per fault class. Each DTC has an inhibit target and a release condition:

| Target | Inhibited function |
|---|---|
| `none` | Nothing |
| `window` | Window starts in both directions |
| `window_dir` | Window starts in the direction of the failure |
| `lock` | Lock actuation |
| `window_lock` | Window starts and lock actuation |
| `SAFE` | Everything (CRITICAL severity) |

| Release | The inhibit ends … |
|---|---|
| `heal` | when the DTC heals |
| `timeout(T)` | T after the failure |
| `test_after(T)` | after the failing condition has been absent for T: one test actuation is allowed; success heals the DTC, failure re-arms the inhibit |
| `clear` | only by UDS 0x14 or a power-on reset |

The DCU ModeMgr computes the effective inhibit mask from the active DTCs and the mode; `DcuSts_WinInhibit` and `DcuSts_LockInhibit` report it on CAN and StatusUpdate reports it as `window_inhibited` and `door_inhibited`.

## 5. DCU DTCs

Section references point to LS-SAIC-001.

| Code | Value | Name | Stage | Detection and qualification | Reaction | Severity | Inhibit (target, release) | Healing |
|---|---|---|---|---|---|---|---|---|
| B1A10-96 | 0x9A1096 | Window bridge fault | A | Window EN/DIAG low, or bridge output readback mismatch; immediate | Reflex Off, latch, DRIVER_FAULT; counts toward `n_driver_fault_safe` | DEGRADED | window, test_after(`t_diag_heal_ms`) | Test drive without fault |
| B1A11-71 | 0x9A1171 | Window motion not detected (NO_MOTION: stall, open motor circuit or encoder failure) | A | LS-SAIC §5.3 | Brake; STALL; BLOCKED; press latched | WARNING | none | Next press passes the supervision |
| B1A12-64 | 0x9A1264 | End-position plausibility | D | Both end positions active ≥ `t_bothlim_ms`, or an active end position not cleared within `t_lim_leave_ms` | Off; SAFE | CRITICAL | SAFE | Power-on reset |
| B1A13-92 | 0x9A1392 | Maximum run time exceeded | A | Driving ≥ `t_win_max_run_ms` within one press | Brake; MAX_RUNTIME; press latched | A: WARNING; B, D: DEGRADED | A: none; B, D: window_dir until an end position is reached | A: next press; B, D: end position reached |
| B1A14-13 | 0x9A1413 | Window motor open circuit (current-based) | LATER | Requires CS calibration; stage A reports an open circuit as B1A11 | — | DEGRADED | window, test_after(5000 ms) | Drive with current |
| B1A15-64 | 0x9A1564 | Window encoder direction mismatch (DIR_MISMATCH) | A | LS-SAIC §5.3 | Brake; DIR_MISMATCH; WindowState FAULT | DEGRADED | window, clear | UDS 0x14 or power-on reset |
| B1A16-19 | 0x9A1619 | Window over-current backstop | A | Filtered CS > `i_oc_backstop_ma` for `t_oc_backstop_ms`, from `t_cs_blank_ms` after drive-on | Brake; OVERCURRENT | WARNING | window, timeout(`t_win_oc_inhibit_ms`) | Next press without over-current |
| B1A20-71 | 0x9A2071 | Lock: no feedback | A | Switch ≠ target after `n_lock_max_attempts` | FAILED_ACTUATOR; LockState FAULT | DEGRADED | lock, timeout(`t_lock_fault_inhibit_ms`) | Next transaction OK |
| B1A21-96 | 0x9A2196 | Lock bridge fault | A | Lock EN/DIAG low, or over-current before `t_lock_min_stroke_ms` | Off; FAILED_ACTUATOR; counts toward `n_driver_fault_safe` | DEGRADED | lock, test_after(`t_diag_heal_ms`) | Test actuation without fault |
| B1A22-98 | 0x9A2298 | Lock rate limit reached | A | Request beyond `n_lock_rate_max` actuations within `t_lock_rate_window_ms` | REJECTED_RATE_LIMIT | WARNING | lock, heal | A rate slot is free |
| B1A30-96 | 0x9A3096 | Temperature sensor communication or identity | A | `n_temp_fail` consecutive failed samples, or ID ≠ 0x0117 | SENSOR_FAULT; raw ID logged | DEGRADED | none (the VNH5019 thermal shutdown remains the backstop) | `n_temp_heal` valid samples |
| B1A31-64 | 0x9A3164 | Temperature implausible | A | Outside the valid range, or gradient > `temp_grad_max_cdeg_s` | OUT_OF_RANGE or IMPLAUSIBLE | WARNING | none | `n_temp_heal` valid samples |
| B1A32-98 | 0x9A3298 | ECU over-temperature | A | `n_overtemp_confirm` samples above `temp_inhibit_cdeg` | OVERTEMP stop; starts rejected | DEGRADED | window, heal | Below `temp_release_cdeg` |
| B1A40-16 | 0x9A4016 | KL30 below threshold, or VDDA implausible | A (sense fitted) | < `vbat_uv_dv` for `t_vbat_uv_ms`; VDDA outside `vdda_plaus_min_mv`…`vdda_plaus_max_mv` | UNDERVOLTAGE stop | DEGRADED | window_lock, heal | Inside the start window for `t_vbat_heal_ms` |
| B1A40-17 | 0x9A4017 | KL30 above threshold | A (sense fitted) | > `vbat_ov_dv`, or ADC at full scale, for `t_vbat_ov_ms` | OVERVOLTAGE stop | DEGRADED | window_lock, heal | Inside the start window for `t_vbat_heal_ms` |
| B1A50-45 | 0x9A5045 | ROM CRC failure | A | Start-up or background mismatch | SAFE | CRITICAL | SAFE | None |
| B1A51-44 | 0x9A5144 | Stack or RAM integrity | A | Stack canary corrupted | SAFE | CRITICAL | SAFE | None |
| B1A52-47 | 0x9A5247 | Watchdog reset | A | ResetReason WATCHDOG at start-up | Logged; counts toward `n_wdt_reset_safe` | WARNING (CRITICAL at the reset limit) | none | Aging |
| B1A53-96 | 0x9A5396 | HSE or CSS failure | A | HSE not ready at start-up, or CSS NMI | HSI64 profile, SAFE, CAN silent | CRITICAL | SAFE | Power-on reset |
| B1A54-48 | 0x9A5448 | Software exception (HardFault, BusFault, UsageFault) | A | Fault record found in noinit RAM after reset | Logged with PC and CFSR (DID 0xFD05); counts toward `n_wdt_reset_safe` | WARNING (CRITICAL at the reset limit) | none | Aging |
| B1A55-48 | 0x9A5548 | State-machine engine error (DTC_SWC_FSM_ERROR) | A | A Visual State engine call returns an error code (contradiction, range error, signal queue full) | Affected actuator braked or Off without the engine; FAULT; SAFE | CRITICAL | SAFE | None |
| U1A00-87 | 0xDA0087 | Lost communication with the CGW | A | CGW_NodeSts missing > 500 ms, or CGW_WinCmd missing > `t_wincmd_lost_dtc_ms`; confirmed after `t_comm_dtc_confirm_ms` | DEGRADED | DEGRADED | window, heal | Both frames VALID for `t_comm_heal_ms` |
| U1A01-88 | 0xDA0188 | CAN bus-off | A | Bus-off state | Window brake then Off, lock Off; recovery per LS-SAIC §7.6 | DEGRADED | none (no valid command during bus-off; re-arm afterwards) | `t_busoff_heal_ms` without errors |
| U1A02-82 | 0xDA0282 | E2E alive-counter errors | A | `n_e2e_err_invalid` consecutive sequence or repetition errors | Frame INVALID; window STOP (E2E_ERROR) | WARNING | none | VALID again |
| U1A02-83 | 0xDA0283 | E2E CRC or DataID errors | A | `n_e2e_err_invalid` consecutive CRC or DLC errors | Frame INVALID; window STOP (E2E_ERROR) | WARNING | none | VALID again |
| U1A03-56 | 0xDA0356 | CAN matrix version mismatch | A | `n_ver_debounce` consecutive CGW_NodeSts with a different major | REJECTED_VERSION | DEGRADED | window_lock, heal | Matching major for `t_comm_heal_ms` |

## 6. CGW DTCs

CGW DTCs send a Notice carrying their 24-bit value on every change of their testFailed bit.

| Code | Value | Name | Detection and qualification | Reaction | Severity | Healing |
|---|---|---|---|---|---|---|
| U1B00-87 | 0xDB0087 | Lost communication with the DCU | DCU_NodeSts missing > 500 ms; confirmed after `t_comm_dtc_confirm_ms` | STOP latch; commands FAILED_COMM | DEGRADED | DCU_NodeSts VALID for `t_comm_heal_ms` |
| U1B01-88 | 0xDB0188 | CAN bus-off | Bus-off state | STOP latch, slot rewrite, recovery per LS-SAIC §7.6 | DEGRADED | `t_busoff_heal_ms` without errors |
| U1B02-82 | 0xDB0282 | E2E alive-counter errors | 3 consecutive sequence or repetition errors | Frame data invalid | WARNING | VALID again |
| U1B02-83 | 0xDB0283 | E2E CRC or DataID errors | 3 consecutive CRC or DLC errors | Frame data invalid | WARNING | VALID again |
| U1B03-56 | 0xDB0356 | CAN matrix version mismatch | `n_ver_debounce` consecutive DCU_NodeSts with a different major | REJECTED_VERSION | DEGRADED | Matching major for `t_comm_heal_ms` |
| B1B10-96 | 0x9B1096 | SoftAP start failure | No AP start within `t_ap_start_to_ms` after 3 attempts | No APP link | DEGRADED | AP started |
| B1B11-96 | 0x9B1196 | TWAI initialisation or driver fault | Driver error | SAFE | CRITICAL | Reset |
| B1B12-47 | 0x9B1247 | Watchdog reset (task or interrupt watchdog) | Reset reason | Logged; `n_wdt_reset_safe` within `t_wdt_reset_window_ms` → SAFE | WARNING (CRITICAL at the reset limit) | Aging |
| B1B13-16 | 0x9B1316 | Brown-out reset | Reset reason | Logged | WARNING | Aging |
| B1B14-47 | 0x9B1447 | Panic reset | Reset reason, core dump present | Logged; counts toward the reset limit | WARNING (CRITICAL at the reset limit) | Aging |
| B1B15-44 | 0x9B1544 | NVS or key store fault (erase performed) | NVS error | Unpaired; sessions closed with 4005 | DEGRADED | New pairing |
| B1B16-96 | 0x9B1696 | Cryptographic self-test failure | RFC 4231 / RFC 5869 known-answer tests at start-up | SAFE | CRITICAL | Reset |

## 7. Notices

`Notice.code` values ≥ 0x010000 are 24-bit DTC values (§6). Codes below 0x010000 are non-DTC notices, sent by the CGW to the APP:

| Code | Name | Severity | Meaning |
|---|---|---|---|
| 0x0001 | WINDOW_STOP_KA_TIMEOUT | WARNING | The keep-alive timeout latched the press |
| 0x0002 | WINDOW_STOP_LINK_QUALITY | WARNING | The RTT gate or a missing Pong latched the press |
| 0x0003 | WINDOW_STOP_DIR_CHANGE | WARNING | Direction change within a press |
| 0x0004 | WINDOW_STOP_COMM | WARNING | Bus-off or DCU lost during a press |
| 0x0005 | WINDOW_STOP_MODE | WARNING | CGW or DCU mode change during a press |
| 0x0006 | WINDOW_STOP_BACKSTOP | WARNING | CGW backstop `t_cgw_max_run_backstop_ms` reached |
| 0x0007 | WINDOW_STOP_DCU | INFO | DCU rejected or stopped the press; the reason is in `window_stop_reason` |
| 0x0008 | WINDOW_STOP_SESSION | INFO | Session loss, pre-emption or station disconnect latched the press (logged only) |
| 0x0010 | PAIRING_WINDOW_OPEN | INFO | Pairing window opened |
| 0x0011 | PAIRING_WINDOW_CLOSED | INFO | Pairing window closed without a new pairing |
| 0x0012 | PAIRING_COMMITTED | INFO | A new pairing was committed |

- `Notice.text` is at most 48 characters of English and never contains secrets.
- The APP never stops a press on a Notice alone; the stop triggers are defined in LS-SAIC-001 §8.7.

## 8. Readout

| Path | Content | Defined in |
|---|---|---|
| UDS 0x19 0x02 (DCU) | DTCs matching a status mask, with status bytes | [LS-IF-001](can_matrix.md) §10 |
| UDS 0x14 (DCU) | Clear all DTCs | [LS-IF-001](can_matrix.md) §10 |
| `$LSDTC` (DCU UART) | One line on every testFailed change; one line per stored entry after boot | [LS-IF-003](uart_telemetry.md) |
| `DcuSts_DtcCount`, `CgwSts_DtcCount` | Number of DTCs with confirmedDTC set | [LS-IF-001](can_matrix.md) §6 |
| `Notice` (CGW → APP) | CGW DTC changes and non-DTC notices | [LS-IF-002](app_protocol.md) §9 |
| StatusUpdate `dcu_dtc_count`, `cgw_dtc_count` | Confirmed DTC counts | [LS-IF-002](app_protocol.md) §8 |

## 9. Generated artefacts

The output paths are defined in the code generation manifest `interfaces/can/codegen.yaml`.

| Consumer | Generated content | Location |
|---|---|---|
| DCU and CGW | DTC values and notice codes | `libs/ls_common/gen/ls_dtc_gen.h` |
| DCU Dem | Event table: code, severity, inhibit target and release, qualification parameters | `firmware/dcu/gen/dem_dtc_gen.{h,c}` |
| APP | DTC and notice constants for messages and the Diagnostics screen | `app/packages/locksys_protocol/lib/src/gen/ls_dtc.dart` |
| HIL | Expected DTC values and notice codes | `hil/src/locksys_hil/gen/dtc.py` |

`uv run tools/codegen/regen.py` regenerates all outputs; `--check` fails on drift.

## 10. Rationale

- **Per-DTC inhibit.** Inhibits per fault class blocked the very actuation that some DTCs need in order to heal (for example a window test drive after a driver fault).
- **No NvM in the MVP.** DTCs survive software and watchdog resets in `noinit` RAM, which covers the fault cases that reset the DCU, without the flash-erase timing risks.
- **CGW DTCs as notices.** The APP shows CGW faults without a diagnostic protocol on the phone.

## 11. References

- [LS-SAIC-001](../02_system/LS-SAIC.md) §6.1–§6.3, §8.9, §13
- [LS-SRS-001](../02_system/system_requirements.md): SYS-060, SYS-061
- [LS-IF-001 CAN matrix](can_matrix.md), [LS-IF-002 APP protocol](app_protocol.md), [LS-IF-003 UART telemetry](uart_telemetry.md)
- ISO 14229-1 (UDS); SAE J2012-DA (not available to the project)
