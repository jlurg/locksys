# Stimulus MCU firmware

| Field | Value |
|---|---|
| Status | Requirements and protocol draft; firmware in M5 |
| Board | Fixed at the start of M5 (OP-HIL-02) and recorded here |
| Protocol | SCPI-like command protocol, version 1.0 (draft) |

## 1. Board requirements

| ID | Requirement | Reason |
|---|---|---|
| ST-01 | ≥ 20 GPIO at 3.3 V, including ≥ 4 open-drain outputs on 5 V-tolerant pins | NRST, CGW EN and BOOT; EN/DIAG lines pulled to 5 V by the shield |
| ST-02 | I2C target mode with controllable clock stretching | TMP117 emulation with one-shot, Data_Ready, NACK and stuck-SDA faults |
| ST-03 | 2 DAC channels, 12 bit | Window and lock current-sense emulation |
| ST-04 | Timer input capture ≥ 10 MHz on 2 channels; quadrature generation ≥ 10 k edges/s | PWM duty and direction; encoder emulation (7.5 k counts/s nominal) |
| ST-05 | ≥ 2 ADC inputs | DCU 3V3 and KL30-sense monitoring |
| ST-06 | Native USB device (CDC) | Host link independent of a debug-probe VCP |
| ST-07 | Cortex-M4 or Cortex-M7 with FPU | Plant models at 10 kHz |
| ST-08 | Professional toolchain without Arduino; vendor HAL/LL allowed | Test equipment, not product code |

## 2. Recommendation

NUCLEO-144 (F429ZI, F446ZE, F746ZG, F767ZI, H743ZI2 or H723ZG): same ecosystem as the DCU, USB OTG FS on its own connector, two DAC channels, I2C target, many timers and a CAN controller for a future fault-injection node. Alternatives: NUCLEO-G474RE (USB through a breakout on PA11/PA12) and nRF52840-DK (external DAC, radio off near the SoftAP).

## 3. Firmware architecture

- Portable core (command parser, plant models, TMP117 register model) with a port layer per board; host unit tests for the core.
- 10 kHz control loop for the plant models; 1 µs event timestamps.
- SYNC output with width-coded pulses: 10 µs host marker, 20 µs injected event, 50 µs power or reset event, 100 µs KL30 threshold crossing, 200 µs self-test pattern.
- Hardware watchdog; host heartbeat: without `SYST:HB` for 2 s all SIM outputs are released and the relay output is cleared.
- Outputs are high-impedance after reset until the host selects HIL-SIM; DAC outputs are disabled while DCU 3V3 is below 3.0 V.
- `BOOT:CGW:HOLD` is refused while CGW EN is low; GPIO0 is never low when EN is released.

## 4. Command protocol 1.0 (draft)

**Transport.** USB CDC interface 0 (control) and interface 1 (events). ASCII lines terminated by LF (CR ignored), ≤ 128 bytes, case-insensitive, `;` separates compound commands.

**Responses.** A query (ending in `?`) returns exactly one line. A command returns nothing; the host confirms with `*OPC?`. Anything that returns data is a query.

**Errors.** SCPI error queue read with `SYST:ERR?` (`0,"No error"` when empty). Standard codes −100 (command error), −113 (undefined header), −222 (data out of range); device codes +101 emergency stop active, +102 DUT unpowered, +103 REAL configuration (SIM outputs disabled), +104 BOOT hold refused while CGW EN is low.

**Versioning.** `SYST:VERS?` returns `MAJOR.MINOR`; the host requires an equal major version. Minor versions only add commands, discovered with `SYST:CAP?`.

**Events.** `!EVT,<t_us>,<type>[,<key>=<value>]…` on the event interface, for example `!EVT,18446012,SYNC,W=10`, `!EVT,18450120,WIN_DRV,STATE=OFF,ON_US=2003410`, `!EVT,…,LOCK_PULSE,W_US=300120`.

| Group | Commands | Function |
|---|---|---|
| IEEE 488.2 | `*IDN?`, `*RST`, `*TST?`, `*OPC?`, `*CLS` | Identity, defaults (all outputs released), self-test, completion, clear errors |
| SYST | `SYST:VERS?`, `SYST:CAP?`, `SYST:ERR?`, `SYST:TIME?`, `SYST:HB` | Version, capabilities, error queue, µs clock, heartbeat |
| MODE | `MODE SIM`, `MODE REAL`, `MODE?` | Configuration; REAL releases encoder, lock-switch and DAC outputs |
| PLANT | `PLANT:WIN:CONF <cpr>,<cps>`, `PLANT:WIN:INJ STALL\|DIR_MISMATCH\|NONE`, `PLANT:WIN?`, `PLANT:LOCK:CONF <stroke_ms>`, `PLANT:LOCK:INJ FB_STUCK\|SLOW\|BOUNCE\|REVERSED\|NONE` | Window and lock plant models |
| TEMP | `TEMP:EMU ON\|OFF`, `TEMP:VAL <degC>`, `TEMP:RAMP <from>,<to>,<degC_per_s>`, `TEMP:FAULT <kind>[,<n>]`, `TEMP:STAT?` | TMP117 emulator at 0x48 |
| SOUR | `SOUR:CS WIN\|LOCK,<amps>` | Current-sense injection through the DACs |
| DIO | `DIO:DIAG WIN\|LOCK,FAULT\|OK` | EN/DIAG open-drain emulation |
| RST | `RST:DCU:PULSE <ms>`, `RST:CGW:HOLD ON\|OFF`, `BOOT:CGW:HOLD <ms>` | Resets and CGW BOOT with interlock |
| REL | `REL:CANSHORT ON\|OFF` | Optional CANH-CANL short relay |
| MEAS | `MEAS:DCU3V3?`, `MEAS:KL30?`, `MEAS:WIN:LAST?`, `MEAS:COUNT?` | Monitors and captures |
| SYNC | `SYNC:PULSE? <width_us>` | Emit a SYNC pulse and return its µs timestamp |

Example session:

```text
> *IDN?
< LOCKSYS,HIL-STIM,<board serial>,1.0.0+g3f9a2c1
> SYST:VERS?
< 1.0
> MODE SIM;:PLANT:WIN:CONF 2640,7480
> SYNC:PULSE? 10
< 18446012
> SYST:ERR?
< 0,"No error"
```
