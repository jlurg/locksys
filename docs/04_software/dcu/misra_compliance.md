# DCU MISRA Compliance Record

| Field | Value |
|---|---|
| Document ID | LS-DCU-MCR-001 |
| Version | 0.1 |
| Status | Draft (per-guideline C-STAT coverage pending the Lab Host day-1 export) |
| Owner | jlurg |
| Enforcement plan | LS-PRC-004 ([Guideline Enforcement Plan](../../08_process/misra/gep.md)) |
| Architecture | LS-DCU-SAD-001 ([DCU software architecture](architecture.md)) |

## 1. Purpose and scope

This record holds the DCU-specific evidence that the [Guideline Enforcement Plan](../../08_process/misra/gep.md) (GEP) refers to:

- the compliance claim and the build configuration it applies to;
- the implementation-defined behaviour that the DCU code relies on (MISRA C:2012 Dir 1.1, GEP review item R1);
- the per-guideline C-STAT coverage list, which assigns a replacement enforcement method to every guideline that C-STAT does not check;
- the deviations and permits in force and the compliance status per release.

Scope: the code in scope of the GEP, that is `firmware/dcu/src/`, `firmware/dcu/cfg/`, the C sources of `firmware/dcu/startup/`, `libs/*/src/` and `libs/*/include/` as compiled into the DCU, the generated code in `firmware/dcu/gen/` and `libs/ls_common/gen/`, and the Visual State engines in `firmware/dcu/gen_vs/release/`. The GEP defines the enforcement methods (CS, CC, LK, SC, CP, RV, CO, MD, NA), the re-categorisation and the analysis configuration; they are not repeated here.

## 2. Compliance claim

| Item | Value |
|---|---|
| Guidelines | MISRA C:2012 with Amendments 1 to 4, checked with the IAR C-STAT MISRA C:2023 rule set |
| Re-categorisation | GEP §"Guideline re-categorisation plan": selected Advisory guidelines adopted as Required |
| Deviations | Records in [`deviations.yaml`](../../08_process/misra/deviations.yaml), process in [deviation process](../../08_process/misra/deviation_process.md) |
| Permits | DP-01 to DP-05 (GEP and deviation process) |
| Assessment | Self-assessed; the DCU is a bench demonstrator |
| Evidence per release | Guideline Compliance Summary (GCS), C-STAT HTML and SARIF reports, deviation and permit list (GEP §"Evidence") |

The claim holds only for the build configuration of §3. A change of compiler version, language options or C-STAT rule set requires a new review of §4 and §5 before the next release.

## 3. Build configuration covered

| Item | IAR build (product image) | GCC shadow build (cross-check) |
|---|---|---|
| Compiler | IAR C/C++ Compiler for Arm, EWARM 9.70 baseline (10.10.x only if the licence allows); exact build recorded in `tools/versions.env` | Arm GNU Toolchain 13.3.rel1 (`arm-none-eabi-gcc`) |
| Target | Cortex-M3 (STM32F103RB), Thumb-2, little-endian, no FPU | `-mcpu=cortex-m3 -mthumb` |
| Language | ISO C as IAR "Standard C" with IAR extensions available only through `ls_compiler.h` (Rule 1.2, permit DP-02) | `-std=c11` |
| Plain `char` | Unsigned (IAR default, set explicitly in the project) | Unsigned (AAPCS default) |
| Diagnostics | Remarks on, warnings as errors, `--require_prototypes` | `-Werror -Wall -Wextra -Wconversion -Wsign-conversion` |
| Library | DLIB Normal, no semihosting, no heap | newlib headers only for `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<string.h>` |
| Configurations analysed | Hil and Release (gating); Debug (not gating) | Hil and Release switches |

Project options are defined in the [IAR project setup guide](iar_project_setup.md) §4.3; the GCC options in `firmware/dcu/cmake/arm-none-eabi-gcc.cmake`.

## 4. Implementation-defined behaviour (Dir 1.1)

### 4.1 Behaviour relied on

The DCU code relies on the following implementation-defined behaviour. Each item is identical on the IAR and the GCC build of §3 and on the host compilers used for unit tests unless stated otherwise. Items marked "static assertion" are checked at compile time in the platform layer (`ls_std_types.h`, `ls_static_assert.h`) or in the module that relies on them.

| ID | Behaviour (C11 Annex J.3 reference) | Value on the target | Where relied on | Check |
|---|---|---|---|---|
| IDB-01 | Sizes of the integer types (J.3.5) | `char` 8, `short` 16, `int` 32, `long` 32, `long long` 64 bits; pointers 32 bits | Fixed-width types are used throughout; `int` width matters only for integer promotion | Static assertion |
| IDB-02 | Representation of signed integers (J.3.5) | Two's complement, no padding bits | Encoder and timer modular arithmetic | Static assertion |
| IDB-03 | Conversion of an out-of-range value to a signed integer type (J.3.5) | Modulo 2^N (bit pattern kept) | `WinPos` 16-bit modular counter difference `(int16_t)(uint16_t)(now - prev)`; signed CAN signals unpacked by the generated code | Static assertion; unit tests at the wrap points |
| IDB-04 | Plain `char` signedness (J.3.4) | Unsigned | Not relied on for arithmetic: `char` holds ASCII text only (telemetry, version strings) | Rule 10.x checks; coding standard |
| IDB-05 | Size of enumerated types (J.3.9) | Smallest integer type that holds the values on both IAR and `arm-none-eabi-gcc` (1 byte for small enumerations); 4 bytes on host compilers | Not relied on: interface values use fixed-width typedefs with `#define` constants (`ls_enums_gen.h`); no enumeration type appears in a stored layout, a CAN payload or a `noinit` record | Review item R1; `noinit` layout static assertions |
| IDB-06 | Allocation and order of bit-fields (J.3.9) | Not used | Register access uses CMSIS masks; CAN packing uses shifts (cantools `bit_fields: false`) | C-STAT (Rule 6.1, 6.2); review |
| IDB-07 | What constitutes an access to a `volatile` object (J.3.10) | One bus access of the declared width per read or write in the source; no access merging or splitting for aligned 8, 16 and 32-bit objects | Peripheral registers through CMSIS; data shared with interrupt handlers | Review item R6; disassembly check of MCAL at M1 |
| IDB-08 | Conversion between pointers and integers (J.3.7) | The integer value equals the byte address | CMSIS peripheral base addresses (Rule 11.4, permit DP-01); ROM checksum range | Permit DP-01 |
| IDB-09 | Right shift of a negative signed value (J.3.5) | Arithmetic shift | Not relied on: shifts operate on unsigned operands only (Rule 10.1) | C-STAT (Rule 10.1) |
| IDB-10 | Search of `#include "..."` headers (J.3.12) | Directory of the including file first, then the configured include paths in order | Per-layer include paths (layer enforcement) | Build; `tools/arch/check_layers.py` |
| IDB-11 | Significant characters of identifiers (J.3.3) | More than 63 internal and 31 external characters on both compilers | Identifiers are unique within 31 characters (Rules 5.1 to 5.5) | C-STAT |
| IDB-12 | Source and execution character sets (J.3.4) | ASCII; sources are plain ASCII | All sources and strings | `tools/codegen/check_ascii.py` in `lint` |
| IDB-13 | `#pragma` directives (J.3.13) | Recognised IAR pragmas only | Not used in DCU sources outside `ls_compiler.h`; generated DCU code contains none | C-STAT (Rule 1.2); review |
| IDB-14 | Compiler extensions: `__no_init`, section placement, call-graph root annotations, intrinsics (`__disable_irq`, `__DSB`, `__set_BASEPRI`) | As documented in the IAR C/C++ Development Guide for Arm | Wrapped by the macros of `ls_compiler.h` (for example `LS_ISR_ROOT`), with GCC equivalents | Permit DP-02; review item R2 |
| IDB-15 | Library: `memcpy`, `memset`, `memcmp` of `<string.h>` | Standard semantics; no implementation-defined aspect used | RTE copies, buffer initialisation | Rules 21.x checks |
| IDB-16 | Alignment of objects (J.3.9, J.3.13) | Natural alignment; no unaligned access is generated for aligned objects | `noinit` record, DMA buffers, CAN mailboxes | Static assertions on offsets |

### 4.2 Behaviour explicitly not relied on

- Division and remainder of negative operands, floating point, `<stdio.h>`, `<signal.h>`, `<time.h>`, `<locale.h>`, wide characters, dynamic memory and the `errno` mechanism.
- The order of evaluation of function arguments and of operands with side effects (Rule 13.2).
- Any behaviour of the Visual State engines that is not documented by the coder output: the engines are analysed as code (GEP §"Visual State generated code").

### 4.3 Change rule

A pull request that introduces reliance on further implementation-defined behaviour adds a row to §4.1 (GEP review item R1). A toolchain update repeats the static assertions and the IDB-07 disassembly check.

## 5. Per-guideline C-STAT coverage

### 5.1 Procedure

1. At the Lab Host day-1 check the list of enabled C-STAT checks is exported for the gating rule set: on EWARM 9.70.x from the C-STAT selection of the project (stored in `firmware/dcu/cstat/`); on 10.10.x with `iarbuild -cstat_checks`.
2. The export is mapped to MISRA C:2012 guideline numbers with the C-STAT check-to-guideline table of the IAR C-STAT Static Analysis Guide for the installed version.
3. Every guideline that the GEP assigns to CS but that has no enabled check is listed in §5.3 with its replacement method (CC, LK, SC or RV).
4. The table is reviewed at every EWARM update and before every release; the GCS of the release is generated from it.

### 5.2 Status

| Item | Status |
|---|---|
| Export of the enabled checks | Pending (Lab Host day-1 check) |
| Mapping to guidelines | Pending |
| First C-STAT baseline of `gen_vs/release/` (fixes the DP-05 list) | Pending (Lab Host check U5) |
| Seeded-defect file baseline | Pending (first EWARM installation) |

### 5.3 Guidelines without a C-STAT check

The table is completed from the day-1 export. A guideline is listed only when no enabled check exists for it; the method column replaces CS for that guideline in every scope of the GEP table.

| Guideline | Category (project) | Replacement method | Evidence |
|---|---|---|---|
| Dir 1.1 | Required | RV | §4 of this record |
| Dir 3.1 | Required | SC, RV | `tools/trace/trace.py --report`; requirement and model trace |
| Dir 4.2, Dir 4.3 | Advisory / Required | RV | Review item R2 |
| Dir 4.14 | Required | RV | Review item R4 |
| Dir 5.1 to Dir 5.3 | Required | RV | Review item R6 |
| *(to be completed from the export)* | | | |

Rows above are the guidelines the GEP already assigns to RV or SC; they are listed so that the GCS can be generated from this table alone.

## 6. Deviations and permits in force

| ID | Kind | Guideline | Scope | Status |
|---|---|---|---|---|
| DP-01 | Permit | Rule 11.4 (adopted) | MCAL, startup, SafeMon ROM checksum pointer | In force |
| DP-02 | Permit | Rule 1.2 (adopted) | `ls_compiler.h` | In force |
| DP-03 | Permit | Adopted generated code | `firmware/dcu/gen/`, `libs/ls_common/gen/` | In force |
| DP-04 | Permit | Dir 4.9 (adopted) | `ls_compiler.h`, `ls_static_assert.h`, `det.h`, MCAL register-instance macros | In force |
| DP-05 | Permit | Visual State engines | `firmware/dcu/gen_vs/release/` | Guideline list pending (check U5) |
| DEV-DCU-nnn | Deviation | — | — | None approved |

The [deviation register](../../08_process/misra/deviations.yaml) is the source of truth; this table is a summary. In-code suppressions use the single-line form `/*cstat !<tag> : DEV-DCU-nnn <reason>*/` and are never placed in generated code.

## 7. Compliance status per release

| Release | C-STAT result (Hil, Release) | Deviations | Permits | GCS |
|---|---|---|---|---|
| 0.1.0-dev | Not yet analysed (no IAR build) | 0 | DP-01 to DP-05 | — |

## 8. Rationale

- **One record per product.** The GEP is shared process; the implementation-defined behaviour and the C-STAT coverage depend on the DCU toolchain and code, so they are kept with the DCU documents and reviewed with them.
- **Static assertions for implementation-defined behaviour.** A compile-time check fails the build on a toolchain that behaves differently, so the claim cannot silently lose its basis.
- **Enumeration size not relied on.** IAR and `arm-none-eabi-gcc` use short enumerations while host compilers use 4 bytes; fixed-width typedefs keep layouts identical on target and host tests.

## 9. References

- [Guideline Enforcement Plan](../../08_process/misra/gep.md) (LS-PRC-004), [deviation process](../../08_process/misra/deviation_process.md), [deviation register](../../08_process/misra/deviations.yaml)
- [Coding standard](../../08_process/coding_standard.md), [Toolchains](../../08_process/toolchains.md), [Lab Host](../../08_process/lab_host.md)
- [DCU software architecture](architecture.md) (LS-DCU-SAD-001), [IAR project setup guide](iar_project_setup.md) (LS-DCU-GDE-002), [Visual State modelling guide](visual_state_guide.md) (LS-DCU-GDE-001)
- [ADR 0008: MISRA guideline enforcement plan](../../adr/0008-misra-guideline-enforcement-plan.md)
- MISRA C:2012 with Amendments 1 to 4; MISRA C:2023; MISRA Compliance:2020; ISO/IEC 9899:2011 Annex J.3
- IAR C/C++ Development Guide for Arm (implementation-defined behaviour chapter); IAR C-STAT Static Analysis Guide
