# LockSys Manual Test Procedures

| Field | Value |
|---|---|
| Document ID | LS-VER-002 |
| Version | 0.2 |
| Status | Draft |
| Owner | jlurg |

## 1. Purpose and scope

This index lists the manual and semi-automatic bench procedures (TST-MAN-SYS-nnn) and the manual APP platform procedures (TST-MAN-APP-nnn) of LockSys stage A. Manual procedures cover physical-layer measurements, calibrations and one-time hardware checks that the automated HIL suites do not cover (see [LS-VER-001](../verification_strategy.md) §4 and [LS-HIL-002](../hil_test_catalog.md)).

- Procedure bodies with record templates live in `hil/procedures/TST-MAN-SYS-nnn.md` and `hil/procedures/TST-MAN-APP-nnn.md`; completed records are kept with the bench qualification records (`hil/qualification/`) or attached to the release evidence.
- Every procedure follows the bench safety rules of [LS-HIL-001](../hil_architecture.md) §12.

## 2. Procedure structure

Each procedure body contains:

1. Purpose and the requirements or parameters it verifies.
2. Preconditions and safety notes (bench configuration, PSU settings, attendance).
3. Equipment and instrument settings.
4. Steps.
5. Acceptance criteria.
6. Record template: date, operator, bench revision, DUT identities and firmware versions, instrument identities and calibration due dates, measured values, verdict.

## 3. Index

| ID | Title | Verifies or supports | Milestone |
|---|---|---|---|
| TST-MAN-SYS-001 | CAN bus resistance | LS-SAIC-001 CAN physical rules; bench qualification | M1 |
| TST-MAN-SYS-002 | CAN differential levels | LS-SAIC-001 CAN physical rules | M1 |
| TST-MAN-SYS-003 | CAN bit time and sample point | SYS-026 (precondition) | M1 |
| TST-MAN-SYS-004 | PWM frequency and duty | SWR-level PWM settings; soft start | M2 |
| TST-MAN-SYS-005 | Window motor current | SYS-031 calibration; CS plausibility | M2 |
| TST-MAN-SYS-006 | Lock pulse | SYS-003, SYS-034 | M2 |
| TST-MAN-SYS-007 | Supply current per mode | Power budget; LS-SAIC-001 power domains | M2 |
| TST-MAN-SYS-008 | I2C rise time and levels | SYS-008 (precondition); SIM/REAL equivalence | M2 |
| TST-MAN-SYS-009 | UART baud rate | SYS-009, SYS-090 (precondition) | M1 |
| TST-MAN-SYS-010 | KL30 sense accuracy | SYS-043; SYS-039 test margins | M2 |
| TST-MAN-SYS-011 | Temperature accuracy | SYS-008 | M2 |
| TST-MAN-SYS-012 | Reverse supply polarity | SYS-080 | M2 |
| TST-MAN-SYS-013 | WPA3-SAE and PMF beacon capture | SYS-053 | M3 |
| TST-MAN-SYS-014 | HSE and clock security failure | SYS-038 (clock part) | M6 |
| TST-MAN-SYS-015 | Encoder counts-per-revolution calibration | SYS-042 parameters (`enc_cpr`) | M2 |
| TST-MAN-SYS-016 | Module wiring and bench safety inspection | SYS-081; bench safety rules | Bench build |
| TST-MAN-SYS-017 | Release-to-stop with a phone | SYS-020 (APP share) | M4 |
| TST-MAN-APP-003 | WPA3-SAE SoftAP join from phones (iOS decision gate L3) | SYS-053; SWR-APP-011; LS-SAIC-001 §8 (L3) | WP0; repeated M4 |
| TST-MAN-APP-010 | Screen-reader hold pass-through | SWR-APP-030, SWR-APP-034, SWR-APP-061; STK-005 | M4 |

## 4. Scope of each procedure

### TST-MAN-SYS-001 CAN bus resistance

With all nodes unpowered, measure the resistance between CANH and CANL at the DCU end of the trunk with a DMM (2-wire). The two Waveshare boards terminate the ends with 120 Ω each and the USB-CAN adapter sits mid-bus with its termination off, so the expected value is 60 Ω ± 5 % (57–63 Ω). A reading near 120 Ω or 40 Ω identifies a missing or an extra termination. The procedure is repeated after any change of the bus wiring and is part of the bench qualification.

### TST-MAN-SYS-002 CAN differential levels

Measure CANH and CANL with two oscilloscope channels (10:1 probes, DC coupling, 20 MHz bandwidth limit, math CANH − CANL), triggered on the falling edge of CANL, once with each node transmitting while the other node is silenced. Acceptance: dominant differential voltage 1.5–3.0 V, recessive differential voltage −120 mV to +12 mV and recessive common mode near 2.3 V (SN65HVD230 data sheet). The oscilloscope ground clip goes to the PSU negative only.

### TST-MAN-SYS-003 CAN bit time and sample point

Capture the DCU CAN RX line with the logic analyser at ≥ 100 MS/s and measure the time between two recessive-to-dominant edges at least 50 bits apart. Acceptance: bit time 2.000 µs ± 0.1 % (the tolerance budget of the 8-tq configuration is 0.485 %). Read back the DCU bit-timing register through SWD (expected `CAN_BTR` = 0x00050008), check the CGW boot log for BRP 10, TSEG1 13, TSEG2 2, SJW 2, and record the adapter configuration (87.5 % sample point).

### TST-MAN-SYS-004 PWM frequency and duty

Measure the window PWM (PC7) and the lock PWM (PB6) with the logic analyser or the oscilloscope. Acceptance: 20.00 kHz ± 0.1 %; duty within ± 0.5 % of the commanded value at 25 %, 50 % and 100 %; the window soft-start ramp reaches the commanded duty after `t_softstart_ms` ± 20 %. The PWM lines are measured on the MCU side of the shield's series resistors.

### TST-MAN-SYS-005 Window motor current

With the actuator supply at 12.0 V and the 3 A current limit, measure the window motor current with a DMM in series (steady state) and with a current clamp or a differential probe across a shunt (start-up), for both directions. Record the free-running current, the start-up peak with the soft start enabled, and the CS voltage reported by the DCU against the measured current (expected ≈ 0.140 V/A, accuracy limited at low current). The shaft is never blocked by hand; the stall current is taken from the vendor data (3.2 A) until a stall fixture exists. The results confirm that `i_oc_backstop_ma` lies above the start-up peak and below the PSU limit.

### TST-MAN-SYS-006 Lock pulse

Measure the voltage across the lock actuator (differential probe) or its current (clamp) with a single trigger on the lock PWM, together with the position-switch line. Acceptance: pulse 300 ± 5 ms in the commanded direction; the switch settles within 70 ms after the pulse; peak current and supply droop recorded; no pulse longer than 500 ms, including the retry case. The values feed the lock calibration and the HIL plant parameters.

### TST-MAN-SYS-007 Supply current per mode

Measure the current drawn from the actuator supply with the PSU read-back and a DMM in series, in the modes idle, window driving, lock pulsing and SAFE, and the 3.3 V load of the NUCLEO (STM32 current through the IDD jumper; external module load through a DMM in series where the board allows it). Acceptance: actuator supply current with both bridges off below the bench target (recorded baseline ± 20 %); external 3.3 V load ≤ 150 mA (NUCLEO regulator budget).

### TST-MAN-SYS-008 I2C rise time and levels

Measure SCL and SDA with a 10:1 probe (≤ 15 pF) at 200 ns/div, in HIL-REAL (real TMP117) and in HIL-SIM (emulator with the substitute pull-ups). Acceptance: 30 % to 70 % rise time ≤ 1000 ns (standard mode; target ≤ 300 ns); low level ≤ 0.4 V; SIM and REAL rise times within 10 % of each other, so that the emulator reproduces the bus conditions.

### TST-MAN-SYS-009 UART baud rate

Capture the DCU USART2 TX line with the logic analyser and measure the duration of N consecutive bits. Expected: 115 384.6 baud (BRR 19.5 at 36 MHz, +0.16 % from 115 200); acceptance: error ≤ ± 1 %. The telemetry parser of the HIL framework must decode 10 min of output without checksum errors.

### TST-MAN-SYS-010 KL30 sense accuracy

Set the supply feeding the KL30 sense module to 8.0, 9.0, 12.0, 16.0 and 16.5 V, measure the module input with the DMM and read the DCU's reported supply voltage (`DcuSts_Vbat`, DID 0xFD02 and telemetry). Record the error at each point and the behaviour at ADC saturation near 16.5 V. Acceptance (SYS-043): error within ± `lim_kl30_accuracy_mv` (200 mV) at 9.0, 12.0 and 16.0 V after the 2-point calibration. The MVP has no calibration storage: the 2-point calibration at 9.00 V and 16.00 V (LS-SAIC-001 BU-09) yields the build-time constants `kl30_gain_x1000` and `kl30_offset_mv`, and the measured error confirms the margins used by TST-HIL-SYS-013 (nominal thresholds ± 0.3 V).

### TST-MAN-SYS-011 Temperature accuracy

Place the DUT TMP117 and a reference TMP117 (the 0x49 board of LS-HIL-001 §6.4) in thermal contact on an aluminium block, soak for 15 min at room temperature (20–30 °C), and record 10 samples of both. Acceptance: |DUT − reference| ≤ 0.3 °C (SYS-008). An accredited reference thermometer is optional.

### TST-MAN-SYS-012 Reverse supply polarity

With the KL30 sense module disconnected, the actuator supply limited to 0.5 A and the leads reversed at the shield input for 5 s, record the current drawn. Acceptance: ≤ 10 mA (the shield's reverse protection blocks the current); after restoring the polarity, the HIL smoke suite passes. The PSU output is off while the leads are swapped.

### TST-MAN-SYS-013 WPA3-SAE and PMF beacon capture

Capture the CGW beacon and the association of the WLAN-DUT on the SoftAP channel with a monitor-mode capable adapter or the macOS wireless diagnostics sniffer. Acceptance: AKM suite SAE; management frame protection capable and required (MFPC = MFPR = 1); no WPA1 information element and no TKIP cipher; release builds never offer WPA2/WPA3 transition mode.

### TST-MAN-SYS-014 HSE and clock security failure

Requires a one-time board rework that makes the 8 MHz clock from the ST-LINK interruptible (replacing the MCO solder bridge with a removable link). Interrupt the clock while running and power up with the clock absent. Acceptance: SAFE within 200 ms; outputs off; CAN silent (no DCU frames); telemetry continues on the internal oscillator and reports SAFE; DTC B1A53-96 reported. Scheduled for M6 together with the clock security mechanism (SM-14).

### TST-MAN-SYS-015 Encoder counts-per-revolution calibration

With the actuator supply off, mark the output shaft and turn it 10 full revolutions by hand in the UP direction, then 10 in the DOWN direction, while reading the encoder position from the DCU (diagnostic DID or telemetry). Counts per revolution = counts / 10; the expected value is 2640 for the 1:60 gearbox (44 × ratio). Record the result in `interfaces/params/timing.yaml` (`enc_cpr`), confirm the counting direction against the configured polarity, and check the encoder signal levels (3.3 V) and edge quality on the logic analyser while the motor runs at nominal speed.

### TST-MAN-SYS-016 Module wiring and bench safety inspection

Inspect the bench against the wiring tables and the bench safety rules, with photographs attached to the record: shield jumper JP9 not fitted; NUCLEO powered from USB only; 7.5 A fuse in the actuator supply; PSU current limit and output-off state at power-up; 18 AWG power wiring on terminal blocks; no breadboard and no actuator current through jumper wires (SYS-081); motor fixed in its bracket with only the indicator disc; encoder and CAN wiring per the tables; KL30 sense module polarity. Repeated after any change of the bench hardware.

### TST-MAN-SYS-017 Release-to-stop with a phone

With the real APP on an Android phone (and an iOS phone where available), perform at least 20 hold-and-release trials and measure the time from the finger leaving the screen to drive-off, using a high-speed video of the screen and a bridge-state indicator, or the CGW TRACE0 pulse and an equivalent touch marker. Acceptance: maximum ≤ 150 ms (SYS-020). The procedure covers the APP share of the budget that the APP simulator excludes.

### TST-MAN-APP-003 WPA3-SAE SoftAP join from phones

Decision gate L3, run in work package WP0 and repeated before the M4 release on an iPhone with iOS ≥ 18 and on Android 10 and Android ≥ 12 phones, against the WPA3-only SoftAP (RC configuration, no transition mode). Steps: programmatic join (`NEHotspotConfiguration` with `joinOnce` on iOS; `WifiNetworkSpecifier` on Android), manual join in Settings, Local Network prompt (iOS), then a session to the CGW with the phone's mobile data enabled, given that the DHCP offer contains no router and no DNS option. Record per phone: OS version, join method, join result and time, whether traffic to the CGW is routed over the SoftAP while mobile data stays enabled, and any system prompt shown. Acceptance: Android joins programmatically; on iOS the result decides between programmatic join and manual join in Settings (fallback 1) and is recorded in the APP architecture open-assumption OA-1; in every case the session reaches the CGW over the SoftAP and no transition mode is configured.

### TST-MAN-APP-010 Screen-reader hold pass-through

With TalkBack (Android) and VoiceOver (iOS) enabled, perform at least 10 "double-tap and hold" gestures on each window hold zone against the CGW simulator or the bench. Record whether the held gesture reaches the hold zone as a pointer down and up, the keep-alive sequence in the simulator log, and the announced labels and hints. Acceptance: motion is requested only while the gesture is held and stops on release; a plain double tap or any semantic action never starts motion and announces the hold hint; every control is labelled. The result closes or keeps open assumption OA-2 of the APP architecture.

## 5. References

| Reference | Title |
|---|---|
| LS-VER-001 | [Verification strategy](../verification_strategy.md) |
| LS-HIL-001 | [HIL architecture](../hil_architecture.md) |
| LS-HIL-002 | [HIL test catalogue](../hil_test_catalog.md) |
| LS-SAF-001 | [Functional safety concept](../../05_safety/safety_concept.md) |
| LS-SEC-001 | [Cybersecurity concept](../../06_security/security_concept.md) |
| LS-SAIC-001 | System architecture and interface contract (`docs/02_system/LS-SAIC.md`) |
