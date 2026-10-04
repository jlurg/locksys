# LockSys Stakeholder Requirements

| Field | Value |
|---|---|
| Document ID | LS-STK-001 |
| Version | 0.1 |
| Status | Draft for baseline |
| Owner | jlurg |
| Date | 2026-10-03 |
| Derived documents | LS-SRS-001 (`docs/02_system/system_requirements.md`), LS-SAIC-001 (`docs/02_system/LS-SAIC.md`) |

---

## 1. Purpose and scope

This document records the stakeholder requirements of LockSys: the owner's original request and the owner decisions taken while planning release v1.0. For each requirement it states how stage A (release v1.0) interprets it and which system requirements or project artefacts satisfy it.

## 2. Source statement

The original request (2026-10-03), in English, numbered R1–R10:

| Ref | Request |
|---|---|
| R1 | A cross-platform mobile app with (a) a button that shows the door-lock state and a button that locks and unlocks the door, and (b) window control: while the button is held, the window motor keeps running; when the button is released or the upper or lower limit is reached, the motor stops. |
| R2 | The app sends the commands over WiFi to an ESP32-S3-based control unit. |
| R3 | The ESP32-S3 sends the door and window commands over CAN to an STM32F103RB MCU. |
| R4 | The STM32 locks and unlocks the door lock and drives the window motor up or down while the button is held. |
| R5 | The STM32 also monitors a temperature sensor over I2C and reports the measurement over UART. |
| R6 | Define the architecture of the whole project, the folder structure and the design patterns to use. |
| R7 | Toolchains: IAR Embedded Workbench for Arm with C-STAT static analysis for the STM32 project; ESP-IDF for the ESP32-S3 project. |
| R8 | Version control on GitHub in a public repository whose main branch is protected against direct pushes. The AI coding assistant used during development must not appear as author or co-author of commits or pull requests. Define and document the branching model and the rules that keep the repository organised and never break stable code. |
| R9 | Include the HIL architecture for tests on real hardware with the available equipment: oscilloscope, logic analyser, digital multimeter, bench power supply, electronic components, a USB-CAN adapter, an extra MCU board for stimulus, and SCPI-capable instruments. |
| R10 | The project shall look as professional as possible, close to automotive-grade firmware and software, without AUTOSAR or other industry tools. |

Owner decisions:

| Ref | Decision |
|---|---|
| U1 | Everything in English: code, comments, commits, pull requests and documentation. |
| U2 | Mobile framework: Flutter (Android and iOS). |
| U3 | The IAR licence is node-locked on a separate Windows PC; there is no cloud licence. IAR builds and C-STAT analyses run on that PC; daily work happens on a MacBook. |
| U4 | HIL equipment: USB-CAN adapter, stimulus MCU board, genuine Saleae logic analyser, SCPI instruments, oscilloscope, DMM, bench power supply. |
| U5 | Hardware from ready-made modules only: no breadboard and no loose through-hole components. |
| U6 | First window stage: a JGB37-520 encoder gear motor turning freely, without mechanics or limits; complexity is added in later stages. |
| U7 | The DCU state machines are modelled in IAR Visual State. |

---

## 3. Stakeholder requirements

| ID | Source | Stakeholder requirement | Stage A interpretation | Trace | Verification |
|---|---|---|---|---|---|
| STK-001 | R1 (a) | The owner shall see the door-lock state in the mobile app. | The state is read from the actuator position switch by the DCU and travels over CAN and WiFi to the APP; status that is not fresh is marked stale. | SYS-001, SYS-002, SYS-024, SYS-025, SYS-095 | System tests of the traced requirements |
| STK-002 | R1 (a) | The owner shall lock and unlock the door from the mobile app. | Lock is a tap; unlock requires an 800 ms hold-to-confirm; one door transaction at a time; the DCU limits actuation pulses and their rate. | SYS-002, SYS-003, SYS-004, SYS-024, SYS-027, SYS-034 | System tests |
| STK-003 | R1 (b), R4 | The owner shall move the window up or down from the mobile app; the motor shall run only while the control is held and shall stop when the control is released. | The stage A motor turns freely in the commanded direction only while the control is held. A layered keep-alive chain (APP → CGW → DCU) stops it on release and on any loss of evidence that the control is held; motion that the encoder does not confirm is stopped. | SYS-005, SYS-007, SYS-020, SYS-021, SYS-022, SYS-023, SYS-030, SYS-031, SYS-033, SYS-035, SYS-042, SYS-096 | System tests |
| STK-004 | R1 (b) | The window shall stop when it reaches its upper or lower limit. | Not applicable in stage A, which has no mechanics and no limits (U6). Stage B satisfies it with virtual end positions derived from the encoder position, stage D with end-position switches; obstacle detection follows in stage E. | SYS-006, SYS-007 (position), SYS-032, SYS-041 | System tests in stages B, D and E |
| STK-005 | R1, U2 | The mobile app shall run on Android and iOS. | One Flutter app for Android 10+ and iOS 15+; Android first; iOS uses a manual WiFi join if a programmatic WPA3 join is not possible. | SYS-095, SYS-096; LS-APP-SAD-001 | Integration tests, review |
| STK-006 | R2 | The app shall send its commands over WiFi to an ESP32-S3-based control unit. | The CGW provides a WPA3 SoftAP and an authenticated WebSocket protocol; one controlling phone at a time; pairing requires physical presence at the CGW. | SYS-021, SYS-050, SYS-051, SYS-052, SYS-053, SYS-054, SYS-055, SYS-056, SYS-057, SYS-072 | System tests |
| STK-007 | R3 | The ESP32-S3 shall forward door and window commands over CAN to an STM32F103RB. | CAN 2.0A at 500 kbit/s with end-to-end protection, timeouts with safe substitutes, version check and bus-off recovery. | SYS-022, SYS-026, SYS-037, SYS-062, SYS-063 | System tests |
| STK-008 | R4 | The STM32 shall lock and unlock the door actuator and drive the window motor while the control is held. | The DCU drives a dual VNH5019 motor driver shield and enforces the final interlocks (supply, temperature, driver faults, start-up behaviour). | SYS-003, SYS-005, SYS-036, SYS-039, SYS-040, SYS-043, SYS-072 | System tests |
| STK-009 | R5 | The STM32 shall monitor a temperature sensor over I2C and report the measurement over UART. | A TMP117 sampled once per second over I2C1; ASCII telemetry with checksums over the ST-LINK virtual COM port; the same value on CAN and in the APP; the value also feeds the over-temperature interlock. | SYS-008, SYS-009, SYS-010, SYS-040, SYS-090 | System tests |
| STK-010 | R6 | The architecture of the whole project, the folder structure and the design patterns shall be defined and documented. | LS-SAIC-001 (system architecture and interfaces), node architecture documents, the repository tree and architecture decision records. | LS-SAIC-001; `docs/04_software/`; `docs/adr/` | Review |
| STK-011 | R7 | The STM32 firmware shall be built with IAR Embedded Workbench for Arm and analysed with C-STAT; the ESP32-S3 firmware shall be built with ESP-IDF. | IAR EWARM (9.70 baseline) with C-STAT for MISRA C:2012 on the Windows Lab Host; a GCC shadow build keeps cloud CI independent of the licence; ESP-IDF v5.5.5. | LS-SAIC §4; `docs/08_process/` | Review, CI gates |
| STK-012 | R8 | The project shall live in a public GitHub repository whose main branch accepts no direct pushes, with a documented branching model that keeps stable code intact, and no AI tool shall appear as author or co-author of commits or pull requests. | GitFlow-lite with `develop` as the default branch, rulesets with required checks and signed commits, and attribution checks in local hooks and in the `pr-policy` CI check. | `docs/08_process/` (branching, commits and pull requests, GitHub settings, AI policy) | Review, CI |
| STK-013 | R9, U4 | A HIL architecture shall allow tests on real hardware with the owner's equipment. | Windows Lab Host with a pytest framework, a stimulus MCU, a logic analyser, an SCPI bench supply and a USB-CAN adapter; unattended HIL-SIM and attended HIL-REAL configurations. | LS-HIL-001; verification methods of LS-SRS-001; SYS-080 | Review, bench qualification |
| STK-014 | R10 | The project shall be as professional as possible and close to automotive-grade firmware and software, without AUTOSAR or commercial automotive tools. | Safety and security concepts inspired by ISO 26262 and ISO/SAE 21434 (not certified), MISRA C:2012 with C-STAT, end-to-end protection, UDS-lite diagnostics with DTCs, bidirectional traceability and recorded HIL evidence. | SYS-038, SYS-060, SYS-061, SYS-070, SYS-071; LS-SAF-001; LS-SEC-001; LS-VER-001 | Review, system tests |
| STK-015 | U1 | The repository content shall be in English. | Applies to code, comments, commit messages, pull requests and documentation. | Repository rules (`CONTRIBUTING.md`, `docs/08_process/`) | Review |
| STK-016 | U5, R9 | The bench hardware shall be built from ready-made modules, without a breadboard. | Pololu shield #2507, Waveshare CAN boards, Adafruit TMP117 with STEMMA QT cable, voltage sense module, blade fuse and lever connectors. This replaces the breadboard components mentioned in R9. | SYS-080, SYS-081; HWR-001…HWR-011 | Inspection |
| STK-017 | U6 | The first window stage shall use a freely turning encoder gear motor; mechanics and limits shall be added in later stages. | Stage A in v1.0; stages B (virtual end positions), C (speed control), D (end-position switches) and E (anti-pinch) follow. | LS-SAIC §0.4; SYS-006, SYS-041, SYS-042; HWR-010 | Review |
| STK-018 | U3 | IAR builds and C-STAT analyses shall run on the Windows PC with the node-locked licence while daily work happens on a MacBook. | Lab Host with a self-hosted runner and a push-triggered IAR gate; remote access from the MacBook. | `docs/08_process/` (Lab Host); `docs/adr/` | Review |
| STK-019 | U7 | The DCU state machines shall be modelled in IAR Visual State. | WinCtrl, DoorCtrl and ModeMgr models with committed generated code; the safety layer stays hand-written below the models. | LS-SAIC §1.2, §11.3 (SM-19), §13.2 (B1A55), §7.8 (DID 0xFD09) | Review, unit tests, HIL |

---

## 4. Stage A summary

- The functional core of R1–R5 is implemented in stage A, except the limit stop of R1 (b): stage A has no mechanics, so STK-004 is satisfied in stage B (virtual end positions) and stage D (end-position switches). This follows owner decision U6.
- R9 mentions breadboard components; owner decision U5 replaces them with modules (STK-016).
- Process and design constraints (STK-010, STK-011, STK-012, STK-015, STK-018, STK-019) are verified by review of the documents named in the Trace column at each milestone exit and by the CI gates.

## 5. Traceability to system requirements

| STK | System requirements |
|---|---|
| STK-001 | SYS-001, SYS-002, SYS-024, SYS-025, SYS-095 |
| STK-002 | SYS-002, SYS-003, SYS-004, SYS-024, SYS-027, SYS-034 |
| STK-003 | SYS-005, SYS-007, SYS-020, SYS-021, SYS-022, SYS-023, SYS-030, SYS-031, SYS-033, SYS-035, SYS-042, SYS-096 |
| STK-004 | SYS-006, SYS-007, SYS-032, SYS-041 |
| STK-005 | SYS-095, SYS-096 |
| STK-006 | SYS-021, SYS-050, SYS-051, SYS-052, SYS-053, SYS-054, SYS-055, SYS-056, SYS-057, SYS-072 |
| STK-007 | SYS-022, SYS-026, SYS-037, SYS-062, SYS-063 |
| STK-008 | SYS-003, SYS-005, SYS-036, SYS-039, SYS-040, SYS-043, SYS-072 |
| STK-009 | SYS-008, SYS-009, SYS-010, SYS-040, SYS-090 |
| STK-013 | SYS-080 |
| STK-014 | SYS-038, SYS-060, SYS-061, SYS-070, SYS-071 |
| STK-016 | SYS-080, SYS-081 |
| STK-017 | SYS-006, SYS-041, SYS-042 |

## 6. References

- LS-SRS-001, `docs/02_system/system_requirements.md`
- LS-SAIC-001, `docs/02_system/LS-SAIC.md`
- Amendment register, `docs/02_system/amendment_register.md`
