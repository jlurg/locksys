# LockSys Shared C Libraries

| Field | Value |
|---|---|
| Document ID | LS-LIB-SAD-001 |
| Version | 0.1 |
| Status | Draft |
| Owner | jlurg |
| Contract | LS-SAIC-001 v0.2 ([LS-SAIC](../../02_system/LS-SAIC.md)) §1.4, §1.5, §7.2 |
| Requirements | `SWR-LIB-nnn` (§6 of this document) |

## 1. Purpose and scope

This document defines the shared C libraries of LockSys and their software requirements:

- `libs/ls_e2e`: the CAN end-to-end protection profile of LS-SAIC-001 §7.2 (sender and receiver);
- `libs/ls_common`: CRC-8/SAE-J1850 (`LsCrc8`), the prioritised event bitset (`LsEvSet`), the timestamp timer pool (`LsTmr`), the single-producer single-consumer ring buffer (`LsRing`) and the generated interface headers in `gen/`.

The libraries are compiled into the DCU (IAR EWARM and the GCC shadow build), into the CGW (as ESP-IDF components) and into host unit tests. The public headers carry the authoritative signatures (Doxygen); this document states the design, the constraints and the requirements.

## 2. Design rules

| Rule | Reason |
|---|---|
| MISRA C:2012 (Amendments 1–4) as for DCU project code; in scope of the [GEP](../../08_process/misra/gep.md) | The code runs in the DCU safety layer |
| C11 subset: fixed-width types, no floating point, no heap, no recursion, no function pointers, no I/O, no global state; all state in caller-owned objects | Usable from any context, testable on the host, deterministic |
| No dependency on a platform, RTOS or ESP-IDF header; `ls_e2e` depends only on `ls_common` | One source for three toolchains |
| Every function validates its arguments and returns a status; an invalid argument leaves the state unchanged | Defensive behaviour without assertions in product code |
| Interface values (enumerations, parameters, DTC codes, CAN frame attributes) come only from `gen/` | Single source of truth in `interfaces/` |
| Not interrupt safe unless stated (`LsRing` is safe for one producer and one consumer context) | Callers own concurrency (DCU critical sections, CGW active objects) |
| Names: `Ls<Module>_<Verb>`, types `Ls<Module>_<Name>Type`, macros `LS_<MODULE>_<NAME>` | LS-SAIC-001 §14.2 |

Build integration: each library has a dual-mode `CMakeLists.txt` (ESP-IDF component when `ESP_PLATFORM` is set, otherwise a static library for the DCU GCC build and host tools), a Ceedling project `project.yml` with the gcov plugin, and is added to the IAR project as a source group with the same include paths.

## 3. `ls_e2e`

### 3.1 Profile

| Item | Definition (LS-SAIC-001 §7.2) |
|---|---|
| Layout | Byte 0 = CRC; byte 1 bits 0–3 = alive counter (0…15, wrapping) |
| CRC | CRC-8/SAE-J1850 (polynomial 0x1D, init 0xFF, final XOR 0xFF, no reflection) over [DataID low byte, DataID high byte, frame bytes 1 … DLC−1]; the DataID is not transmitted |
| Configuration per message | DataID, fixed DLC, mode (cyclic or event), MaxDelta, `n_e2e_ok_valid`, `n_e2e_err_invalid` — taken from the DBC attributes through `ls_can_matrix_gen.h` and from `ls_params_gen.h` |
| Sender | `LsE2e_Protect` writes the next alive counter and the CRC and advances the counter; it is called once for every frame handed to the controller or driver, and a caller that cannot hand the frame over restores the previous sender state. `LsE2e_ProtectWithCounter` writes a given counter (CGW STOP rewrite of owned slots). Bits 4–7 of byte 1 carry signal data and are kept |
| Receiver, cyclic mode | Every frame checked in arrival order. DLC or CRC mismatch → CRC_ERROR (reference unchanged). First frame after initialisation or timeout → OK and sets the reference. Δ = (counter − reference) mod 16: 0 → REPEATED; 1…MaxDelta → OK; > MaxDelta → WRONG_SEQUENCE and resynchronisation. `n_e2e_ok_valid` consecutive OK → VALID; `n_e2e_err_invalid` consecutive errors → INVALID; `LsE2e_RxTimeout` → INVALID and reference cleared |
| Receiver, event mode | DLC and CRC checked; counter not sequence-checked; no timeout |
| Data usability | `LsE2e_IsDataValid` is true only if the last frame was OK and the state after processing it is VALID |
| Counters | CRC, sequence, repeated and timeout counts per message, saturating (DCU DID 0xFD08) |

### 3.2 API

| Function | Purpose |
|---|---|
| `LsE2e_IsConfigValid` | Check a message configuration (DLC 2…8, mode cyclic or event, MaxDelta 1…14 for cyclic messages, thresholds ≥ 1) |
| `LsE2e_Crc` | CRC of a frame with its DataID |
| `LsE2e_TxInit`, `LsE2e_Protect`, `LsE2e_ProtectWithCounter` | Sender |
| `LsE2e_RxInit`, `LsE2e_Check`, `LsE2e_RxTimeout`, `LsE2e_IsDataValid` | Receiver |

E2E protects against corruption, repetition, loss, delay and masquerading inside the system; it is not a security control (LS-SAIC-001 §12, TS-07).

## 4. `ls_common`

| Module | Responsibility | Principal API | Users |
|---|---|---|---|
| `LsCrc8` | CRC-8/SAE-J1850, bytewise, with incremental update; check value of "123456789" = 0x4B | `LsCrc8_J1850`, `LsCrc8_J1850Update` | `ls_e2e` |
| `LsEvSet` | Prioritised event bitset of up to 32 events; the lowest event number is delivered first; posting a pending event again has no effect; a drain cycle delivers at most a configured number of events and reports `LS_EVSET_E_LIMIT` when exhausted (treated as an engine fault by the DCU adapters) | `LsEvSet_Init`, `_Post`, `_Cancel`, `_IsPending`, `_IsEmpty`, `_Clear`, `_BeginDrain`, `_Next` | DCU SWC adapters (Visual State event derivation) |
| `LsTmr` | Pool of timestamp timers, each slot bound to one event; `LsTmr_Poll` posts the bound event when the elapsed time reaches the duration (elapsed ≥ duration, never early); 32-bit millisecond timestamps with modular differences, exact up to 2^31 − 1 ms | `LsTmr_Init`, `_Start`, `_Stop`, `_IsActive`, `_Elapsed`, `_Poll` | DCU SWC adapters (Visual State timers), services |
| `LsRing` | Single-producer single-consumer byte ring with a power-of-two capacity; whole-record push (all or nothing); pop, or peek-contiguous plus consume for DMA; free-running 32-bit indices written by one side each; `LS_RING_BARRIER()` orders data and index accesses | `LsRing_Init`, `_Used`, `_Free`, `_Push`, `_Pop`, `_PeekContiguous`, `_Consume` | DCU telemetry (Tlm with USART2 DMA), CGW queues where applicable |

### 4.1 Generated headers (`libs/ls_common/gen/`)

| File | Source | Content |
|---|---|---|
| `ls_enums_gen.h` | `interfaces/enums/locksys_enums.yaml` | Canonical enumerations as fixed-width typedefs and `#define` constants |
| `ls_params_gen.h` | `interfaces/params/timing.yaml` | `LS_<KEY>` parameter constants |
| `ls_dtc_gen.h` | `interfaces/dtc/dtc_catalog.yaml` | DTC values and notice codes |
| `ls_can_matrix_gen.h` | `interfaces/can/locksys.dbc` | Frame identifiers, DLC, send type, cycle, gap, repetitions, E2E mode, DataID, MaxDelta, RX timeout, matrix version |

Generated files start with a `DO NOT EDIT` line, are committed, and are regenerated with `uv run tools/codegen/regen.py` (`--check` fails on drift). They are analysed under permit DP-03 of the GEP.

## 5. Verification

| Measure | Gate |
|---|---|
| Ceedling 1.1.9 host tests in the `locksys/ceedling:1.1.9` container: `tools/docker/ceedling/run.sh libs/<lib> test:all` | `libs-unit` |
| E2E vectors of `interfaces/vectors/e2e_v1.json` (generated into `libs/ls_e2e/test/gen/`), including the CRC check value 0x4B and the frame vectors of LS-SAIC-001 §7.10 | `libs-unit` |
| Coverage (`tools/ci/quality_gates.yaml`): `ls_e2e`, `LsCrc8`, `LsEvSet`, `LsTmr` 100 % lines and branches; rest of `ls_common` ≥ 95 % lines and ≥ 90 % branches | Enforced now |
| Target compilation in the DCU GCC shadow build (`-Werror -Wconversion -Wsign-conversion`) and the CGW build | `dcu-gcc`, `cgw-build` |
| C-STAT in the DCU IAR build (gating scope of the GEP) | `iar-gate` |
| Traceability: `/* @satisfies SWR-LIB-nnn */` in sources, `/* @verifies SWR-LIB-nnn */` in tests | `tools/trace/trace.py --report` |

## 6. Software requirements

Columns as in the node requirement documents: Parents are SYS requirements, SM entries or contract sections; Ver is UT (host unit test), R (review) or CI (automated CI check); MS is the milestone.

| ID | Requirement | Parents | Ver | Tag | MS |
|---|---|---|---|---|---|
| SWR-LIB-001 | `LsCrc8_J1850` shall compute CRC-8/SAE-J1850 (polynomial 0x1D, init 0xFF, final XOR 0xFF, no reflection) with a check value of 0x4B for "123456789", and `LsCrc8_J1850Update` shall give the same result over split input. | SYS-037; SM-04 | UT | [SAF] | M0 |
| SWR-LIB-002 | `LsE2e_Protect` shall write the alive counter into byte 1 bits 0–3 without changing bits 4–7 and the CRC over the DataID and bytes 1…DLC−1 into byte 0, and advance the sender counter modulo 16. | SYS-037; SM-04 | UT | [SAF] | M0 |
| SWR-LIB-003 | `LsE2e_Check` shall classify every frame as OK, CRC_ERROR (DLC or CRC mismatch), REPEATED or WRONG_SEQUENCE according to LS-SAIC-001 §7.2 for cyclic messages, and check only DLC and CRC for event messages. | SYS-037; SM-04 | UT | [SAF] | M0 |
| SWR-LIB-004 | The receiver shall become VALID after `n_e2e_ok_valid` consecutive OK frames and INVALID after `n_e2e_err_invalid` consecutive errors or `LsE2e_RxTimeout`, which also clears the reference counter; `LsE2e_IsDataValid` shall be true only for an OK frame with the state VALID after processing it. | SYS-037; SM-04 | UT | [SAF] | M0 |
| SWR-LIB-005 | `ls_e2e` shall reproduce every vector of `interfaces/vectors/e2e_v1.json`. | SYS-037 | UT | [SAF] | M0 |
| SWR-LIB-006 | `ls_e2e` shall keep saturating CRC, sequence, repeated and timeout counters per receiver. | SYS-060 | UT | — | M0 |
| SWR-LIB-010 | `LsEvSet` shall deliver pending events lowest number first, ignore a repeated post of a pending event, and stop a drain cycle after its configured budget with `LS_EVSET_E_LIMIT`, leaving the remaining events pending. | SYS-005; SM-19 | UT | [SAF] | M0 |
| SWR-LIB-011 | `LsTmr_Poll` shall post the bound event of a started timer exactly once when the elapsed time is greater than or equal to its duration, never earlier, including across the 32-bit timestamp wrap for durations up to 2^31 − 1 ms. | SYS-033; SM-08 | UT | [SAF] | M0 |
| SWR-LIB-012 | `LsRing_Push` shall store a record completely or not at all, and `LsRing` shall be correct for one producer context and one consumer context without a lock. | SYS-090 | UT | — | M0 |
| SWR-LIB-020 | Every library function shall validate its arguments, return a status, and leave the state unchanged on an invalid argument. | — | UT | — | M0 |
| SWR-LIB-021 | The libraries shall use static memory only, no recursion, no function pointers, no floating point and no platform headers, and shall build without warnings with IAR EWARM, `arm-none-eabi-gcc` (`-Werror -Wconversion -Wsign-conversion`), the ESP-IDF toolchain and the host compiler. | SYS-070 | R, CI | — | M0 |
| SWR-LIB-022 | Interface constants used by the libraries and their users shall come only from the generated headers of `libs/ls_common/gen/`. | LS-SAIC-001 §1.4 | R, CI | — | M0 |

## 7. Rationale

- **One E2E implementation for both nodes.** A sender and a receiver that share code and test vectors cannot disagree on the profile; the HIL checks the same vectors in Python.
- **Event bitset and timestamp timers instead of queues and countdowns.** A bitset cannot overflow and fixes the event priority by numbering; comparing elapsed time with "greater or equal" gives exact minimum times (brake and dead time, SYS-033) independent of polling jitter.
- **Dual-mode CMake.** The same sources are an ESP-IDF component and a plain library, so the CGW and the DCU shadow build use identical code.

## 8. References

- [LS-SAIC-001](../../02_system/LS-SAIC.md) §1.4, §1.5, §7.2, §7.10
- [LS-IF-001 CAN matrix](../../03_interfaces/can_matrix.md) §5
- [DCU software architecture](../dcu/architecture.md) (LS-DCU-SAD-001) §8, [CGW software architecture](../cgw/architecture.md) (LS-CGW-SAD-001) §10
- [Guideline Enforcement Plan](../../08_process/misra/gep.md), [coding standard](../../08_process/coding_standard.md), [verification strategy](../../07_verification/verification_strategy.md)
- AUTOSAR E2E Protocol Specification (profile concepts); SAE J1850 (CRC-8 polynomial)
