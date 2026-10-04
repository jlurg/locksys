# Glossary

| Field | Value |
| --- | --- |
| Document ID | LS-PRJ-002 |
| Version | 1.0 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This glossary defines the terms and abbreviations used across the LockSys documentation. Identifier formats are defined normatively in the [documentation index](../README.md); interface terms are defined in the [system architecture and interface contract](../02_system/LS-SAIC.md).

## Terms

| Term | Definition |
| --- | --- |
| ADR | Architecture decision record in MADR 4.0.0 format, stored in `docs/adr/` |
| Alive counter | 4-bit counter in every E2E-protected CAN frame; detects repeated, lost and out-of-order frames |
| APP | Flutter application for Android and iOS: user interface, pairing, authenticated session and hold-to-run keep-alive. It takes no safety decisions. |
| APP simulator | Python implementation of the APP side of the APP protocol, used on the HIL bench and in CGW tests |
| ASIL | Automotive Safety Integrity Level (ISO 26262); LockSys uses indicative ratings only |
| ASPICE | Automotive SPICE process assessment model; the documentation folders map to its process groups |
| Back-merge | Merge of `main` into `develop` or into an open release branch after a release, through a `sync/*` branch |
| Bench lock | File `bench.lock` on the Lab Host that reserves the HIL bench for manual use; HIL jobs do not start while it exists |
| BOM | Bill of materials |
| Break-glass | Documented procedure for temporarily disabling a ruleset, recorded in an audit issue |
| Build type | `DEV`, `RC` or `RELEASE`; reported by every build |
| bxCAN | CAN controller of the STM32F103 |
| C-SPY | IAR debugger |
| C-SPYLink | Visual State plugin that animates the state machine model during a C-SPY session on the target |
| C-STAT | IAR static analysis tool with MISRA C, CERT C and standard check sets; the authoritative checker for the DCU |
| CAN matrix | Definition of all CAN frames and signals in `interfaces/can/locksys.dbc`, versioned as major.minor |
| Ceedling | Unit test build system for C (Unity, CMock), run in a Docker image |
| CGW | Central gateway node: ESP32-S3 with ESP-IDF; Wi-Fi access point, WebSocket server, authentication, keep-alive supervision and CAN communication with the DCU |
| CMSIS | Arm Cortex Microcontroller Software Interface Standard; LockSys vendors CMSIS-Core and the STM32F1 device headers |
| Code generation gate | CI check that regenerates all generated code from its sources and fails on any difference |
| Conventional Commits | Commit message format `type(scope): subject`, used for commits and PR titles |
| CPR | Encoder counts per revolution of the motor output shaft in x4 decoding |
| CS | Current sense output of the VNH5019, about 140 mV/A on the Pololu shield |
| CSG, CSR | Cybersecurity goal and cybersecurity requirement (ISO/SAE 21434-inspired), IDs `CSG-nn` and `CSR-nnn` |
| DataID | Identifier of an E2E-protected frame; included in the CRC but not transmitted, so a frame received under the wrong identity fails the check |
| DCU | Door control unit: NUCLEO-F103RB with bare-metal C built with IAR EWARM; drives the window motor and the lock actuator and implements all final safety interlocks |
| Dead-man chain | The chain of keep-alive supervisions in APP, CGW, CAN and DCU: the window moves only while fresh evidence of a held button arrives |
| Deviation | Approved exception to a coding guideline for specific instances, recorded as `DEV-<area>-nnn` |
| Deviation permit | Approved exception for a recurring, path-scoped use case, recorded as `DP-nn` |
| DID | Data identifier read through UDS service 0x22 |
| DIR_MISMATCH | Window fault: encoder counts move opposite to the commanded direction |
| DoR, DoD | Definition of Ready, Definition of Done |
| DTC | Diagnostic trouble code: 3 bytes (SAE J2012 code and failure type byte); DCU codes B1A and U1A, CGW codes B1B and U1B |
| E2E | End-to-end protection of CAN frames: CRC-8 (SAE J1850) over DataID and payload, and an alive counter |
| ECUAL | ECU abstraction layer of the DCU: device drivers above the MCAL (H-bridge, encoder position, lock actuator, sensors, CAN interface) |
| EN/DIAG | Enable and diagnostic pin of the VNH5019; read by the DCU as a fault input only |
| EWARM | IAR Embedded Workbench for Arm |
| FTB | Failure type byte of a DTC |
| FTTI | Fault tolerant time interval |
| GCS | Guideline Compliance Summary: per-release statement of MISRA compliance |
| GEP | Guideline Enforcement Plan: assigns every MISRA guideline to its enforcement method |
| GRP | Guideline re-categorisation plan, part of the GEP |
| HARA | Hazard analysis and risk assessment |
| HAZ, SG, SM | Hazard, safety goal and safety mechanism, IDs `HAZ-nn`, `SG-nn` and `SM-nn` |
| HIL | Hardware-in-the-loop bench: the real firmware runs on the real hardware while the bench simulates or controls its environment |
| HIL-SIM, HIL-REAL | HIL configuration with a simulated plant (stimulus MCU, actuator supply off) and with the real motor, actuator and sensor (attended runs only) |
| HKDF, HMAC | HMAC-based key derivation function and keyed-hash message authentication code (SHA-256), used for the APP session |
| HoldAge | Age of the last keep-alive received by the CGW, transmitted in `WinCmd`; the DCU stops the window when it exceeds 400 ms |
| Hold-to-run | Window operation that continues only while the user holds the button |
| HSE, HSI | External and internal high-speed clock of the STM32; the DCU uses the 8 MHz clock from the ST-LINK in HSE bypass mode |
| HWR | Hardware requirement, ID `HWR-nnn` |
| IWDG | Independent watchdog of the STM32 |
| K_pair, K_sess | Pairing key shared by APP and CGW; session key derived from it with HKDF for each connection |
| KL30, KL31 | Automotive terminal designations: permanent +12 V supply and ground |
| Lab Host | Windows PC holding the IAR licence, the self-hosted runners and the HIL bench |
| LATER | Scope tag for items deferred beyond the MVP |
| MADR | Markdown Architectural Decision Records |
| MCAL | Microcontroller abstraction layer of the DCU: register-level drivers on CMSIS |
| MCO | Microcontroller clock output; the ST-LINK provides the 8 MHz clock of the DCU through it |
| MISRA C | Guidelines for the use of C in critical systems; LockSys follows MISRA C:2012 with Amendments 1 to 4 |
| MVP | Minimum viable product: the scope of release v1.0.0 |
| NO_MOTION | Window fault: drive commanded but fewer encoder counts than expected (stall or encoder failure) |
| NodeSts | Periodic CAN status frame of each node, with mode and CAN matrix version |
| PMF | Protected management frames (IEEE 802.11w), required with WPA3 |
| PressId | Identifier of one button press in `WinCmd`; the DCU latches it at every stop, so a stale or repeated frame cannot restart the motor |
| PW | Person-week, 40 focused hours |
| RC | Release candidate, tagged `vX.Y.Z-rc.N` |
| Required check | Status check that must pass before a PR can merge: `pr-policy`, `ci-gate`, `iar-gate`, `hil-gate` |
| Restbus | Simulation of the missing CAN nodes by the test bench |
| RTE | Runtime environment of the DCU: typed signals between components, with one writer per signal |
| Ruleset | GitHub protection rules for branches or tags |
| SAFE | DCU mode with all outputs de-energised, entered on critical faults and latched until cleared by the defined diagnostic sequence or a power-on reset |
| SARIF | Static Analysis Results Interchange Format |
| SCPI | Standard Commands for Programmable Instruments |
| Shadow build | Build of the DCU sources with Arm GCC and `-Werror`, independent of the IAR licence |
| Soak | Long-duration HIL run (8 h in HIL-SIM) |
| SoftAP | Wi-Fi access point provided by the CGW |
| SPDX | Software Package Data Exchange; licence identifiers in file headers |
| Stage A to E | Window functionality stages: A hold-to-run without limits, B virtual limits, C speed control, D physical limits, E anti-pinch |
| Stimulus MCU | Microcontroller board on the HIL bench that emulates the plant: encoder signals, lock feedback, temperature sensor, current sense |
| STK, SYS, SWR | Stakeholder, system and software requirements, IDs `STK-nnn`, `SYS-nnn`, `SWR-DCU-nnn`, `SWR-CGW-nnn`, `SWR-APP-nnn`, `SWR-LIB-nnn` |
| SWC | Software component of the DCU application layer: WinCtrl, DoorCtrl, TempMon, CmdArb, ModeMgr, DiagHdl |
| TARA | Threat analysis and risk assessment (ISO/SAE 21434-inspired) |
| Trace tags | `@satisfies` and `@verifies` annotations that link code and tests to requirement IDs |
| TST | Test identifier `TST-<level>-<scope>-nnn`, levels UT, IT, HIL and MAN, scopes DCU, CGW, APP, SYS and LIB |
| TWAI | Two-Wire Automotive Interface: the CAN controller of the ESP32-S3 |
| UDS, ISO-TP | Unified Diagnostic Services (ISO 14229-1) and the transport protocol on CAN (ISO 15765-2); the DCU implements a subset ("UDS-lite") |
| V-model | Development model that pairs each specification level with a verification level |
| VCP | Virtual COM port of the ST-LINK, connected to USART2 of the DCU |
| Visual State | IAR tool for modelling, verifying, validating and generating code from state machines |
| VNH5019 | Automotive H-bridge driver IC; two are fitted on the Pololu driver shield |
| VSDeduct | Generated Visual State function `<System>VSDeduct` that processes one event |
| WinCmd | CAN frame `CGW_WinCmd` carrying the window request, PressId and HoldAge, sent every 20 ms and on events |
| WP0 | Early phone verification gate for Wi-Fi join and network binding |
| WPA3-SAE, H2E | Wi-Fi security with Simultaneous Authentication of Equals, and its hash-to-element password derivation |

## References

- [Documentation index](../README.md)
- [System architecture and interface contract](../02_system/LS-SAIC.md)
- [Roadmap](roadmap.md)
