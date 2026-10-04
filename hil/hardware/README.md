# HIL bench hardware

Module-based bench (no interface board, no breadboard): the stimulus MCU connects with Dupont jumpers to the stackable top sockets of the Pololu Dual VNH5019 shield #2507 and to the NUCLEO-F103RB morpho pins (LS-HIL-001 section 6).

| File | Content |
|---|---|
| `wiring/wiring.csv` | DCU module wiring: every DCU pin, its peripheral, module terminal and access point (LS-SAIC-001 section 3.1) |
| `wiring/stimulus_sim.csv` | Stimulus connections for HIL-SIM and the shared monitoring taps (LS-HIL-001 section 6.2) |
| `../config/profiles/logic.yaml` | Logic-analyser channel maps and profiles A-D (LS-HIL-001 section 8.2) |

Rules:

- The tables are authoritative for the bench build; morpho pin numbers are checked against UM1724 and the board silkscreen at bring-up (OP-HIL-09).
- A change of wiring updates these tables in the same pull request and requires a new bench qualification report.
- 12 V wiring uses terminal blocks and 18 AWG cable only; logic-analyser probes go on 3.3 V MCU-side nets only.
