# LockSys FMEA-lite (Stage A)

| Field | Value |
|---|---|
| Document ID | LS-SAF-002 |
| Version | 0.2 |
| Status | Draft |
| Owner | jlurg |

## 1. Purpose and scope

This FMEA-lite analyses the failure modes of the stage A elements against the hazards and safety goals of the [safety concept](safety_concept.md) (LS-SAF-001). It confirms that each relevant failure mode is detected or controlled by a safety mechanism (SM-nn), identifies the verification that demonstrates it, and records the residual risk.

- Scope: window and lock paths of the stage A bench (free-spinning JGB37-520 encoder motor, 12 V lock actuator, Pololu #2507 shield, NUCLEO-F103RB, Waveshare SN65HVD230 boards, TMP117, KL30 sense module, CGW, APP).
- Out of scope: quantitative hardware metrics (FMEDA, SPFM, LFM, PMHF), wear-out, EMC.

## 2. Method

| Column | Meaning |
|---|---|
| ID | `FM-nn` |
| Element / failure mode | Element of LS-SAF-001 §2.3 and its failure mode |
| Effect without mechanism | Worst system effect if no mechanism acted |
| Hazard (S) | Hazard of LS-SAF-001 §4 and its severity class |
| Detection / mechanism | Safety mechanism (SM-nn) or design measure |
| Reaction and time | Reaction and worst-case time to the safe state |
| Verification | Test that demonstrates the detection and reaction |
| Residual | Low / Medium / High, with the reason when not Low |

Residual rating: **Low** = detected and controlled within the budget by an independent mechanism; **Medium** = controlled only by a backstop, by the operator or by the bench limits; **High** = not controlled (none accepted for stage A).

## 3. Window path

| ID | Element / failure mode | Effect without mechanism | Hazard (S) | Detection / mechanism | Reaction and time | Verification | Residual |
|---|---|---|---|---|---|---|---|
| FM-01 | Encoder: no pulses (cable open, Hall sensor dead, supply lost) | Motion cannot be confirmed; a stall would go unnoticed | HAZ-03 (S3) | SM-18 `NO_MOTION` | Brake, press latched, DTC B1A11 ≤ `t_start_grace_ms` + 100 ms after drive-on | TST-HIL-SYS-007 (b) | Low |
| FM-02 | Encoder: one channel stuck | Counter dithers by ±1; net counts ≈ 0 | HAZ-03 (S3) | SM-18 `NO_MOTION` | Brake ≤ 100 ms after motion evidence ceases | TST-HIL-SYS-007 (d) | Low |
| FM-03 | Encoder: A/B swapped or motor leads reversed | Counts opposite to the command; direction errors undetected | HAZ-02 (S3) | SM-18 `DIR_MISMATCH`; polarity as configuration | Brake ≤ `t_start_grace_ms` + 110 ms after drive-on; DTC B1A15; window inhibited until UDS 0x14 or power-on reset | TST-HIL-SYS-007 (c) | Low |
| FM-04 | Encoder: spurious edges (noise) during a stall | Stall masked by false counts | HAZ-03, HAZ-04 (S3) | Timer input filter; backstops SM-07, SM-08, PSU limit, VNH5019 thermal shutdown | Over-current stop ≤ 100 ms or max-run stop at 8 s | TST-HIL-SYS-006, -008 | Medium: backstops only (RR-04) |
| FM-05 | Motor: shaft blocked (stall) | Stall current up to 3.2 A (PSU-limited to 3 A) until release | HAZ-03, HAZ-04 (S3) | SM-18 `NO_MOTION`; SM-07 backstop | Brake ≤ 100 ms | TST-HIL-SYS-007 (a), -008 | Low |
| FM-06 | Motor: open circuit (lead disconnected) | No motion, no current | — (availability) | SM-18 `NO_MOTION` | Brake, latch, DTC | TST-HIL-SYS-007 (b) | Low |
| FM-07 | Motor or bridge output short circuit | Over-current, heating | HAZ-04 (S2) | VNH5019 current limitation and thermal shutdown reported on EN/DIAG (SM-07); PSU limit; 7.5 A fuse | Reflex stop ≤ 1 ms after EN/DIAG low; DTC; 3 faults in 60 s → SAFE | TST-HIL-SYS-008 (d) | Low |
| FM-08 | VNH5019: output stage stuck on (internal failure) | Motor runs although the MCU commands off | HAZ-02, HAZ-03 (S3) | Not controllable by the MCU (no independent shut-off path, RR-01); encoder shows motion while not commanded | Operator emergency stop (PSU output); HIL `safe_state()` switches the PSU output off | Bench rule (LS-HIL-001 §12) | Medium: operator and bench measure only (RR-01) |
| FM-09 | MCU output: PWM stuck high | Bridge drives whenever INA ≠ INB | HAZ-02 (S3) | INA = INB brake path remains available; SM-19 gate commands brake; output read-back | Brake within one task period | Unit tests (HBridge read-back) | Low |
| FM-10 | MCU output: INA or INB stuck | Direction or brake not controllable through that line | HAZ-02 (S3) | PWM = 0 coast path remains available; output read-back; SM-18 `DIR_MISMATCH` if motion results | Coast within one task period | Unit tests (HBridge read-back); TST-HIL-SYS-007 | Low |
| FM-11 | MCU pins floating during reset | Random bridge inputs during reset | HAZ-02 (S3) | VNH5019 input pull-downs (A-SAF-09); early safe GPIO initialisation (SM-15) | Bridge high impedance; outputs low ≤ 0.1 ms after reset release | TST-HIL-SYS-011 | Medium: relies on inferred pull-downs (RR-02) |
| FM-12 | CS path open or stuck low | Over-current backstop blind | HAZ-04 (S2) | Latent; not detectable at free-running current (CS inactive while coasting, offset); primary stall detection by SM-18 | — | Bench calibration check (TST-MAN-SYS-005) | Medium: latent backstop (RR-05) |
| FM-13 | CS path stuck high | False over-current stop | — (availability) | SM-07 trips | Stop, DTC | TST-HIL-SYS-008 | Low |
| FM-14 | EN/DIAG line open | Driver faults not reported | HAZ-04 (S2) | Latent; VNH5019 still protects itself; SM-18 detects loss of motion | — | Wiring inspection (TST-MAN-SYS-016) | Medium: latent reporting loss |
| FM-15 | Supply under-voltage (PSU current limit during inrush, weak supply) | VNH5019 under-voltage shutdown, erratic motion | HAZ-03 (S3) | Soft-start ramp (≈ 200 ms); SM-10 under-voltage stop; EN/DIAG | Stop ≤ 110 ms after crossing 8.0 V | TST-HIL-SYS-013 | Low |
| FM-16 | Supply over-voltage | Stress on driver and actuator | HAZ-04 (S2) | SM-10 over-voltage stop (ADC saturation classified as over-voltage) | Stop ≤ 30 ms after crossing 16.5 V | TST-HIL-SYS-013 | Low |
| FM-17 | Reverse supply polarity | Damage to the driver | — (equipment) | Shield reverse protection to −16 V; the KL30 sense module sees the reversal (≈ −0.3 mA injection into PA4) | Check polarity before enabling the output | TST-MAN-SYS-012 | Low |

## 4. Lock path

| ID | Element / failure mode | Effect without mechanism | Hazard (S) | Detection / mechanism | Reaction and time | Verification | Residual |
|---|---|---|---|---|---|---|---|
| FM-20 | Position switch stuck | Lock state wrong; repeated pulses | HAZ-04 (S2), HAZ-06 | Feedback verification; ≤ 2 attempts (SM-09); state from feedback only | FAILED_ACTUATOR, lock state FAULT, DTC B1A20-71 | TST-HIL-SYS-015 (d) | Low |
| FM-21 | Actuator slow or jammed | Long energisation | HAZ-04 (S2) | 500 ms hard cap (SM-09); EN/DIAG reflex; PSU limit | Pulse ends ≤ 500 ms; retry once | TST-HIL-SYS-015 (c) | Low |
| FM-22 | Request flood (repeated lock/unlock) | Actuator overheating | HAZ-04 (S2) | Rate limit ≤ 10 actuations per 60 s (SM-09); CGW single transaction in flight | REJECTED_RATE_LIMIT, DTC B1A22-98 | TST-HIL-SYS-015 (e) | Low |
| FM-23 | Lock bridge stuck on | Continuous energisation | HAZ-04 (S2) | Not controllable by the MCU (RR-01); actuator thermal limit; PSU limit | Operator emergency stop | Bench rule (LS-HIL-001 §12) | Medium (RR-01) |
| FM-24 | Duplicate `DoorCmd` frames (repetition, CGW retransmission) | Double actuation | HAZ-05 (QM) | Deduplication by ReqId; one transaction in flight (SYS-027) | ≤ 1 actuation per ReqId | TST-HIL-SYS-016 (d) | Low |

## 5. Sensors and supply monitoring

| ID | Element / failure mode | Effect without mechanism | Hazard (S) | Detection / mechanism | Reaction and time | Verification | Residual |
|---|---|---|---|---|---|---|---|
| FM-30 | TMP117: no answer, wrong ID, stuck SDA | Over-temperature interlock unavailable | HAZ-04 (S2) | Identity check, 3 consecutive failures → SENSOR_FAULT; I2C recovery; DTC B1A30-96 | Mode DEGRADED; no actuator inhibit (see note) | TST-HIL-SYS-017 (d) | Medium (RR-08) |
| FM-31 | TMP117: stale data (Data_Ready never set) | Old temperature used | HAZ-04 (S2) | STALE after 3 s; job timeout 200 ms | Status STALE; DTC | TST-HIL-SYS-017 (d) | Low |
| FM-32 | TMP117: implausible value | Wrong interlock decision | HAZ-04 (S2) | Range (−40…125 °C) and gradient (≤ 5 °C/s) checks | OUT_OF_RANGE / IMPLAUSIBLE; DTC B1A31-64 | TST-HIL-SYS-017 (d) | Low |
| FM-33 | KL30 sense open (reads 0 V) | Under-voltage detected falsely | — (availability) | SM-10: starts inhibited | Window inhibited, DTC B1A40-16 | TST-HIL-SYS-013 | Low |
| FM-34 | KL30 sense gain drift | Thresholds shifted | HAZ-03 (S3) | Verification at nominal ± 0.3 V; VNH5019 internal under/over-voltage protection | — | TST-MAN-SYS-010 | Medium: no calibration storage in the MVP |
| FM-35 | Module not fitted (desk setup) | Supply window unavailable | HAZ-03 (S3) | Configuration check at startup (KL30 reading implausible) | Window inhibited | Bench configuration self-test | Low on the HIL bench |

Note to FM-30: LS-SAIC-001 v0.2 §13.2 classifies B1A30-96 as DEGRADED with no inhibit. LS-SAF-001 §7.6 confirms this policy for stage A: the TMP117 measures the ECU board, and the thermal protection of the actuators rests on the maximum run time, the over-current backstop, the VNH5019 thermal shutdown and the PSU limit. Window inhibition after a sensor fault is re-assessed once a load is coupled to the window (ACT-03).

## 6. Communication path

| ID | Element / failure mode | Effect without mechanism | Hazard (S) | Detection / mechanism | Reaction and time | Verification | Residual |
|---|---|---|---|---|---|---|---|
| FM-40 | CAN open or short | `WinCmd` lost while moving | HAZ-03 (S3) | SM-03 RX timeout; bus-off handling | Stop ≤ 130 ms | TST-HIL-SYS-004, -022 | Low |
| FM-41 | Corrupted frame (bit errors past the CAN CRC, buffer corruption) | Wrong request | HAZ-02 (S3) | SM-04 CRC-8 over DataID and payload | Frame discarded; 3 errors → STOP ≤ 70 ms | TST-HIL-SYS-010 | Low |
| FM-42 | Frozen sender (repeated frames) | Stale UP request keeps the motor running | HAZ-02, HAZ-03 (S3) | SM-04 alive counter; SM-03 HoldAge ≤ 400 ms | Frame discarded; STOP ≤ 70 ms | TST-HIL-SYS-010 | Low |
| FM-43 | Masquerade (frame received under another ID) | Wrong command type | HAZ-02 (S3) | DataID included in the CRC | Frame discarded | TST-HIL-SYS-010 | Low |
| FM-44 | Stale frames transmitted after bus-off recovery | Old UP request after recovery | HAZ-02 (S3) | CGW rewrites its transmit slots to STOP on bus-off; SM-05: valid STOP required before a new press | No motion until STOP and a new press | TST-HIL-SYS-022 | Low |
| FM-45 | CGW reset or power loss during a hold | Request stream ends; restart risk after reboot | HAZ-02, HAZ-03 (S3) | SM-03 RX timeout; SM-05 PressId seeding and latch | Stop ≤ 130 ms; no restart without a new press | TST-HIL-SYS-004 (c), -009 | Low |
| FM-46 | CGW `core` task hung, CAN transmit alive | Stale request transmitted | HAZ-03 (S3) | SM-16: `can_io` ages HoldAge and sends STOP; task watchdog | STOP ≤ 372 ms after the last keep-alive | TST-HIL-SYS-003 (d) | Low |
| FM-47 | Wi-Fi loss, APP crash or APP in background during a hold | Keep-alive stream ends | HAZ-03 (S3) | SM-02 keep-alive timeout | Stop ≤ 400 ms | TST-HIL-SYS-003 | Low |
| FM-48 | APP UI frozen while the keep-alive timer runs | Motion continues after release | HAZ-03 (S3) | Keep-alive generated in the UI isolate (stops with it); SM-08 maximum run 8 s | Stop ≤ 8 s | APP unit tests; TST-HIL-SYS-006 | Medium: bounded by max run (RR-07) |
| FM-49 | CAN transceiver dominant during DCU reset (Rs tied low on the module) | Bus errors, CGW bus-off | — (availability) | Bus-off handling on the CGW; DCU outputs off during reset | CGW STOP latch, recovery | TST-HIL-SYS-011, -022 | Low (RR-06) |

## 7. DCU processing

| ID | Element / failure mode | Effect without mechanism | Hazard (S) | Detection / mechanism | Reaction and time | Verification | Residual |
|---|---|---|---|---|---|---|---|
| FM-50 | Task hang or endless loop | Outputs frozen in the drive state | HAZ-03 (S3) | SM-11 hang monitor and IWDG with alive supervision | Outputs off ≈ 21 ms (hang monitor), reset ≤ 77 ms | TST-HIL-SYS-005 | Low |
| FM-51 | Hang with interrupts disabled | Hang monitor blocked | HAZ-03 (S3) | SM-11 IWDG | Reset ≤ 77 ms; pins high impedance | TST-HIL-SYS-005 | Low |
| FM-52 | HardFault / BusFault / UsageFault | Undefined outputs | HAZ-02 (S3) | Fault handlers: safe outputs, evidence in `noinit`, reset | Outputs safe < 1 µs after handler entry | TST-HIL-SYS-005 | Low |
| FM-53 | Stack overflow | Data corruption | HAZ-02 (S3) | SM-13 canary and stack placement | SAFE, DTC B1A51-44 | TST-HIL-SYS-005 | Low |
| FM-54 | Flash content corrupted | Undefined behaviour | All | SM-12 ROM CRC at startup | SAFE ≤ 200 ms after reset, DTC B1A50-45 | TST-HIL-SYS-012 | Low (background check at M6) |
| FM-55 | HSE clock lost | CAN timing out of tolerance | — (availability) | SM-14 CSS (M6); until then CAN errors and RX timeouts | SAFE, CAN silent | TST-MAN-SYS-014 | Medium until M6 |
| FM-56 | Statechart model or generator defect | Wrong sequencing | HAZ-02, HAZ-03 (S3) | SM-19 permission gate; independent reflexes; engine error → SAFE | Bounded by one task period; drives not permitted are refused | Unit tests; TST-HIL-SYS-001, -009 | Low |
| FM-57 | Repeated watchdog resets | Motion attempts after each reboot | HAZ-02 (S3) | SM-05 startup PressId latch; ≥ 3 resets within 10 min → SAFE | No restart without a new press | TST-HIL-SYS-005 | Low |

## 8. Actions

| ID | Action | Owner | Due | Status |
|---|---|---|---|---|
| ACT-01 | Specify the stage A DTC entries and stop reasons for `NO_MOTION` and `DIR_MISMATCH` (`WindowStopReason` has no free 4-bit value) | Interfaces (`locksys_enums.yaml`, `dtc_catalog.yaml`) | M1 | Closed: LS-SAIC-001 v0.2 defines StopReason STALL with DTC B1A11 (`NO_MOTION`) and StopReason DIR_MISMATCH with DTC B1A15 |
| ACT-02 | Classify ADC saturation of the KL30 sense input as over-voltage (FM-16) | DCU SAD | M2 | Closed at contract level: an ADC reading at full scale counts as over-voltage (`vbat_ov_dv`, B1A40-17); implementation in the DCU |
| ACT-03 | Decide window-start inhibition while `TempStatus` ≠ VALID (FM-30) | DTC catalogue owner | M2 | Decided for stage A: no inhibit (LS-SAF-001 §7.6); re-assessed at stage B |
| ACT-04 | Measure the bridge inputs during reset with the real driver powered (FM-11) and add discrete pull-downs if pulses > 10 µs are observed | HIL / hardware | M2 | Open |
| ACT-05 | Evaluate an implausible-speed check on encoder counts (FM-04) | DCU SAD | Stage B | Open |
| ACT-06 | Define the healing of the maximum-run latch for stage A, where no limit exists to clear it | DCU SAD | M2 | Closed: in stage A B1A13 inhibits nothing and the next press heals it (LS-SAIC-001 §13.2) |

## 9. References

| Reference | Title |
|---|---|
| LS-SAF-001 | [Functional safety concept](safety_concept.md) |
| LS-HIL-001 | [HIL architecture](../07_verification/hil_architecture.md) |
| LS-HIL-002 | [HIL test catalogue](../07_verification/hil_test_catalog.md) |
| LS-VER-002 | [Manual test procedures](../07_verification/procedures/README.md) |
| LS-SAIC-001 | System architecture and interface contract (`docs/02_system/LS-SAIC.md`) |
