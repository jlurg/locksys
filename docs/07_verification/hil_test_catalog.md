# LockSys HIL Test Catalogue (MVP, Stage A)

| Field | Value |
|---|---|
| Document ID | LS-HIL-002 |
| Version | 0.2 |
| Status | Draft |
| Owner | jlurg |

## 1. Purpose and scope

This catalogue specifies the automated HIL tests of the MVP for stage A of the window function (free-spinning encoder motor, no limits). It covers every [SAF] system requirement of stage A, the core functions and the security requirements that are verified on the bench. Tests not scheduled for the MVP are listed in the backlog (§5).

- Bench, topologies, configurations, measurement methods (M1–M6), signal definitions (drive-on, brake, drive-off) and decision rules: [LS-HIL-001](hil_architecture.md) §4, §5 and §8.4.
- Verification approach and traceability rules: [LS-VER-001](verification_strategy.md).
- Each test is implemented in `hil/tests/` with exactly one `test_id` marker and one or more `verifies` markers carrying the identifiers listed here.
- Parameter names in backticks refer to `interfaces/params/timing.yaml`; the values given are the current defaults.

Conventions:

- **Configuration:** SIM = HIL-SIM (unattended); REAL = HIL-REAL (attended).
- **Profile:** logic-analyser profile of LS-HIL-001 §8.2 used on 8-channel units.
- **Suites:** smoke (reduced trial counts), regression, nightly, soak, real.
- Every pass criterion on a timing applies the guard band (measured value + U ≤ limit) and is judged on the maximum over all trials.

## 2. Summary

| ID | Title | Verifies | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|---|
| TST-HIL-SYS-001 | Hold-to-run: motion only while held | SYS-005 | T3 | SIM, REAL | A | smoke, regression, real |
| TST-HIL-SYS-002 | Release-to-stop latency | SYS-020 | T3 | SIM | A | smoke, regression, nightly |
| TST-HIL-SYS-003 | Keep-alive loss | SYS-021 | T3 | SIM | A | smoke, regression, nightly |
| TST-HIL-SYS-004 | Command supervision at the DCU | SYS-022, SYS-035 | T1, T3 | SIM | A | smoke, regression |
| TST-HIL-SYS-005 | DCU hang, watchdog and fault handling | SYS-023, SYS-061 | T1 | SIM (DEV build) | A | regression, nightly |
| TST-HIL-SYS-006 | Maximum run time and stop sequence | SYS-030, SYS-033 | T1 | SIM, REAL | A | smoke, regression, real |
| TST-HIL-SYS-007 | Motion plausibility: `NO_MOTION`, `DIR_MISMATCH` | SYS-042 | T1 | SIM, REAL | A | smoke, regression, real |
| TST-HIL-SYS-008 | Over-current backstop and driver fault input | SYS-031 | T1 | SIM | A, B | smoke, regression |
| TST-HIL-SYS-009 | Press latch: no automatic restart | SYS-035 | T3, T1 | SIM | A | regression |
| TST-HIL-SYS-010 | E2E protection of window commands | SYS-037 | T1 | SIM | A | smoke, regression |
| TST-HIL-SYS-011 | De-energised outputs during reset; start-up time | SYS-036, SYS-072 | T1, T3 | SIM, REAL | D, B | regression, nightly |
| TST-HIL-SYS-012 | ROM integrity check at start-up | SYS-038 | T1 | SIM (destructive) | B | regression, nightly |
| TST-HIL-SYS-013 | Supply voltage window | SYS-039 | T1 | SIM | A | regression |
| TST-HIL-SYS-014 | ECU over-temperature interlock | SYS-040 | T1 | SIM | A | regression |
| TST-HIL-SYS-015 | Lock pulse, verification, retries and caps | SYS-003, SYS-004, SYS-034 | T1 | SIM, REAL | B | smoke, regression, real |
| TST-HIL-SYS-016 | Door transaction end to end | SYS-001, SYS-002, SYS-024, SYS-027 | T3, T1 | SIM | B | smoke, regression |
| TST-HIL-SYS-017 | Temperature acquisition, telemetry and consistency | SYS-008, SYS-009, SYS-010, SYS-061, SYS-090 | T3 | SIM, REAL | C | smoke, regression, nightly |
| TST-HIL-SYS-018 | Session authentication, replay and throttling | SYS-050, SYS-051, SYS-056 | T2 | SIM | — | smoke, regression |
| TST-HIL-SYS-019 | Single controller, flooding and malformed input | SYS-052, SYS-055, SYS-021 | T2, T3 | SIM | A | regression, nightly |
| TST-HIL-SYS-020 | Pairing, factory reset and secret hygiene | SYS-054, SYS-057 | T2 | SIM | — | regression |
| TST-HIL-SYS-021 | UDS-lite and DTC lifecycle | SYS-060, SYS-061 | T1 | SIM | — | smoke, regression |
| TST-HIL-SYS-022 | CAN bus-off and recovery | SYS-063, SYS-022, SYS-035 | T3 | SIM | A | regression, nightly |
| TST-HIL-SYS-023 | Endurance soak | SYS-070, SYS-071 | T3 | SIM | — | soak |

## 3. Requirement coverage

Test numbers refer to TST-HIL-SYS-nnn (for example, 001 = TST-HIL-SYS-001).

| Requirement | Class | Tests |
|---|---|---|
| SYS-005 | [SAF] | 001 |
| SYS-020 | [SAF] | 002 |
| SYS-021 | [SAF] | 003, 019 |
| SYS-022 | [SAF] | 004, 022 |
| SYS-023 | [SAF] | 005 |
| SYS-030 | [SAF] | 006 |
| SYS-031 | [SAF] | 008 |
| SYS-033 | [SAF] | 006 |
| SYS-034 | [SAF] | 015 |
| SYS-035 | [SAF] | 004, 009, 022 |
| SYS-036 | [SAF] | 011 |
| SYS-037 | [SAF] | 010 |
| SYS-038 | [SAF] | 012 (ROM integrity); HSE/CSS part by TST-MAN-SYS-014 |
| SYS-039 | [SAF] | 013 |
| SYS-040 | [SAF] | 014 |
| SYS-042 | [SAF] | 007 |
| SYS-043 | [SAF] | 013 (regression check against the PSU read-back); reference accuracy by TST-MAN-SYS-010 |
| SYS-001, SYS-002, SYS-024, SYS-027 | Core | 016 |
| SYS-003, SYS-004 | Core | 015 |
| SYS-008, SYS-009, SYS-010, SYS-090 | Core | 017 |
| SYS-050, SYS-051, SYS-056 | [SEC] | 018 |
| SYS-052, SYS-055 | [SEC] | 019 |
| SYS-054, SYS-057 | [SEC] | 020 |
| SYS-053 | [SEC] | TST-MAN-SYS-013 (manual) |
| SYS-060, SYS-061 | Diagnostics | 021 (DTC reactions also checked in 005, 007, 008, 010, 017, 022) |
| SYS-063 | Diagnostics | 022 |
| SYS-070, SYS-071 | Resources, endurance | 023 |
| SYS-072 | Start-up | 011 |

## 4. Test specifications

### TST-HIL-SYS-001 Hold-to-run: motion only while held

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-005 | SM-01, SM-02, SM-03, SM-19 | T3 | SIM; REAL subset | A | smoke (10 holds), regression, real |

- **Preconditions:** DCU and CGW in NORMAL; APP simulator authenticated; window idle; window plant model at nominal speed.
- **Steps:**
  1. Per direction, 50 holds of 2.0 s: SYNC marker, first `WindowMove` with a new `press_id`, keep-alive every 100 ms, SYNC marker, `WindowStop`.
  2. 100 negative sessions, evenly split: status requests only; `WindowStop` without a press; `WindowMove` with direction STOP or an invalid value; `WindowMove` with a `press_id` not greater than the previous one; first `WindowMove` with `hold_ms` above the new-press limit (300 ms); connect and close without commands.
  3. REAL: 10 holds per direction on the real motor.
- **Measurement:** M1 drive interval (drive-on to drive-off) against the SYNC markers; direction from INA/INB and from the sign of the encoder counts; `DCU_WinSts` state and stop reason; in step 2 the number of drive pulses longer than 10 µs.
- **Pass criteria:**
  - Every hold: drive interval 2.0 s ± 150 ms; direction as requested; `DCU_WinSts` MOVING_UP or MOVING_DOWN during the hold, then STOPPED with stop reason RELEASED.
  - Negative sessions: 0 drive pulses longer than 10 µs; the APP simulator receives the specified rejection results.
  - REAL: same criteria; the speed reported in `DCU_WinSts` is within ±20 % of the speed derived from the captured encoder edges.

### TST-HIL-SYS-002 Release-to-stop latency

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-020 | SM-01, SM-02 | T3 | SIM | A | smoke (10 trials), regression (100), nightly (1 000) |

- **Preconditions:** APP simulator answers `Ping` immediately; last RTT ≤ 200 ms sampled within 2 s.
- **Steps:**
  1. 100 holds of random length 1.0–3.0 s (seed logged); the host emits a SYNC marker immediately before sending `WindowStop`.
  2. A trial is excluded if the last RTT sample is above 200 ms or older than 2 s; more than 5 % excluded trials end the test as ERROR.
  3. Degraded link (nightly, 20 trials): the APP simulator delays `Pong` so that the RTT is 180–200 ms and sends `WindowStop` 100 ms after the SYNC marker.
- **Measurement:** M4 mapped to M1: SYNC (send) → drive-off; CGW TRACE0 `WindowStop` pulse → drive-off.
- **Pass criteria:**
  - Maximum SYNC → drive-off ≤ 130 ms (SYS-020 limit of 150 ms minus the APP send allocation of 20 ms).
  - Maximum CGW TRACE0 → drive-off ≤ 25 ms.
  - Degraded link: SYNC → drive-off ≤ 130 ms with no RTT-gate stop during the hold.
  - p50, p95, p99 and maximum reported.

### TST-HIL-SYS-003 Keep-alive loss

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-021 | SM-02, SM-03, SM-16 | T3 | SIM | A | smoke (case a × 10), regression, nightly (case c × 100) |

- **Steps:** during a hold:
  1. (a) The APP simulator stops sending with the socket open, × 100.
  2. (b) The APP simulator process is killed, × 100.
  3. (c) The WLAN-DUT is disconnected, × 20.
  4. (d) DEV CGW build: the `core` task hang is injected while `can_io` keeps running, × 20.
  5. After each case the APP simulator resumes keep-alives for the same press or reconnects.
- **Measurement:** M1: CGW TRACE0 pulse of the last accepted keep-alive → drive-off; in case (d) also the first `CGW_WinCmd` with Req = STOP on CAN_RX.
- **Pass criteria:**
  - Cases (a)–(d): last keep-alive → drive-off ≤ 400 ms.
  - Case (d): `CGW_WinCmd` STOP on the bus ≤ 372 ms after the last keep-alive.
  - No drive-on for the same press after the stop; a new press moves.

### TST-HIL-SYS-004 Command supervision at the DCU

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-022, SYS-035 | SM-03, SM-05 | T1 (a, b, d), T3 (c) | SIM | A | smoke (case a × 10), regression |

- **Steps:**
  1. (a) T1: the restbus sends `CGW_WinCmd` UP with valid E2E, a new PressId and HoldAge 0–50 ms; after drive-on it stops `CGW_WinCmd` while `CGW_NodeSts` continues, × 50.
  2. (b) T1: during motion the restbus raises HoldAge above 400 ms while frames continue, × 20; start attempts with HoldAge above 400 ms, × 20.
  3. (c) T3: CGW reset through EN during a hold, × 50; the APP simulator keeps holding and reconnects.
  4. (d) T1: the restbus stops only `CGW_NodeSts` (`CGW_WinCmd` STOP continues); window and lock requests follow.
- **Measurement:** M1: end of the last valid `CGW_WinCmd` on CAN_RX → drive-off; in (b) first frame with HoldAge above 400 ms → drive-off; `DCU_WinSts`, `DCU_NodeSts`, DTCs through UDS.
- **Pass criteria:**
  - (a) Drive-off ≤ 130 ms after the last valid frame; stop reason CAN_TIMEOUT.
  - (b) Drive-off ≤ 12 ms after the first frame with HoldAge above 400 ms; stop reason HOLD_TIMEOUT; no start with HoldAge above 400 ms.
  - (c) Drive-off ≤ 130 ms after the last valid frame; 0 restarts after the CGW reboots; a new press moves.
  - (d) DEGRADED ≤ 600 ms after the last `CGW_NodeSts`; window starts rejected; lock requests accepted; DTC U1A00-87 confirmed after 1 s.

### TST-HIL-SYS-005 DCU hang, watchdog and fault handling

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-023, SYS-061 | SM-11, SM-13 | T1 | SIM, DEV or HIL build with fault injection | A | regression, nightly |

- **Steps:** during motion, 20 injections each through the DEV fault-injection channel on the DCU UART (command set of `interfaces/uart/telemetry_v1.md`):
  1. Task hang.
  2. Task hang with interrupts disabled.
  3. Skipped alive checkpoint.
  4. HardFault.
  5. Stack overflow.
  6. Between trials the watchdog-reset counter is cleared through the fault-injection channel or by a power cycle.
  7. Separate sub-test: 3 task hangs within 10 min.
- **Measurement:** M4 mapped to M1: SYNC marker emitted immediately before the injection command → drive-off; reset reason from `$LSRST`; DTCs through UDS 19 02; stopping mechanism (hang monitor or IWDG) from `#LOG`.
- **Pass criteria:**
  - Drive-off ≤ 100 ms for the hang, hang with interrupts disabled, skipped checkpoint and HardFault cases.
  - Reset reason WATCHDOG for the hang and checkpoint cases; DTC B1A52-47; HardFault DTC per the DTC catalogue.
  - Stack overflow: canary violation detected, SAFE, DTC B1A51-44, no motion.
  - 3 watchdog resets within 10 min → SAFE.
  - No drive-on after any reset without a new press.

### TST-HIL-SYS-006 Maximum run time and stop sequence

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-030, SYS-033 | SM-08, SM-19 | T1 | SIM; REAL subset (case a) | A | smoke (case b × 5), regression, real |

- **Steps:**
  1. (a) The restbus holds UP, then DOWN, for 10 s with fresh frames (free-spinning plant, no limit), × 10 each; then a press in the opposite direction.
  2. (b) Drive UP, release and immediately press DOWN with a new PressId, × 50; DOWN then UP, × 50; restart in the same direction immediately after a stop, × 20.
- **Measurement:** M1: drive-on → drive-off; brake interval (INA = INB = 0, PWM high); off interval (PWM low) until the next drive-on.
- **Pass criteria:**
  - (a) Drive-off at 8.00 ± 0.05 s after drive-on; stop reason MAX_RUNTIME; DTC B1A13-92; a press in the opposite direction moves; the press is latched (no restart while the same press is held); a new press in either direction moves, because B1A13 inhibits nothing in stage A (LS-SAIC-001 §13.2).
  - (b) Brake 100 ± 5 ms; bridge off continuously ≥ 150 ms between the end of the brake and the next drive-on, for reversals and for same-direction restarts.

### TST-HIL-SYS-007 Motion plausibility: NO_MOTION and DIR_MISMATCH

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-042 | SM-18 | T1 | SIM; REAL subset | A | smoke (case a × 2), regression, real |

- **Steps:** window plant at nominal speed; the restbus holds the request.
  1. (a) Stall: the plant stops the encoder edges ≥ 500 ms after drive-on, × 20 per direction.
  2. (b) Dead encoder from the start, × 20.
  3. (c) Reversed encoder (counts opposite to the command), × 20.
  4. (d) Channel B constant during motion, × 10.
  5. (e) Threshold bracket: plant speed at 0.5 × and at 2 × the trip level (`no_motion_min_pct` of the expected counts), × 10 each; 50 holds at the minimum supervised duty (25 %) with the nominal plant.
  6. (f) Soft start: PWM duty ramp from drive-on.
  7. REAL (attended): encoder connector unplugged, × 5; A and B swapped, × 5.
- **Measurement:** M1: last encoder edge (SYNC-marked by the stimulus) → drive-off in (a) and (d); drive-on → drive-off in (b) and (c); stimulus PWM capture in (f); stop reason, fault flags, DTCs.
- **Pass criteria:**
  - (a), (d): drive-off ≤ 100 ms after the last encoder edge.
  - (b): drive-off ≤ `t_start_grace_ms` + 100 ms after drive-on.
  - (c): drive-off ≤ `t_start_grace_ms` + 110 ms after drive-on.
  - (e): trip at 0.5 × the trip level, no trip at 2 ×; 0 trips in the 50 holds at minimum duty.
  - (f): duty reaches the commanded value after `t_softstart_ms` ± 20 % (informative).
  - `NO_MOTION` trips ((a), (b), (d)): brake then off; stop reason STALL; WindowState BLOCKED; DTC B1A11-71; PressId latched; the next press moves.
  - `DIR_MISMATCH` trips ((c)): brake then off; stop reason DIR_MISMATCH; WindowState FAULT; DTC B1A15-64; window starts rejected until UDS 0x14 or a power-on reset.
  - REAL: unplugged encoder trips within the (b) limit; swapped channels trip within the (c) limit.

### TST-HIL-SYS-008 Over-current backstop and driver fault input

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-031 | SM-07 | T1 | SIM | A (window), B (lock) | smoke (case a × 2), regression |

- **Steps:** during motion, after the soft start and the inrush blanking:
  1. (a) DAC step on WIN_CS to `i_oc_backstop_ma` + 15 % (CS voltage = I × 0.140 V/A), held ≥ 200 ms, × 20.
  2. (b) Step to `i_oc_backstop_ma` − 15 % for 1 s, × 10.
  3. (c) Step above the threshold for 40 ms (shorter than the 50 ms qualification), × 10.
  4. (d) WIN_EN_DIAG pulled low during motion, 3 times within 60 s.
  5. (e) LOCK_EN_DIAG pulled low during a lock pulse, × 3.
- **Measurement:** M1: SYNC (step or EN/DIAG edge, emitted by the stimulus) → drive-off; CS value seen by the DCU through the window diagnostic DID.
- **Pass criteria:**
  - (a) Drive-off ≥ 45 ms and ≤ 60 ms after the step (qualification 50 ms; SYS-031 limit 100 ms); stop reason OVERCURRENT; DTC per the DTC catalogue.
  - (b), (c) No stop.
  - (d) Drive-off ≤ 1 ms after EN/DIAG low; stop reason DRIVER_FAULT; DTC B1A10-96; the third fault within 60 s → SAFE.
  - (e) Lock bridge off ≤ 1 ms; DTC B1A21-96.

### TST-HIL-SYS-009 Press latch: no automatic restart

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-035 | SM-02, SM-05, SM-19 | T3 (a–d), T1 (e) | SIM | A | regression |

- **Steps:** 20 trials per scenario.
  1. (a) The same `press_id` sends UP, then DOWN.
  2. (b) DCU reset (NRST) during a hold; the APP simulator continues the same press after the DCU is back in NORMAL.
  3. (c) CGW reset (EN) during a hold; the APP simulator reconnects and continues the same press.
  4. (d) Keep-alives of the same press resume after the CGW keep-alive timeout.
  5. (e) T1: immediately after a DCU reset, the restbus sends UP with the PressId that was active before the reset.
  6. (f) Positive control after each scenario: a new press.
- **Measurement:** M1 drive pulses; `CommandAck` results; PressId echo in `DCU_WinSts`.
- **Pass criteria:**
  - (a)–(e): 0 drive pulses longer than 10 µs after the stop.
  - (f): drive-on for every new press.

### TST-HIL-SYS-010 E2E protection of window commands

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-037 | SM-04 | T1 | SIM | A | smoke (CRC and frozen counter, 20 frames each), regression |

- **Steps:** restbus fault hooks, 100 frames per fault type, once while idle (start attempt with a new PressId) and once during motion:
  1. Wrong CRC.
  2. CRC computed with a wrong DataID.
  3. Wrong DLC.
  4. Frozen alive counter.
  5. Counter jump greater than 2.
  6. Replay of a recorded valid sequence.
  7. Positive control: counter wrap 15 → 0.
- **Measurement:** M1 drive pulses; drive-off relative to the first erroneous frame on CAN_RX; E2E state and counters through the E2E diagnostic DID (FD08); DTCs.
- **Pass criteria:**
  - 0 motion starts caused by invalid frames.
  - During motion, 3 consecutive errors → drive-off ≤ 70 ms after the first erroneous frame; stop reason E2E_ERROR.
  - The counter wrap is accepted.
  - After errors, motion only after 2 valid frames and a new PressId.
  - DTC U1A02-83 (CRC or DataID) or U1A02-82 (counter).

### TST-HIL-SYS-011 De-energised outputs during reset; start-up time

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-036, SYS-072 | SM-11, SM-15 | T1 (DCU), T3 (CGW) | SIM; REAL subset | D (glitch), B (start-up time) | regression, nightly (full counts) |

- **Steps:**
  1. (a) 100 NRST pulses (low time 1–50 ms, seeded) while the restbus sends valid UP frames with a fresh PressId.
  2. (b) 20 IWDG resets (DEV build, injected task hang); the watchdog-reset counter cleared between trials.
  3. (c) 20 resets by UDS 11 01.
  4. (d) With capability `power.dcu_usb`: 100 DCU power cycles (off-time 10 ms–2 s); skipped and reported otherwise.
  5. (e) 20 CGW resets through EN.
  6. (f) REAL (attended): 20 NRST pulses with the actuator supply on and the real motor connected.
- **Measurement:** logic analyser at ≥ 100 MS/s on all bridge inputs, triggered by the reset SYNC; reset release (NRST rising edge or supply return) → first `DCU_NodeSts` with mode NORMAL; CGW EN release → CGW TRACE0 "AP started" pulse; REAL: encoder edges.
- **Pass criteria:**
  - No pulse longer than 10 µs on any INA, INB or PWM line from reset assertion until NORMAL; no drive-on without a new press.
  - DCU NORMAL ≤ 300 ms after reset release or power-up; first `DCU_NodeSts` time reported.
  - CGW access point up ≤ 3 s after EN release.
  - REAL: 0 encoder edges during and after the resets.

### TST-HIL-SYS-012 ROM integrity check at start-up

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-038 (ROM integrity) | SM-12 | T1 | SIM, destructive | B | regression (release and hotfix), nightly |

- **Steps:**
  1. Flash the CI-built image with a corrupted checksum word (`dcu_badcrc.hex`, SHA-256 from the build manifest).
  2. Release reset, × 5; send valid `CGW_WinCmd` and `CGW_DoorCmd` requests.
  3. Restore the good image, clear the SAFE latch (UDS 10 03 followed by 11 01, or power-on reset) and verify NORMAL.
- **Measurement:** reset release (SYNC) → first `DCU_NodeSts` with mode SAFE on CAN_RX; `$LSSTA`; DTCs through UDS.
- **Pass criteria:**
  - SAFE ≤ 200 ms after reset release.
  - `DCU_NodeSts` continues every 100 ms ± 10 % in SAFE.
  - Requests rejected with REJECTED_MODE; 0 drive pulses.
  - DTC B1A50-45.
  - NORMAL after the good image is restored and the latch cleared.

### TST-HIL-SYS-013 Supply voltage window

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-039, SYS-043 | SM-10 | T1 | SIM; KL30 sense module on PSU CH2 | A | regression |

- **Steps:**
  1. (a) Start attempts at 8.7, 9.3, 15.7 and 16.3 V, × 5 each (thresholds ± 0.3 V to cover the KL30 measurement error).
  2. (b) During motion, CH2 steps from 12.0 V to 7.7 V, × 5.
  3. (c) During motion, CH2 steps from 12.0 V to 16.8 V, × 5.
  4. (d) Return to 12.0 V and wait for healing.
  5. (e) CH2 at 9.0, 12.0 and 16.0 V, idle: read `DcuSts_Vbat` and DID 0xFD02, × 5 each.
- **Measurement:** the stimulus ADC on the KL30 sense node emits a SYNC at the threshold crossing (8.0 V or 16.5 V equivalent) → drive-off (M1); stop reason; DTCs.
- **Pass criteria:**
  - Starts accepted at 9.3 V and 15.7 V; rejected at 8.7 V and 16.3 V.
  - Under-voltage: drive-off 100–110 ms after crossing 8.0 V; stop reason UNDERVOLTAGE; DTC B1A40-16.
  - Over-voltage: drive-off ≤ 30 ms after crossing 16.5 V; stop reason OVERVOLTAGE; DTC B1A40-17.
  - DTC healed after 1 s within 9.0–16.0 V.
  - (e) Reported KL30 within ± `lim_kl30_accuracy_mv` (200 mV) of the PSU CH2 voltage read-back (SYS-043).

### TST-HIL-SYS-014 ECU over-temperature interlock

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-040 | SM-10 | T1 | SIM (TMP117 emulator) | A | regression |

- **Steps:**
  1. (a) Emulated 85.5 °C; start attempts, × 5.
  2. (b) Emulated 82.0 °C after (a); start attempts, × 5.
  3. (c) Emulated 79.5 °C; start attempt.
  4. (d) During motion, ramp from 84.0 °C to 86.0 °C at 1 °C/s.
- **Measurement:** drive state, stop reason, temperature status, DTCs.
- **Pass criteria:**
  - (a), (b) Starts rejected.
  - (c) Start accepted.
  - (d) Drive-off within 2 temperature samples (≤ 2.1 s) after the emulated value exceeds 85.00 °C; stop reason OVERTEMP; DTC B1A32-98.

### TST-HIL-SYS-015 Lock pulse, verification, retries and caps

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-003, SYS-004, SYS-034 | SM-09 | T1 | SIM; REAL subset | B | smoke (cases a and b × 2), regression, real |

- **Steps:**
  1. (a) LOCK and UNLOCK alternately, × 20 each, with new ReqIds, paced within the rate limit.
  2. (b) 20 requests for the state already reached.
  3. (c) Plant stroke of 600 ms, × 5.
  4. (d) Position switch stuck, × 5.
  5. (e) 11 alternating requests within 60 s.
  6. (f) REAL (attended): 20 cycles on the real actuator.
- **Measurement:** M1 on LOCK_INA, LOCK_INB, LOCK_PWM and LOCK_POS; stimulus pulse counter (M2); `DCU_DoorSts` (LastReqId, LastResult, lock state); DTCs; REAL: actuator current with DMM or oscilloscope (manual).
- **Pass criteria:**
  - (a) One pulse of 300 ± 5 ms in the commanded direction, then off; LastResult OK; lock state from the switch.
  - (b) OK with 0 PWM edges.
  - (c) Two pulses, each ≤ 500 ms, pause 500 ± 20 ms; OK ≤ 2.0 s after the request.
  - (d) ≤ 2 attempts; FAILED_ACTUATOR; lock state FAULT; DTC B1A20-71.
  - (e) ≤ 10 actuations; the 11th request REJECTED_RATE_LIMIT; DTC B1A22-98.
  - No pulse longer than 500 ms in any case.
  - (f) Pulse 300 ± 5 ms; switch settled ≤ 70 ms after the pulse; peak current recorded.

### TST-HIL-SYS-016 Door transaction end to end

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-001, SYS-002, SYS-024, SYS-027 | — | T3 (a–c), T1 (d) | SIM | B | smoke (case a × 2), regression |

- **Steps:**
  1. (a) APP simulator `DoorCommand` LOCK and UNLOCK, × 25 each, paced by the rate limit.
  2. (b) The plant forces UNLOCKED, LOCKED, a stroke in progress, a stuck switch (FAULT) and a DCU reset (UNKNOWN), × 10 each.
  3. (c) A second `DoorCommand` while one is in flight, × 10.
  4. (d) T1: the restbus repeats one ReqId 3 times, and sends a new ReqId while a transaction is in flight.
- **Measurement:** M4 mapped to M1: SYNC (send) → `CommandAck` → transitional `StatusUpdate` → `DoorCommandResult`; `CGW_DoorCmd` content on CAN; actuation count (M2); APP simulator state against the plant state.
- **Pass criteria:**
  - (a) Send → transitional state ≤ 264 ms; send → final state ≤ 964 ms (≤ 1964 ms with one retry); the request type in `CGW_DoorCmd` matches the command in every trial.
  - (b) The APP simulator shows the plant state within 1.5 s in 50 of 50 cases.
  - (c) REJECTED_BUSY from the CGW; no additional actuation.
  - (d) Exactly 1 actuation per ReqId; a new ReqId received while busy is dropped (LastReqId unchanged).

### TST-HIL-SYS-017 Temperature acquisition, telemetry and consistency

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-008, SYS-009, SYS-010, SYS-061, SYS-090 | — | T3 | SIM (emulator); REAL subset | C | smoke (60 samples), regression, nightly (600 samples) |

- **Steps:**
  1. (a) Constant values, including negative values and rounding ties (raw ≡ 16 mod 32).
  2. (b) Ramp at 1 °C/s.
  3. (c) 600 consecutive samples.
  4. (d) Faults: address NACK; wrong ID (0x0119, 0x2117); clock stretching 50 ms; stuck SDA; Data_Ready never set; 0x8000; 126 °C; −41 °C; 10 °C step.
  5. (e) Fault removal.
  6. (f) REAL: real sensor for 10 min.
- **Measurement:** I2C decode; `$LSTMP` parser (checksum, sequence continuity, monotonic `t_ms`, period); `DCU_TempSts`; APP simulator `StatusUpdate`.
- **Pass criteria:**
  - Period 1000 ± 50 ms.
  - Value = round-half-away-from-zero(raw × 0.78125) in 0.01 °C units.
  - UART and CAN agree for the same sample (`seq & 0xFF` = `SampleSeq`); the APP simulator shows the value ≤ 3 s after the sample.
  - 0 checksum errors; 0 sequence gaps.
  - Faults: SENSOR_FAULT after 3 consecutive failures; STALE after 3 s without new data; OUT_OF_RANGE and IMPLAUSIBLE as specified; ≥ 9 recovery clocks on stuck SDA; DTCs B1A30-96 and B1A31-64.
  - (e) VALID on the first good sample after removal.
  - (f) REAL: readings VALID and plausible; accuracy is verified by TST-MAN-SYS-011.

### TST-HIL-SYS-018 Session authentication, replay and throttling

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-050, SYS-051, SYS-056 | CSR-004, CSR-006, CSR-007 | T2 | SIM; USB-CAN monitor | — | smoke (5 wrong keys), regression |

- **Steps:**
  1. (a) 100 authentication attempts with random wrong keys.
  2. (b) Timing of verifications after the third failure.
  3. (c) Record a valid session; replay its frames within the same session and in a new session.
  4. (d) Bit flips in tag and body; counter 0 inside a session; counter equal to the previous one.
- **Measurement:** WebSocket close codes and `AuthResult`; `CGW_WinCmd` and `CGW_DoorCmd` on CAN (M3); host timestamps (M4).
- **Pass criteria:**
  - (a) Close 4002 every time; 0 command frames on CAN (`CGW_WinCmd` stays STOP, no `CGW_DoorCmd`).
  - (b) ≥ 5 s between verifications after the third failure.
  - (c), (d) Frames rejected; session closed with 1008; 0 command frames; STOP latched.

### TST-HIL-SYS-019 Single controller, flooding and malformed input

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-052, SYS-055, SYS-021 | CSR-003, CSR-008, CSR-009 | T2 (a, b, d), T3 (c) | SIM | A (case c) | regression, nightly (fuzzing) |

- **Steps:**
  1. (a) Two APP simulators: the second authenticates while the first is active, × 10; then after the first has been idle for ≥ 3 s.
  2. (b) Text frame; 300 B frame; 40 frames/s; plain HTTP request to `/ws/v1`; seeded post-authentication fuzzing, 1 000 frames (10 000 nightly); pre-authentication fuzzing, 200 frames (nightly).
  3. (c) T3: flood during a hold.
  4. (d) New session after each close.
- **Measurement:** close codes and results; CGW reset reason and uptime; drive-off timing (M1) in (c).
- **Pass criteria:**
  - (a) REJECTED_BUSY and close 4003 while the first session is active; pre-emption after 3 s idle (old session closed with 4004).
  - (b) Close 1003 (text), 1009 (> 256 B), 1008 (rate limit, malformed); 400 for plain HTTP; 0 CGW resets.
  - (c) Drive-off ≤ 400 ms after the last valid keep-alive.
  - (d) New session accepted ≤ 5 s after the close.

### TST-HIL-SYS-020 Pairing, factory reset and secret hygiene

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-054, SYS-057 | CSR-010…CSR-013 | T2 | SIM; stimulus drives CGW BOOT (interlocked) | — | regression |

- **Steps:**
  1. (a) BOOT held 3 s.
  2. (b) BOOT held 6 s, then authentication with the new key inside the window.
  3. (c) BOOT held 6 s, no authentication until the window expires.
  4. (d) BOOT held 11 s (factory reset), then re-pairing.
  5. (e) Secret scan of all stored logs, evidence and artifacts of the run.
- **Measurement:** `ServerHello.pairing_window_open`; authentication results with the old and new keys; window duration (M4); WLAN association with the old and new passphrase. The QR block is read and decoded in memory only, never persisted.
- **Pass criteria:**
  - (a) No pairing window.
  - (b) Window open; new key committed only after the successful authentication; old key rejected afterwards.
  - (c) Window closes at 120 ± 1 s; old key still valid; pending key rejected.
  - (d) K_pair erased; a session using the old key is closed with 4005; the new passphrase differs from the old one; access point restarted; re-pairing works.
  - (e) 0 occurrences of the key or the passphrase (raw, hex, base64, base64url) in redacted logs, evidence and artifacts.

### TST-HIL-SYS-021 UDS-lite and DTC lifecycle

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-060, SYS-061 | — | T1 | SIM | — | smoke (22 F195, 19 02), regression |

- **Steps:**
  1. (a) udsoncan suite over ISO-TP 0x7A0/0x7A8: sessions 10 01 and 10 03; 11 01; 14 FFFFFF; 19 02 and 19 0A; 22 F18C, F195 and FD00–FD09; 3E; unsupported services, sub-functions and lengths; S3 time-out 5 s.
  2. (b) DTC lifecycle: TMP117 NACK injected; fault removed; clear all.
  3. (c) SAFE latch: enter SAFE (DEV stack-canary injection); 11 01 in the default session; then 10 03 followed by 11 01.
- **Measurement:** ISO-TP traces (M3); responses decoded by udsoncan; `$LSDTC`.
- **Pass criteria:**
  - (a) Positive responses and negative response codes 0x11, 0x12, 0x13, 0x22, 0x31 and 0x7F as specified.
  - (b) Status bits 0, 2, 3 and 5 as specified; bit 0 cleared after healing; fault memory empty after 14 FFFFFF.
  - (c) 11 01 in the default session resets and SAFE persists; 10 03 followed by 11 01 clears SAFE.

### TST-HIL-SYS-022 CAN bus-off and recovery

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-063, SYS-022, SYS-035 | SM-05 | T3 | SIM; relay module (`can.short`) | A | regression, nightly |

- **Steps:**
  1. CANH–CANL short for seeded random durations of 0.2–3.0 s, × 20, idle and during a hold.
  2. After recovery: an UP request with the old press, then STOP, then a new press.
- **Measurement:** CAN_RX activity; drive-off; first DCU frame after the short is removed; telemetry `t_ms` continuity (no reset); DTCs.
- **Pass criteria:**
  - Drive-off ≤ 130 ms after the short.
  - DTCs U1A01-88 (DCU) and U1B01-88 (CGW).
  - Transmission resumes ≤ 0.51 s after the short is removed, and always ≤ 1 s; no reset.
  - No motion after recovery until a valid STOP frame and then a new press have been received.

### TST-HIL-SYS-023 Endurance soak

| Verifies | Mechanisms or controls | Topology | Config | Profile | Suites |
|---|---|---|---|---|---|
| SYS-070, SYS-071 | All | T3 | SIM | — | soak (8 h, weekly from M5 and per release candidate) |

- **Steps:**
  1. 8 h run with randomised holds of 0.2–3 s (seed logged): at least 3 000 window presses and 300 lock cycles (SYS-071). The 24 h soak with 10 000 window and 1 000 lock cycles is LATER.
  2. Continuous temperature telemetry.
  3. Every 10 min: DID reads of stack high-water (FD06) and CPU load (FD07); CGW free-heap signal from the CGW status frame.
- **Measurement:** stimulus counters and drive capture (M2); telemetry; CAN logs; DTC reads.
- **Pass criteria:**
  - 0 DCU or CGW resets; 0 unexpected DTCs.
  - Stack high-water ≤ 75 %; CPU load ≤ 60 %; CGW free heap ≥ 30 %.
  - 0 telemetry gaps; 0 drive pulses outside holds.
  - Every release followed by drive-off within the SYS-020 limit (stimulus capture).

## 5. Backlog (LATER)

| Item | Requirements | Planned |
|---|---|---|
| Virtual limit stop, rejection of motion into an active limit, position reporting | SYS-006, SYS-007 | Stage B |
| Speed-control tracking and supervision | Stage C requirements (to be defined) | Stage C |
| Physical limit switches: EXTI stop, plausibility (both active, limit not left) | SYS-006, SYS-032 | Stage D |
| Obstacle detection and reversal (spring gauge, 100 N criterion) | SYS-041 | Stage E |
| Status push latency, 1 Hz / 10 Hz rates, stale marking | SYS-025 | After M5 |
| Cyclic CAN timing and bus load, idle and in motion | SYS-026 | After M5 |
| Interface version mismatch, both directions | SYS-062 | After M5 |
| Tick jitter added by telemetry, with and without log load | SYS-090 | After M5 |
| Worst-case CPU load with all statechart events pending | SYS-070 | After M5 |
| HSE/CSS failure through fault injection; background ROM CRC | SYS-038 | M6 |
| Real phone in the loop (display, release-to-stop with a phone) | SYS-001, SYS-020, SYS-096 | LATER |
| Silent Wi-Fi loss (adapter power cut) | SYS-021 | LATER |
| CAN open circuit by relay | SYS-022 | LATER |
| Bit-level CAN disturbance | SYS-037 | LATER |
| Oscilloscope automation for physical-layer checks | — | LATER |
| CGW diagnostics over UDS | — | LATER |
| HIL-REAL thermal soak | SYS-071 | LATER |
| Latency statistics with 1 000 trials per chain for SYS-021 and SYS-022 | SYS-021, SYS-022 | Nightly extension |

## 6. References

| Reference | Title |
|---|---|
| LS-HIL-001 | [HIL architecture](hil_architecture.md) |
| LS-VER-001 | [Verification strategy](verification_strategy.md) |
| LS-VER-002 | [Manual test procedures](procedures/README.md) |
| LS-SAF-001 | [Functional safety concept](../05_safety/safety_concept.md) |
| LS-SEC-001 | [Cybersecurity concept](../06_security/security_concept.md) |
| LS-SRS-001 | System requirements (`docs/02_system/system_requirements.md`) |
| LS-SAIC-001 | System architecture and interface contract (`docs/02_system/LS-SAIC.md`) |
