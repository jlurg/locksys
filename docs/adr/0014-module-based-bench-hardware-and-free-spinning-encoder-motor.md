---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# Module-based bench hardware and free-spinning encoder motor for stage A

## Context and Problem Statement

The project's focus is embedded software. The bench must still provide a real window drive and a real door lock actuator, with automotive-grade drivers, CAN between the nodes and a temperature sensor. An earlier bench concept used two discrete H-bridge carriers, a geared motor driving a lead-screw slide with limit switches, discrete signal conditioning and a breadboard. That concept needs mechanical work and electronics effort, and stall detection through the H-bridge current sense is inaccurate at the low currents of a small motor. How is the bench hardware built, and which window drive is used first?

## Decision Drivers

- No breadboard, no loose through-hole components, minimal soldering.
- Automotive-grade H-bridge with fault reporting.
- Stall detection that does not depend on current-sense accuracy.
- A path to the original requirement (stop at the end positions) through software stages.
- Bench electrical safety.
- Low cost and parts that can be bought as modules.

## Considered Options

- Module bench: Pololu Dual VNH5019 Motor Driver Shield on the NUCLEO, CAN transceiver boards, a TMP117 breakout and a free-spinning 12 V encoder gear motor (JGB37-520)
- Discrete bench: two VNH5019 carriers, conditioning on a breadboard, geared motor with a lead-screw slide and limit switches
- Stepper motor with a TMC2209 driver
- Worm-gear DC motor with a lower-current H-bridge
- A real window regulator motor
- A linear actuator with internal limit switches

## Decision Outcome

Chosen option: "Module bench with a free-spinning encoder gear motor", because it removes the breadboard and the mechanics, keeps an automotive-grade H-bridge for both loads, and the encoder detects stall and wrong direction without relying on current sensing.

Hardware:

- Pololu Dual VNH5019 Motor Driver Shield (#2507) on the Arduino headers of the NUCLEO-F103RB: channel M1 drives the window motor, channel M2 the door lock actuator. Jumper JP9 is never fitted; the NUCLEO is powered from USB only.
- Window drive: JGB37-520, 12 V, gear ratio 1:60, with a Hall quadrature encoder, mounted on a bracket and turning freely (no load, no end stops).
- Door lock: 12 V five-wire automotive actuator on M2 with its built-in position switch read by the DCU.
- CAN: Waveshare SN65HVD230 boards at both bus ends (fitted 120 Ω termination); the USB-CAN adapter sits in the middle with its termination off.
- Temperature: Adafruit TMP117 breakout (#4821) with a STEMMA QT cable to male pins (#4209).
- Power: 12 V supply limited to 3 A, 7.5 A blade fuse, lever connectors; optional 0 to 25 V divider module for supply sensing.
- The lock fault line (EN/DIAG) uses option A (PA6, injection current limited by the shield resistors, to be confirmed against the datasheet in M1); option B (cut the shield trace and wire to PB4) remains available.

Window behaviour by stage:

| Stage | Content | Type |
| --- | --- | --- |
| A (MVP) | UP and DOWN only while the APP button is held; no position limits | Software |
| B | Virtual limits from encoder counts: configurable travel, position 0 to 100 %, stop at 0 % and 100 % | Software |
| C | PI speed control with ramps | Software |
| D | Physical limit switches | Simple hardware |
| E | Anti-pinch from speed and current | Software and calibration |

Stage A supervision: `NO_MOTION` (commanded duty of at least 25 %, after a start grace time of 200 to 300 ms, fewer than 10 to 15 % of the expected counts within 100 ms) and `DIR_MISMATCH` (counts opposite to the command). The current sense serves only as a gross over-current backstop (about 2.0 to 2.5 A filtered over 50 ms), together with the EN/DIAG fault inputs. A PWM ramp of about 200 ms limits inrush; a press runs for at most 8 s (calibratable).

### Consequences

- Good, because the bench needs about 38 solder joints and no breadboard.
- Good, because stages B and C need software only.
- Good, because stall and encoder failures are detected by counts, independent of current-sense accuracy.
- Bad, because the requirements that need end positions move to stages B and D, and a motion plausibility requirement is added; the system requirements record these changes.
- Bad, because a free-spinning motor has no realistic load profile.
- Bad, because the shield's EN/DIAG lines cannot disable a bridge; firmware stops with PWM 0 (coast) or brakes with INA = INB = 0 and PWM 1.

### Confirmation

- The bench safety checklist in the [bill of materials](../00_project/bom.md) is completed before the 12 V supply is first switched on.
- The M2 exit criterion: the window motor turns while the CAN command is held and stops when it is released; the HIL tests cover `NO_MOTION` and `DIR_MISMATCH`.

## Pros and Cons of the Options

### Module bench with a free-spinning encoder gear motor

- Good, because of minimal hardware effort and a staged path to the full requirement.
- Neutral, because the motor turns freely; a simple disc on the shaft shows the motion.

### Discrete bench with lead-screw slide and limit switches

- Good, because it reproduces a window with physical end stops from the start.
- Bad, because of mechanical fabrication, breadboard wiring and higher fault risk.

### Stepper motor with TMC2209

- Bad, because a stall does not raise the current, so open-loop stall detection is not possible, and a different driver would be needed.

### Worm-gear DC motor

- Good, because it is self-locking like a real window drive.
- Bad, because its current falls in the range where the VNH5019 current sense is inaccurate, so a different driver would be needed.

### Real window regulator motor

- Bad, because its stall current (15 to 30 A) exceeds what the bench supply and wiring are designed for.

### Linear actuator with internal limit switches

- Bad, because internal limit switches hide the end-position behaviour the firmware must implement.

## More Information

- [Bill of materials](../00_project/bom.md)
- [Roadmap](../00_project/roadmap.md)
- [System architecture and interface contract](../02_system/LS-SAIC.md)
- [Pololu Dual VNH5019 Motor Driver Shield](https://www.pololu.com/product/2507)
