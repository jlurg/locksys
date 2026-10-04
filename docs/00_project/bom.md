# Bench and HIL bill of materials

| Field | Value |
| --- | --- |
| Document ID | LS-PRJ-003 |
| Version | 1.1 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This document lists the hardware of the DCU and CGW nodes, the bench power and wiring, the window motor mounting and the HIL bench equipment. It states the purchase requirements for the USB-CAN adapter, the recommendation for the stimulus MCU, the buying checklist for the window motor and the bench safety checklist.

The pin allocation of the DCU is defined in the [system architecture and interface contract](../02_system/LS-SAIC.md); the HIL setup in the [HIL architecture](../07_verification/hil_architecture.md).

## Principles

- Modules only: no breadboard and no loose through-hole components ([ADR 0014](../adr/0014-module-based-bench-hardware-and-free-spinning-encoder-motor.md)).
- One soldering session: the headers and terminal blocks of the driver shield, about 38 joints.
- 12 V only on terminals and cables.

Status values: **To buy** (in the purchase list below), **Available** (already owned), **Optional**, **To confirm** (ownership or model to be confirmed).

## DCU node

| ID | Item | Part or variant | Qty | Notes | Status |
| --- | --- | --- | --- | --- | --- |
| D1 | MCU board | ST NUCLEO-F103RB, board MB1136 revision C-02 or later | 1 | Revision C-02 or later provides the 8 MHz ST-LINK clock to the HSE input and the LSE crystal; check the label on the bottom side (Lab Host day-1 check D6) | Available |
| D2 | H-bridge driver | Pololu Dual VNH5019 Motor Driver Shield for Arduino (#2507) | 1 | Plugs onto the NUCLEO Arduino headers; stackable headers pass every signal to the top; M1 drives the window motor, M2 the lock actuator; two automotive VNH5019 (12 A continuous, 30 A peak per channel); current sense about 140 mV/A | To buy |
| D3 | Window motor | JGB37-520 gear motor, 12 V, ratio 1:60, Hall quadrature encoder, 6 wires | 2 | One spare; see the buying checklist | To buy |
| D4 | Motor mounting | 37 mm L bracket, 6 mm D-shaft flange coupling, light disc of 60 to 80 mm with a radial stripe, base board | 1 set | Nothing that can wind onto the shaft | To buy |
| D5 | Motor suppression capacitor | 100 nF ceramic capacitor across the motor terminals | 1 | Only if the motor has none fitted | Optional |
| D6 | Door lock actuator | 12 V automotive central-locking actuator, 5 wires, with built-in position switch | 1 | Motor on shield channel M2; position switch read on PB12 with internal pull-up | To buy |
| D7 | Temperature sensor | Adafruit TMP117 breakout (#4821) | 1 | I2C address 0x48; 10 kΩ pull-ups on the breakout; powered from 3.3 V | To buy |
| D8 | Sensor cable | Adafruit STEMMA QT to male header cable (#4209) | 1 | Male pins plug into the stackable top sockets of the shield | To buy |
| D9 | CAN transceiver board | Waveshare SN65HVD230 CAN Board | 1 | 3.3 V; 120 Ω termination fitted, so the board sits at a bus end | To buy |
| D10 | Supply voltage sense | 0 to 25 V voltage divider module (30 kΩ and 7.5 kΩ, ratio 1/5) | 1 | 12 V gives 2.4 V and 16.5 V gives 3.3 V on PA4; not protected against reverse polarity. Required on the HIL bench, where SYS-039 and SYS-043 are verified (HWR-008); optional on a desk setup without supply monitoring | To buy |

The lock actuator fault line (EN/DIAG) uses option A by default: shield pin D12 (PA6) with the injection current limited by the shield resistors, to be confirmed against the STM32F103 datasheet in M1. Option B cuts the D12 trace on the shield and wires the signal to D5 (PB4); it needs one jumper wire and no extra part.

## CGW node

| ID | Item | Part or variant | Qty | Notes | Status |
| --- | --- | --- | --- | --- | --- |
| C1 | MCU board | Espressif ESP32-S3-DevKitC-1-N8R8, board v1.1 | 1 | On board v1.0 the RGB LED is on another GPIO | Available |
| C2 | CAN transceiver board | Waveshare SN65HVD230 CAN Board | 1 | 120 Ω termination fitted; the other bus end | To buy |

## CAN bus

| ID | Item | Part or variant | Qty | Notes | Status |
| --- | --- | --- | --- | --- | --- |
| B1 | Bus cable | Twisted pair for CANH and CANL, plus a ground reference conductor | As needed | Short stubs to the boards | To buy |
| B2 | USB-CAN adapter | Per the purchase requirements below | 1 | Placed in the middle of the bus with its termination off | To buy |

Topology: DCU transceiver board (120 Ω) — USB-CAN adapter (termination off) — CGW transceiver board (120 Ω). With the bus unpowered, 60 Ω is measured between CANH and CANL. The Rs pin of both boards is tied to ground through 10 kΩ, so the transceivers are always active; the MVP has no transceiver standby control.

## Bench power and wiring

| ID | Item | Part or variant | Qty | Notes | Status |
| --- | --- | --- | --- | --- | --- |
| P1 | Bench power supply | Programmable supply with at least two independently programmable outputs: CH1 ≥ 20 V / ≥ 5 A (actuator power), CH2 ≥ 18 V (KL30 sense emulation in HIL-SIM); per-channel current limit and over-voltage protection; output off at power-on; voltage and current read-back; remote control over LAN (SCPI); floating outputs | 1 | CH1 set to 12.0 V with a 3 A limit (5 A only for a stalling lock test); CH2 set to 12.0 V with a 0.1 A limit and 18.0 V OVP ([HIL architecture](../07_verification/hil_architecture.md) §6.3). Alternative: a single-output CH1 supply plus a second SCPI-controlled single-output supply of at least 18 V for CH2, on the same instrument LAN | To confirm |
| P2 | Main fuse | Inline blade fuse holder with a 7.5 A blade fuse | 1 | Between supply positive and the shield VIN terminal | To buy |
| P3 | Lock fuse | Blade fuse 5 A in the lock actuator lead | 1 | Additional protection of the lock branch | Optional |
| P4 | Lever connectors | WAGO 221 series | Assorted | 12 V and ground distribution | To buy |
| P5 | Power wire | 18 AWG silicone wire, red and black | A few metres | Supply, motor and actuator leads | To buy |
| P6 | Jumper wires | Dupont male-to-female and female-to-female | About 15 | About 13 logic connections | To buy |

Power path: supply positive → 7.5 A fuse → lever connector → shield VIN terminal; supply negative → shield GND terminal; window motor → M1A and M1B; lock actuator → M2A and M2B. Motor return current flows only through the shield GND terminal. The NUCLEO is powered from USB only.

## HIL bench equipment

| ID | Item | Part or variant | Qty | Notes | Status |
| --- | --- | --- | --- | --- | --- |
| H1 | Lab Host PC | Windows 11 Pro, USB 3 ports, Ethernet | 1 | Runs IAR, the runners and the HIL framework ([Lab Host](../08_process/lab_host.md)) | Available |
| H2 | USB-CAN adapter | Same unit as B2 | (1) | Restbus in topology T1 and T2, listen-only monitor in T3 | To buy |
| H3 | Stimulus MCU board | NUCLEO-144 board; see the recommendation below | 1 | Model confirmed at the start of M5 | Available |
| H4 | Logic analyser | Saleae logic analyser | 1 | The channel count determines the capture profiles | Available |
| H5 | Oscilloscope and multimeter | General-purpose bench instruments | 1 each | Manual procedures: CAN physical layer, PWM, motor current, lock pulse | To confirm |
| H6 | USB Wi-Fi adapter | WPA3-SAE capable (H2E preferred), with a driver for Windows 11 | 1 | The Lab Host joins the CGW access point as the APP simulator; Ethernet keeps the default route | To buy |
| H7 | Relay module | One-channel relay module with 3.3 V compatible input | 1 | Shorts CANH to CANL for bus-off tests; the short can also be made by hand | Optional |
| H8 | Jumper wires | Dupont wires | Set | Stimulus MCU to the shield top sockets and the NUCLEO morpho pins | To buy |
| H9 | I2C pull-up board for HIL-SIM | Second Adafruit TMP117 breakout (#4821) with its address set to 0x49 (address solder jumper), with a STEMMA QT cable (#4209) | 1 | Plugged in place of the real sensor in HIL-SIM: provides the same 10 kΩ pull-ups and bus capacitance, and does not answer at 0x48, where the stimulus MCU emulates the sensor | To buy |
| H10 | Switchable USB hub | USB hub with per-port power switching controllable from Windows | 1 | Power cycling of the DCU and CGW boards (capabilities `power.dcu_usb`, `power.cgw_usb`); tests skip the affected sub-cases without it | Optional |

There is no interface board in the MVP. In HIL-SIM the real encoder is disconnected and the stimulus MCU generates the A and B signals on PA15 and PB3 from the commanded PWM and direction; its DAC outputs inject the current-sense voltages on A0 and A1, which the 10 kΩ series resistors of the shield allow while the driver has no supply; the real TMP117 is replaced by the pull-up board H9 and the emulated sensor, never both at 0x48; the actuator supply is switched by output CH1 of the bench supply, and output CH2 feeds the KL30 sense module with SCPI-controlled ramps.

## USB-CAN adapter: purchase requirements

| Requirement | Reason |
| --- | --- |
| CAN 2.0A and 2.0B at 500 kbit/s with configurable bit timing (prescaler, segments and SJW, or sample point) | Match the 87.5 % sample point of the nodes |
| Maintained Windows 11 driver and support in python-can | The HIL framework runs on the Lab Host |
| Hardware timestamps with a resolution of 1 µs or better | Latency measurements and jitter of cyclic frames |
| Galvanic isolation between USB and CAN | Bench with 12 V loads and a motor: avoids ground loops and protects the PC |
| Listen-only mode and reporting of error frames and bus-off | Passive monitoring (topology T3), bus-off and E2E tests |
| Switchable or known 120 Ω termination | The bench defines the bus ends |
| Desirable: CAN FD, macOS support, free monitoring software | Future use and use from the Mac |

Candidates:

1. **PEAK PCAN-USB FD** (IPEH-004022 with USB-A, IPEH-004023 with USB-C). Recommended: meets every requirement; isolated; 1 µs timestamps; python-can interface `pcan`; PCAN-View; macOS support through MacCAN.
2. **Kvaser U100 or Leaf v3.** Good Windows support (CANlib, python-can interface `kvaser`), no official macOS driver; confirm that the chosen model is isolated.
3. **CANable 2.x or candleLight (gs_usb).** Low-cost option for monitoring; usually not isolated and without precise timestamps (latencies would be measured with the logic analyser); needs a WinUSB driver on Windows.

Avoid adapters that use a proprietary serial protocol without python-can support.

## Stimulus MCU recommendation

Requirements:

- At least 20 GPIOs at 3.3 V, including at least 4 open-drain outputs on 5 V-tolerant (FT) pins: NRST, CGW EN and BOOT, and the EN/DIAG lines, which the shield pulls up to 5 V.
- I2C target mode with clock stretching, to emulate the TMP117 including one-shot conversions, data-ready, NACK and stuck SDA faults.
- Two DAC channels to inject the window and lock current-sense voltages.
- Timers with input capture at 10 MHz or more to measure PWM and direction.
- Native USB device (CDC).
- Cortex-M4 or Cortex-M7.
- Professional toolchain; no Arduino framework.

Order of preference:

1. **NUCLEO-144 (F429ZI, F446ZE, F746ZG, F767ZI, H743ZI2 or H723ZG). Recommended.** Same ecosystem as the DCU with STM32CubeIDE already installed; USB OTG full speed with its own connector; two DAC channels; I2C target mode; many timers; CAN for a future fault-injection node. Being test equipment, its firmware may use the ST HAL and LL drivers.
2. **NUCLEO-64 G474RE or G491RE.** DACs, high-resolution timer and FDCAN; USB needs a breakout on PA11 and PA12, otherwise the ST-LINK virtual COM port is used.
3. **nRF52840-DK.** Native USB, PPI-based timestamps and I2C target mode; no DAC (an external dual DAC is needed) and its radio must stay off near the CGW access point; Zephyr toolchain.
4. LaunchPad or Curiosity boards only if they meet every requirement; they offer no advantage over a NUCLEO.

The stimulus firmware has a portable core (command protocol, plant models, TMP117 emulator) and one port layer per board.

## Window motor: JGB37-520 buying checklist

Variants at 12 V (manufacturer data for the Hiwonder 520 encoder motor; currents for other vendors are unverified):

| Ratio | No-load speed | Counts per output revolution (x4) | Rated current | Stall current |
| --- | --- | --- | --- | --- |
| 1:30 | 320 rpm | 1320 | 0.36 A | 3.2 A |
| 1:56 (Yahboom MD520) | 205 rpm | 2464 | Not published | Not published |
| **1:60 (recommended)** | 170 rpm | 2640 | 0.53 A | 3.2 A |
| 1:90 | 110 rpm | 3960 | 0.36 A | 3.2 A |

Counts per output revolution are 44 times the gear ratio (11-pole magnetic ring, two Hall sensors, x4 decoding). The actual value is calibrated on the bench by turning the output shaft 10 turns by hand.

Checklist for a listing:

- [ ] Rated voltage 12 V (not 6 V or 24 V).
- [ ] Encoder with 6 wires (M+, M−, VCC, GND, A, B).
- [ ] 11 lines (pulses per motor revolution) stated.
- [ ] Encoder supply range including 3.3 V (3.3 to 5 V).
- [ ] Encoder board silkscreen visible in the photos.
- [ ] Ratio 1:60 (alternatively 1:56, 1:90 or 1:30).
- [ ] Two units ordered.

Wiring and expected values:

- Motor leads go to the shield terminals; the encoder is powered from the NUCLEO 3.3 V, and A and B go to the encoder inputs defined in LS-SAIC.
- Follow the silkscreen, not the wire colours; the motor pair measures a few ohms with a multimeter.
- Expected currents at 12 V: free-running about 0.1 to 0.3 A (unverified), loaded up to about 0.5 A, inrush and stall 3.2 to 3.9 A; 5 A is assumed for units of unknown origin.

## Bench safety checklist

Completed before the 12 V supply output is switched on for the first time, and after any wiring change:

- [ ] Jumper JP9 (ARDVIN=VOUT) on the shield is not fitted; nothing is connected to NUCLEO VIN or E5V; the NUCLEO is powered from USB only.
- [ ] The supply is set to 12.0 V with a 3 A current limit (5 A only for a stalling lock test).
- [ ] The 7.5 A main fuse is fitted.
- [ ] Polarity is checked before the output is enabled: the shield tolerates reverse supply down to −16 V, the voltage divider module does not.
- [ ] Motor return current flows only through the shield GND terminal.
- [ ] The firmware never drives EN/DIAG high and never relies on EN/DIAG to stop a motor.
- [ ] The lock pulse has a hard timeout in firmware.
- [ ] The motor is fixed on its bracket; the shaft is not touched while it runs.
- [ ] The supply output is off while anything is connected or disconnected.
- [ ] While the CPU is halted by the debugger with a motor connected, the supply output is off or its current limit is at most 0.5 A.

## Purchase list for M1 and M2

| Item | Qty |
| --- | --- |
| Pololu Dual VNH5019 Motor Driver Shield (#2507) | 1 |
| JGB37-520, 12 V, 1:60, with encoder | 2 |
| 37 mm bracket and 6 mm flange coupling | 1 |
| 12 V automotive lock actuator, 5 wires | 1 |
| Waveshare SN65HVD230 CAN Board | 2 |
| Adafruit TMP117 (#4821) and STEMMA QT cable (#4209) | 1 each |
| 0 to 25 V voltage divider module (required for the HIL bench) | 1 |
| Blade fuse holder and 7.5 A fuse | 1 |
| WAGO 221 lever connectors | Assorted |
| 18 AWG wire | As needed |
| Dupont jumper wires, male-to-female and female-to-female | As needed |
| USB-CAN adapter per the purchase requirements | 1 |
| USB Wi-Fi adapter with WPA3-SAE (needed from M3) | 1 |

The NUCLEO-144 used as stimulus MCU is already available.

## Purchase list for M5 (HIL bench)

| Item | Qty |
| --- | --- |
| Bench supply with two programmable outputs and SCPI over LAN (P1), unless the owned supply meets the requirements | 1 |
| Adafruit TMP117 (#4821) set to address 0x49, with a STEMMA QT cable (#4209) (H9) | 1 each |
| Dupont jumper wires for the stimulus MCU (H8) | Set |
| Relay module for bus-off tests (H7, optional) | 1 |
| Switchable USB hub (H10, optional) | 1 |

## References

- [ADR 0014: Module-based bench hardware and free-spinning encoder motor for stage A](../adr/0014-module-based-bench-hardware-and-free-spinning-encoder-motor.md)
- [System architecture and interface contract](../02_system/LS-SAIC.md)
- [HIL architecture](../07_verification/hil_architecture.md)
- [Manual test procedures](../07_verification/procedures/README.md)
- [Pololu Dual VNH5019 Motor Driver Shield (#2507)](https://www.pololu.com/product/2507)
- [Pololu Dual VNH5019 Motor Driver Shield user's guide](https://www.pololu.com/docs/0J49/all)
- [Waveshare SN65HVD230 CAN Board](https://www.waveshare.com/wiki/SN65HVD230_CAN_Board)
- [Adafruit TMP117 (#4821)](https://www.adafruit.com/product/4821)
- [Hiwonder 520 encoder gear motor](https://www.hiwonder.com/products/hall-encoder-dc-geared-motor)
