# LockSys DCU Software Requirements

| Field | Value |
|---|---|
| Document ID | LS-DCU-SRS-001 |
| Version | 0.1 |
| Status | Draft |
| Owner | jlurg |
| Parents | LS-SRS-001 ([system requirements](../../02_system/system_requirements.md)), LS-SAF-001 safety mechanisms |
| Architecture | LS-DCU-SAD-001 ([DCU software architecture](architecture.md)) |

## 1. Purpose and scope

This document specifies the software requirements of the DCU firmware (`SWR-DCU-nnn`), derived from the system requirements allocated to the DCU (allocation D in LS-SRS-001) and from the safety mechanisms allocated to the DCU in LS-SAF-001. Stage A requirements form the MVP; requirements of later stages are listed so that interfaces and identifiers stay stable.

Parameters in backticks are keys of `interfaces/params/timing.yaml` (LS-SAIC-001 §5.0); their values are not repeated here. Frame, signal, DID and DTC names refer to [LS-IF-001](../../03_interfaces/can_matrix.md), [LS-IF-003](../../03_interfaces/uart_telemetry.md) and [LS-IF-004](../../03_interfaces/dtc_catalog.md).

## 2. Conventions

| Column | Meaning |
|---|---|
| ID | `SWR-DCU-nnn`; numbers are grouped by area and never reused |
| Requirement | "The DCU shall …"; normative |
| Parents | SYS requirements; SM entries where the requirement implements a safety mechanism; CSR entries for security |
| Ver | UT unit test; IT integration test; HIL hardware-in-the-loop test; M manual bench procedure; A analysis; R review or inspection |
| Stage | A (release v1.0), B, D or E; LATER for items outside the window stages |
| Tag | [SAF] safety-related; [SEC] security-related |
| MS | Milestone in which the requirement is implemented and verified |

- Implementations carry `/* @satisfies SWR-DCU-nnn */`; unit tests carry `/* @verifies SWR-DCU-nnn */`; HIL tests reference the parent SYS requirement and the SWR where applicable.
- Every stage A requirement is verified by a unit test where the behaviour is testable on the host (LS-VER-001 §4).

## 3. Requirements

### 3.1 Window motion

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-001 | The DCU shall drive the window motor only in the direction requested by a CGW_WinCmd frame processed with E2E state VALID, and only while every start and run condition of LS-SAIC-001 §5.2 holds. | SYS-005, SYS-037; SM-03 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-002 | The DCU shall start a window drive only from the idle state after the dead time, and shall refuse a start that violates any of the start conditions S1–S4 or S6–S11 of LS-SAIC-001 §5.2, reporting the specified `WinSts_WinResult` and `WinSts_StopReason`. | SYS-005, SYS-035, SYS-039, SYS-040 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-003 | The DCU shall evaluate every run condition of LS-SAIC-001 §5.2 every `t_dcu_task_ms` while the window moves and, when one fails, shall stop with the StopReason of highest priority in the order of LS-SAIC-001 §5.2. | SYS-020, SYS-021, SYS-022 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-004 | The DCU shall treat a missing CGW_WinCmd for the RX timeout of the CAN matrix (100 ms) as STOP with StopReason CAN_TIMEOUT. | SYS-022; SM-03 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-005 | After any CGW_WinCmd INVALID state, CGW_WinCmd RX timeout or DCU reset, the DCU shall refuse window starts until at least one VALID CGW_WinCmd with Req = STOP has been received. | SYS-035, SYS-037; SM-04 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-006 | The DCU shall refuse a start with FAILED_TIMEOUT, and stop a running motor with StopReason HOLD_TIMEOUT, when `WinCmd_HoldAge` exceeds `t_holdage_max_ms` or is raw 255. | SYS-021; SM-03 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-007 | The DCU shall ignore PressId 0 and the latched PressId, shall latch the PressId of every stopped press, refused start and refused drive, and shall initialise the latch after reset with the first PressId received with E2E OK. | SYS-035; SM-05 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-008 | The DCU shall stop a running window by braking for `t_win_brake_ms` and then switching the bridge off, and shall keep the bridge off for at least `t_win_rev_dead_ms` before any new drive, in either direction. | SYS-033; SM-08 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-009 | The DCU shall switch a bridge off without braking on a VNH5019 driver fault, a hang-monitor trip, a fault exception and SAFE entry. | SYS-023, SYS-038; SM-07, SM-11 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-010 | The DCU shall stop a press after `t_win_max_run_ms` of driving with StopReason MAX_RUNTIME, latch its PressId and set B1A13; in stage A the stop shall inhibit no direction, so the next re-armed press may drive in either direction. | SYS-030; SM-08 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-011 | The DCU shall drive the window PWM at `win_pwm_freq_hz` and ramp the duty linearly from 0 to `win_duty_run_pct` within `t_softstart_ms` at every start. | SYS-005 | UT, M | A | — | M2 |
| SWR-DCU-012 | The DCU shall transmit DCU_WinSts every 50 ms and on change with a minimum gap of 10 ms, with WindowState mapped as defined in LS-SAIC-001 §5.2, the StopReason of the last stop or refused start, PressIdEcho, WinResult and the fault flags; `WinSts_PosPct` shall be 255 in stage A. | SYS-007, SYS-026 | UT, HIL | A | — | M2 |
| SWR-DCU-013 | The DCU shall transmit DCU_WinMotion every 50 ms with EncoderStatus, output-shaft speed, commanded duty and relative position. | SYS-007, SYS-026 | UT, HIL | A | — | M2 |
| SWR-DCU-014 | The DCU shall pass every window bridge command through an output permission gate that is independent of the window state machine: a drive is applied only for a VALID, re-armed request with the same direction and the active, unlatched PressId, with no reflex latch for that direction, with the mode permitting motion and with the continuous drive time below `t_win_max_run_ms` plus `t_dcu_task_ms`; a refused drive is replaced by brake; brake and off are never refused. | SYS-005, SYS-030, SYS-035; SM-19 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-015 | The DCU shall make reflex stops non-overridable: a bridge command shall be checked against the reflex latch and written atomically with respect to the reflex handlers, and a latch cause shall be released only under the release condition of its DTC. | SYS-031, SYS-042; SM-07, SM-18 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-016 | With `act_serialise`, the DCU shall refuse a window start with REJECTED_INTERLOCK while a lock pulse is in progress, and a lock request with REJECTED_INTERLOCK while the window bridge drives. | SYS-003, SYS-005 | UT, HIL | A | — | M2 |

### 3.2 Window supervision

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-020 | The DCU shall sample the encoder counter every `t_dcu_task_ms` using 16-bit modular differences, apply `enc_dir_invert`, accumulate a 32-bit relative position (positive = UP) and compute the output-shaft speed over 50 ms in 0.1 rpm from `enc_cpr`. | SYS-042, SYS-007 | UT | A | [SAF] | M2 |
| SWR-DCU-021 | While the window bridge drives with a duty of at least `no_motion_duty_min_pct`, from `t_start_grace_ms` after drive-on, the DCU shall detect NO_MOTION when the counts in the commanded direction over the sliding `no_motion_window_ms`, evaluated every `t_dcu_task_ms`, are below `no_motion_min_pct` % of the expected counts, and shall brake within one task period with StopReason STALL, WindowState BLOCKED, `WinSts_FltStall`, EncoderStatus NO_MOTION and B1A11. | SYS-042; SM-18 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-022 | The DCU shall detect DIR_MISMATCH, evaluated before NO_MOTION, when the net counts over `t_dir_mismatch_ms` oppose the commanded direction by at least `dir_mismatch_min_counts`, and shall brake with StopReason DIR_MISMATCH, set WindowState FAULT, `WinSts_FltDirMismatch`, EncoderStatus DIR_MISMATCH and B1A15, and inhibit the window until UDS 0x14 or a power-on reset. | SYS-042; SM-18 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-023 | The DCU shall sample the window current sense at 1 kHz and, when the filtered current exceeds `i_oc_backstop_ma` continuously for `t_oc_backstop_ms`, evaluated from `t_cs_blank_ms` after drive-on, shall brake with StopReason OVERCURRENT, set B1A16 and inhibit window starts for `t_win_oc_inhibit_ms`. | SYS-031; SM-07 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-024 | On a falling edge of a VNH5019 EN/DIAG line the DCU shall switch that bridge off within 1 ms from the interrupt, set the reflex latch and B1A10 (window) or B1A21 (lock), never drive EN/DIAG, allow one test actuation after the line has been high for `t_diag_heal_ms`, and enter SAFE after `n_driver_fault_safe` driver faults within `t_driver_fault_window_ms`. | SYS-031; SM-07 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-025 | The DCU shall report the filtered window current in `WinSts_Current` with 0.1 A resolution and raw 255 when the measurement is invalid. | SYS-007, SYS-031 | UT, HIL | A | — | M2 |
| SWR-DCU-026 | The DCU shall read back the INA and INB outputs of both bridges every 1 ms and, on a mismatch with the commanded level, switch the bridge off, set the reflex latch and B1A10 or B1A21. | SYS-005; SM-15 | UT, HIL | A | [SAF] | M2 |

### 3.3 Door lock

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-030 | The DCU shall treat a CGW_DoorCmd frame with E2E OK whose ReqId is not 0 and differs from `DoorSts_LastReqId` as a new request, and ignore repetitions of the current `LastReqId`. | SYS-027, SYS-003 | UT, HIL | A | — | M2 |
| SWR-DCU-031 | For a new request the DCU shall, in one DCU_DoorSts update sent at once, set `LastReqId` to the ReqId and `LastResult` to REJECTED_INVALID, REJECTED_MODE, REJECTED_VERSION, REJECTED_INTERLOCK or REJECTED_RATE_LIMIT in the order of LS-SAIC-001 §5.1, otherwise to OK without a pulse when the switch already shows the requested state, otherwise to ACCEPTED with LockState LOCKING or UNLOCKING. | SYS-004, SYS-024, SYS-027 | UT, HIL | A | — | M2 |
| SWR-DCU-032 | For an accepted request the DCU shall pulse the lock at 100 % for `t_lock_pulse_ms` in the requested direction, wait `t_lock_settle_ms` plus `t_debounce_ms`, compare the switch with the target and, on a mismatch, pause `t_lock_retry_pause_ms` and pulse again up to `n_lock_max_attempts`; the final result shall be OK, or FAILED_ACTUATOR with LockState FAULT and B1A20. | SYS-003, SYS-024 | UT, HIL | A | — | M2 |
| SWR-DCU-033 | The DCU shall end every lock pulse at `t_lock_pulse_hard_max_ms` by a mechanism independent of the door state machine, and shall treat lock over-current above `i_oc_lock_ma` for `t_oc_lock_ms` before `t_lock_min_stroke_ms` as a fault (FAILED_ACTUATOR, B1A21) and after it as the end of stroke. | SYS-034; SM-09 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-034 | The DCU shall reject a request with REJECTED_RATE_LIMIT and set B1A22 when `n_lock_rate_max` requests lie inside `t_lock_rate_window_ms`, and shall report `DoorSts_RateLimited` while the limit is reached. | SYS-034; SM-09 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-035 | While a transaction executes the DCU shall drop and log new ReqIds without changing `LastReqId`, so that a ReqId causes at most one actuation sequence; the DCU shall never report REJECTED_BUSY. | SYS-027 | UT, HIL | A | — | M2 |
| SWR-DCU-036 | The DCU shall derive DoorLockState only from the debounced position switch and `lock_fb_locked_level` (FAULT while B1A20 is active; LOCKING or UNLOCKING during execution), never from the command, and transmit DCU_DoorSts every 100 ms and on change with a minimum gap of 10 ms. | SYS-001, SYS-026 | UT, HIL | A | — | M2 |
| SWR-DCU-037 | The DCU shall end a transaction with FAILED_ACTUATOR when EN/DIAG of the lock channel goes low (B1A21) or the supply falls below `vbat_uv_dv` (B1A40) during a pulse. | SYS-003, SYS-034 | UT, HIL | A | — | M2 |

### 3.4 Temperature

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-040 | The DCU shall trigger a TMP117 one-shot conversion (configuration 0x0C20, written explicitly) every `t_temp_period_ms`, first access not earlier than `t_tmp117_boot_ms` after power-up, poll Data_Ready every 5 ms from `t_temp_drdy_first_ms` and count a sample as failed when Data_Ready is not set by `t_temp_drdy_to_ms`. | SYS-008 | UT, HIL | A | — | M2 |
| SWR-DCU-041 | The DCU shall access the TMP117 with an interrupt-driven I2C driver at the highest interrupt priority, use a pointer write, STOP and a new START for register reads (no repeated START), apply the ES096 §2.8 workarounds, time out a job after 5 ms, and recover a stuck bus without blocking: up to 9 SCL pulses and STOP, SCL held low for at least 45 ms, the ES096 §2.8.7 sequence, and a full re-initialisation after any peripheral reset. | SYS-008 | UT, HIL | A | — | M2 |
| SWR-DCU-042 | The DCU shall convert the TMP117 result to cdeg as sign(raw) × ⌊(\|raw\| × 78125 + 50000) / 100000⌋ and treat raw 0x8000 as a failed sample. | SYS-008, SYS-010 | UT | A | — | M2 |
| SWR-DCU-043 | The DCU shall check the TMP117 identity (0x0117 on 16 bits) at start-up and every `t_temp_id_check_ms`, report SENSOR_FAULT and B1A30 after `n_temp_fail` consecutive failed samples or an identity mismatch, OUT_OF_RANGE or IMPLAUSIBLE and B1A31 for values outside `temp_valid_min_cdeg`…`temp_valid_max_cdeg` or gradients above `temp_grad_max_cdeg_s`, and heal after `n_temp_heal` valid samples. | SYS-008, SYS-061 | UT, HIL | A | — | M2 |
| SWR-DCU-044 | After `n_overtemp_confirm` samples above `temp_inhibit_cdeg` the DCU shall set B1A32, refuse window starts with REJECTED_INTERLOCK and stop a running window with StopReason OVERTEMP until the temperature falls below `temp_release_cdeg`. | SYS-040; SM-10 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-045 | The DCU shall emit `$LSTMP` within 10 ms after a sample completes and transmit DCU_TempSts right after each sample with `TempSts_SampleSeq` equal to the `$LSTMP` sequence number modulo 256; the DCU shall never report TempStatus STALE. | SYS-009, SYS-010 | UT, HIL | A | — | M2 |

### 3.5 Supply

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-050 | With `kl30_sense_fitted`, the DCU shall measure KL30 from PA4 with `kl30_gain_x1000` and `kl30_offset_mv`, use VREFINT only for the VDDA plausibility check, and report KL30 in `DcuSts_Vbat` and DID 0xFD02; without the module it shall report raw 255 and keep the supply interlock inactive. | SYS-043 | UT, M, HIL | A | [SAF] | M2 |
| SWR-DCU-051 | With `kl30_sense_fitted`, the DCU shall allow window and lock starts only with KL30 between `vbat_start_min_dv` and `vbat_start_max_dv`, stop the window with UNDERVOLTAGE when KL30 is below `vbat_uv_dv` for `t_vbat_uv_ms` and with OVERVOLTAGE when it is above `vbat_ov_dv`, or the ADC is at full scale, for `t_vbat_ov_ms`, set B1A40-16 or B1A40-17, and heal after `t_vbat_heal_ms` inside the start window. | SYS-039; SM-10 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-052 | On a PVD event the DCU shall set both bridges off from the interrupt and record the event in `noinit` RAM. | SYS-039; SM-10 | UT, HIL | A | [SAF] | M1 |

### 3.6 Communication

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-060 | The DCU shall run bxCAN at 500 kbit/s with `CAN_BTR = 0x00050008` (sample point 87.5 %), ABOM = 0, TTCM = 0, NART = 0 and TXFP = 0, accept only the identifiers of LS-IF-001 §3.2 with duplicate entries in unused filter slots, and keep the USB peripheral off. | SYS-026 | UT, IT, M | A | — | M1 |
| SWR-DCU-061 | The DCU shall check every received application frame in arrival order with `ls_e2e` (CRC-8/SAE-J1850 over DataID and payload, alive counter with the MaxDelta of the matrix, VALID after `n_e2e_ok_valid` OK frames, INVALID after `n_e2e_err_invalid` errors or an RX timeout), use frame data only when the frame is OK and the state is VALID, set U1A02-82 or U1A02-83, and count errors per message for DID 0xFD08. | SYS-037; SM-04 | UT, HIL | A | [SAF] | M1 |
| SWR-DCU-062 | The DCU shall protect every transmitted application frame with E2E and increment the alive counter once for every frame handed to the CAN controller. | SYS-037, SYS-026; SM-04 | UT | A | [SAF] | M1 |
| SWR-DCU-063 | The DCU shall transmit its cyclic frames with the periods and minimum gaps of the CAN matrix within ± 10 % and send start values until valid content exists. | SYS-026, SYS-062 | UT, HIL | A | — | M1 |
| SWR-DCU-064 | The DCU shall keep the CAN controller in initialisation mode until scheduler start and send the first DCU_NodeSts with NodeMode INIT within 100 ms after reset. | SYS-062, SYS-072; SM-15 | HIL | A | — | M1 |
| SWR-DCU-065 | The DCU shall transmit DCU_NodeSts every 100 ms and DCU_Version every 1000 ms with the content of LS-IF-001 §6, including CAN matrix version 1.0, the reset reason, the confirmed DTC count, the maximum CPU load and the inhibit flags. | SYS-062 | UT, HIL | A | — | M1 |
| SWR-DCU-066 | The DCU shall set U1A03, enter DEGRADED and reject window and door commands with REJECTED_VERSION after `n_ver_debounce` consecutive CGW_NodeSts frames with a different CAN matrix major version, and treat the version as not matching before the first CGW_NodeSts. | SYS-062 | UT, HIL | A | — | M2 |
| SWR-DCU-067 | The DCU shall set U1A00, confirmed after `t_comm_dtc_confirm_ms`, inhibit the window and enter DEGRADED when CGW_NodeSts is missing for more than 500 ms or CGW_WinCmd for more than `t_wincmd_lost_dtc_ms`, and heal after both are VALID for `t_comm_heal_ms`. | SYS-022, SYS-061 | UT, HIL | A | — | M2 |
| SWR-DCU-068 | On bus-off the DCU shall brake and switch off the window, switch off the lock, set U1A01, recover with `n_busoff_fast` attempts every `t_busoff_fast_ms` and then every `t_busoff_slow_ms` by setting and clearing INRQ, reset the attempt counter and heal U1A01 after `t_busoff_heal_ms` without errors, and resume transmission within 1 s after the fault is removed. | SYS-063 | UT, HIL | A | — | M1 |

### 3.7 Modes, start-up and integrity

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-070 | As the first initialisation step, before clock set-up, the DCU shall drive the bridge PWM pins low, set INA = INB = 0 and configure EN/DIAG as floating inputs; it shall write `AFIO->MAPR` once before enabling any timer output and shall not actuate in INIT. | SYS-036; SM-15 | R, HIL | A | [SAF] | M1 |
| SWR-DCU-071 | The DCU shall manage NodeMode as defined in LS-SAIC-001 §6.1: INIT → NORMAL within `t_init_max_ms` when the completion conditions hold, INIT → DEGRADED at the deadline with only non-critical causes, NORMAL ↔ DEGRADED with `t_mode_heal_ms` of healing, and function availability per LS-SAIC-001 §6.3. | SYS-072, SYS-062 | UT, HIL | A | — | M2 |
| SWR-DCU-072 | The DCU shall compute the window and lock inhibits from the active DTCs with their inhibit target and release condition (`heal`, `timeout`, `test_after`, `clear`) and from the mode, and report them in `DcuSts_WinInhibit` and `DcuSts_LockInhibit`. | SYS-039, SYS-040, SYS-042, SYS-061 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-073 | On a critical cause (a CRITICAL DTC, the driver-fault or reset escalation, or a set SAFE latch) the DCU shall switch both bridges off, reject every request with REJECTED_MODE, set the SAFE latch in `noinit` RAM, keep CAN status, UDS and telemetry running except after a clock failure, and leave SAFE only by a power-on reset or by UDS 0x11 0x01 received in the extended session. | SYS-038, SYS-060; SM-11 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-074 | The DCU shall verify the ROM CRC-32 over 0x08000000–0x0801EFFB against the word at 0x0801EFFC at start-up and enter SAFE with B1A50 on a mismatch within `lim_safe_entry_ms`; a background check of the same range shall follow in milestone M6. | SYS-038; SM-12 | UT, HIL | A | [SAF] | M1, M6 |
| SWR-DCU-075 | The DCU shall run from the HSE bypass clock at 72 MHz and, when HSE is not ready within 5 ms at start-up or the clock security system triggers, switch to the HSI64 profile, enter SAFE with the CAN controller silent, set B1A53 and leave SAFE only by a power-on reset. | SYS-038; SM-14 | UT, HIL, M | A | [SAF] | M1, M6 |
| SWR-DCU-076 | The DCU shall start the IWDG (`t_iwdg_nom_ms`) before initialisation steps that can hang, refresh it between initialisation steps of at most 10 ms each, then refresh it only from the watchdog manager when every supervised entity passed its alive check, and never from an interrupt. | SYS-023; SM-11 | UT, HIL | A | [SAF] | M1 |
| SWR-DCU-077 | The DCU shall switch all bridges off from the SysTick interrupt when a task has run longer than `t_hang_detect_ms`, so that outputs are off within 25 ms after a task hang starts. | SYS-023; SM-11 | UT, HIL | A | [SAF] | M1 |
| SWR-DCU-078 | On a HardFault, BusFault or UsageFault the DCU shall switch the outputs to the safe state within 1 µs after handler entry, store PC, LR, xPSR, CFSR, HFSR and BFAR in `noinit` RAM, reset, and report B1A54 at the next start-up. | SYS-023; SM-11 | UT, HIL | A | [SAF] | M1 |
| SWR-DCU-079 | The DCU shall place the stack at the bottom of SRAM, paint it at start-up, check its canary every 100 ms (B1A51 and SAFE on corruption), and report its high-water mark in DID 0xFD06. | SYS-023, SYS-070; SM-13 | UT, HIL | A | [SAF] | M1 |
| SWR-DCU-080 | The DCU shall decode the reset reason with the priority IWDG > WWDG > SFT > LPWR > POR > PIN, never report BROWNOUT, report it in `$LSRST`, `DcuSts_ResetReason` and DID 0xFD05, count watchdog and fault resets per power cycle, and enter SAFE after `n_wdt_reset_safe` of them within `t_wdt_reset_window_ms`. | SYS-061, SYS-023; SM-11 | UT, HIL | A | [SAF] | M1 |
| SWR-DCU-081 | The DCU shall run continuously through the SYS-071 soak without resets, scheduler overruns or DTCs other than those caused by the test. | SYS-071 | HIL | A | — | M5 |

### 3.8 Diagnostics

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-090 | The DCU shall store DTCs as defined in LS-IF-004: status bits 0, 2, 3 and 5, `n_dtc_entries` entries in `noinit` RAM protected by CRC-32 and readable after a software reset, aging after `n_dtc_aging_cycles` operation cycles, `DcuSts_DtcCount`, and `$LSDTC` on every testFailed change. | SYS-061 | UT, HIL | A | — | M2 |
| SWR-DCU-091 | The DCU shall implement ISO 15765-2 on 0x7A0/0x7A8 with normal addressing, padding 0xCC, flow control BS `n_isotp_bs` and STmin `t_isotp_stmin_ms`, N_Bs and N_Cr `t_isotp_n_bs_ms` and `t_isotp_n_cr_ms`, and messages up to `isotp_max_payload_bytes`. | SYS-060 | UT, HIL | A | — | M2 |
| SWR-DCU-092 | The DCU shall provide the UDS-lite services 0x10 (0x01, 0x03), 0x11 0x01, 0x14 0xFFFFFF, 0x19 0x02, 0x22 and 0x3E with the negative response codes, P2, P2* and S3 timing of LS-IF-001 §10.2. | SYS-060 | UT, HIL | A | — | M2 |
| SWR-DCU-093 | The DCU shall provide DIDs 0xF18C, 0xF195 and 0xFD00–0xFD09 with the layouts of LS-IF-001 §10.3. | SYS-060, SYS-070 | UT, HIL | A | — | M2 |
| SWR-DCU-094 | The DCU shall record the identifier of every state-machine transition in an 8-entry trace ring in all builds and in a 64-bit coverage bitmap in DEV and HIL builds, readable through DID 0xFD09. | SYS-060 | UT, HIL | A | — | M2 |

### 3.9 Telemetry and test support

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-100 | The DCU shall emit UART telemetry version 1 as defined in LS-IF-003: grammar, checksum, length limits, sentence fields and rates. | SYS-009, SYS-090 | UT, HIL | A | — | M1 |
| SWR-DCU-101 | The DCU shall send telemetry without blocking through a `tlm_ring_bytes` ring and DMA, drop whole lines on overflow followed by a DROPPED line, limit log lines to `n_log_max_per_s`, compile D-level logs out of Release builds, and add less than 50 µs of jitter to the 1 ms tick. | SYS-090 | UT, HIL | A | — | M1 |
| SWR-DCU-102 | DEV and HIL builds of the DCU shall accept the `!LSFI` commands of LS-IF-003 §7 with ACK and NAK log lines; RC and RELEASE builds shall keep USART2 RX disabled and contain no `Fi_` symbols. | SYS-023, SYS-038; CSR-015 | R, HIL | A | [SEC] | M1, M2 |
| SWR-DCU-103 | DEV and HIL builds of the DCU shall drive TRACE0–TRACE3 as defined in LS-SAIC-001 §3.5 with single BSRR stores; RC and RELEASE builds shall not toggle them. | SYS-020, SYS-023 | R, HIL | A | — | M1 |

### 3.10 Software structure and resources

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-110 | The DCU software shall use static memory only, with no heap, no recursion and no function pointers in authored code. | SYS-070 | R, A | A | — | M1 |
| SWR-DCU-111 | The DCU shall keep the CPU load at or below 60 % in every 1 s window (reported in DID 0xFD07 and `DcuSts_CpuLoadMax`), flash use at or below 75 % of the application region and RAM use at or below 60 %. | SYS-070 | A, HIL | A | — | M1 |
| SWR-DCU-112 | The DCU shall schedule its runnables from a 1 ms SysTick in the task table of LS-DCU-SAD-001 §5.1 and count task overruns. | SYS-026, SYS-090 | UT, HIL | A | — | M1 |
| SWR-DCU-113 | The DCU shall take CAN packing, frame attributes, parameters, enumerations and the DTC table only from code generated from `interfaces/`, without manual changes. | SYS-026, SYS-061 | R | A | — | M1 |
| SWR-DCU-114 | The DCU shall use the WinCtrl, DoorCtrl and ModeMgr engines generated from the committed Visual State model with the committed options only, call each engine only from its adapter in the 10 ms task with exactly one event per call, and check every return code. | SYS-005, SYS-035; SM-19 | R, UT | A | [SAF] | M2 |
| SWR-DCU-115 | On a state-machine engine error the DCU shall brake or switch off the affected actuator without the engine, set B1A55, enter SAFE and not call that engine again until reset. | SYS-005, SYS-038; SM-19 | UT, HIL | A | [SAF] | M2 |
| SWR-DCU-116 | The DCU software shall respect the errata constraints of LS-SAIC-001 §3.3: PB5 never configured as alternate function, no `LDR SP` from memory in assembly, regular ADC conversions only, TTCM = 0, USART2 TE kept set. | SYS-005, SYS-008, SYS-026 | R | A | — | M1 |

### 3.11 Later stages

| ID | Requirement | Parents | Ver | Stage | Tag | MS |
|---|---|---|---|---|---|---|
| SWR-DCU-120 | The DCU shall derive the window position from the encoder position relative to a homed reference and `win_travel_counts`, stop within 10 ms when it reaches 0 % or 100 %, reject motion into an active virtual end position with REJECTED_INTERLOCK, and report `WinSts_LimUp`, `WinSts_LimDn`, `WinSts_PosPct` and FULLY_CLOSED or FULLY_OPEN. | SYS-006, SYS-007; SM-06 | UT, HIL | B | [SAF] | LATER |
| SWR-DCU-121 | The DCU shall stop the window within 5 ms when a normally-closed end-position switch opens, and set B1A12 and enter SAFE when both switches are active for `t_bothlim_ms` or an active switch does not clear within `t_lim_leave_ms`. | SYS-006, SYS-032; SM-06 | UT, HIL | D | [SAF] | LATER |
| SWR-DCU-122 | While closing, the DCU shall detect an obstacle from the encoder speed and the current signature, stop and reverse toward open. | SYS-041 | UT, HIL | E | [SAF] | LATER |

## 4. Traceability

### 4.1 System requirements allocated to the DCU

| SYS | Stage | SWR-DCU |
|---|---|---|
| SYS-001 | A | SWR-DCU-036 |
| SYS-003 | A | SWR-DCU-016, SWR-DCU-030, SWR-DCU-032, SWR-DCU-037 |
| SYS-004 | A | SWR-DCU-031 |
| SYS-005 | A | SWR-DCU-001, SWR-DCU-002, SWR-DCU-011, SWR-DCU-014, SWR-DCU-016, SWR-DCU-026, SWR-DCU-114, SWR-DCU-115, SWR-DCU-116 |
| SYS-006 | B, D | SWR-DCU-120, SWR-DCU-121 |
| SYS-007 | A, B | SWR-DCU-012, SWR-DCU-013, SWR-DCU-020, SWR-DCU-025, SWR-DCU-120 |
| SYS-008 | A | SWR-DCU-040, SWR-DCU-041, SWR-DCU-043, SWR-DCU-116 |
| SYS-009 | A | SWR-DCU-045, SWR-DCU-100 |
| SYS-010 | A | SWR-DCU-045 |
| SYS-020 | A | SWR-DCU-003, SWR-DCU-103 |
| SYS-021 | A | SWR-DCU-003, SWR-DCU-006 |
| SYS-022 | A | SWR-DCU-003, SWR-DCU-004, SWR-DCU-067 |
| SYS-023 | A | SWR-DCU-009, SWR-DCU-076, SWR-DCU-077, SWR-DCU-078, SWR-DCU-079, SWR-DCU-080, SWR-DCU-102, SWR-DCU-103 |
| SYS-024 | A | SWR-DCU-031, SWR-DCU-032 |
| SYS-026 | A | SWR-DCU-012, SWR-DCU-013, SWR-DCU-036, SWR-DCU-060, SWR-DCU-062, SWR-DCU-063, SWR-DCU-112, SWR-DCU-113, SWR-DCU-116 |
| SYS-027 | A | SWR-DCU-030, SWR-DCU-031, SWR-DCU-035 |
| SYS-030 | A | SWR-DCU-010, SWR-DCU-014 |
| SYS-031 | A | SWR-DCU-015, SWR-DCU-023, SWR-DCU-024, SWR-DCU-025 |
| SYS-032 | D | SWR-DCU-121 |
| SYS-033 | A | SWR-DCU-008 |
| SYS-034 | A | SWR-DCU-033, SWR-DCU-034, SWR-DCU-037 |
| SYS-035 | A | SWR-DCU-002, SWR-DCU-005, SWR-DCU-007, SWR-DCU-014, SWR-DCU-114 |
| SYS-036 | A | SWR-DCU-070 |
| SYS-037 | A | SWR-DCU-001, SWR-DCU-005, SWR-DCU-061, SWR-DCU-062 |
| SYS-038 | A | SWR-DCU-009, SWR-DCU-073, SWR-DCU-074, SWR-DCU-075, SWR-DCU-102, SWR-DCU-115 |
| SYS-039 | A | SWR-DCU-002, SWR-DCU-051, SWR-DCU-052, SWR-DCU-072 |
| SYS-040 | A | SWR-DCU-002, SWR-DCU-044, SWR-DCU-072 |
| SYS-041 | E | SWR-DCU-122 |
| SYS-042 | A | SWR-DCU-015, SWR-DCU-020, SWR-DCU-021, SWR-DCU-022, SWR-DCU-072 |
| SYS-043 | A | SWR-DCU-050 |
| SYS-060 | A | SWR-DCU-073, SWR-DCU-091, SWR-DCU-092, SWR-DCU-093, SWR-DCU-094 |
| SYS-061 | A | SWR-DCU-043, SWR-DCU-067, SWR-DCU-072, SWR-DCU-080, SWR-DCU-090, SWR-DCU-113 |
| SYS-062 | A | SWR-DCU-063, SWR-DCU-064, SWR-DCU-065, SWR-DCU-066, SWR-DCU-071 |
| SYS-063 | A | SWR-DCU-068 |
| SYS-070 | A | SWR-DCU-079, SWR-DCU-093, SWR-DCU-110, SWR-DCU-111 |
| SYS-071 | A | SWR-DCU-081 |
| SYS-072 | A | SWR-DCU-064, SWR-DCU-071 |
| SYS-090 | A | SWR-DCU-100, SWR-DCU-101, SWR-DCU-112 |

### 4.2 Safety mechanisms allocated to the DCU

| SM | SWR-DCU |
|---|---|
| SM-03 | SWR-DCU-001, SWR-DCU-004, SWR-DCU-006 |
| SM-04 | SWR-DCU-005, SWR-DCU-061, SWR-DCU-062 |
| SM-05 | SWR-DCU-007 |
| SM-06 | SWR-DCU-120, SWR-DCU-121 |
| SM-07 | SWR-DCU-009, SWR-DCU-015, SWR-DCU-023, SWR-DCU-024 |
| SM-08 | SWR-DCU-008, SWR-DCU-010 |
| SM-09 | SWR-DCU-033, SWR-DCU-034 |
| SM-10 | SWR-DCU-044, SWR-DCU-051, SWR-DCU-052 |
| SM-11 | SWR-DCU-009, SWR-DCU-073, SWR-DCU-076, SWR-DCU-077, SWR-DCU-078, SWR-DCU-080 |
| SM-12 | SWR-DCU-074 |
| SM-13 | SWR-DCU-079 |
| SM-14 | SWR-DCU-075 |
| SM-15 | SWR-DCU-026, SWR-DCU-064, SWR-DCU-070 |
| SM-18 | SWR-DCU-015, SWR-DCU-021, SWR-DCU-022 |
| SM-19 | SWR-DCU-014, SWR-DCU-114, SWR-DCU-115 |

## 5. Rationale

- **Gate and model as separate requirements.** SWR-DCU-014 and SWR-DCU-015 hold for any model, so the safety argument does not depend on the correctness of the generated engines (LS-SAF-001 §8).
- **Parameters by key.** Requirements reference parameter keys instead of values, so a parameter change in `timing.yaml` does not require a requirement change; verification limits come from the same file.
- **Later-stage requirements kept.** SWR-DCU-120 to SWR-DCU-122 reserve identifiers and keep the signals of the CAN matrix traceable.

## 6. References

- [LS-SRS-001 System requirements](../../02_system/system_requirements.md)
- [LS-SAIC-001](../../02_system/LS-SAIC.md)
- [LS-DCU-SAD-001 DCU software architecture](architecture.md)
- [Safety concept](../../05_safety/safety_concept.md) (LS-SAF-001), [security concept](../../06_security/security_concept.md) (LS-SEC-001)
- [Verification strategy](../../07_verification/verification_strategy.md) (LS-VER-001), [HIL test catalogue](../../07_verification/hil_test_catalog.md) (LS-HIL-002)
