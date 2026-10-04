# MISRA C Guideline Enforcement Plan

| Field | Value |
| --- | --- |
| Document ID | LS-PRC-004 |
| Version | 1.0 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This Guideline Enforcement Plan (GEP) states how every MISRA C guideline is enforced for the code that runs on the DCU. It follows the structure of MISRA Compliance:2020 and is the basis of the Guideline Compliance Summary (GCS) published with each release.

In scope:

| Code | Paths |
| --- | --- |
| DCU project code | `firmware/dcu/src/`, `firmware/dcu/cfg/`, C sources in `firmware/dcu/startup/` |
| Shared libraries as compiled into the DCU | `libs/*/src/`, `libs/*/include/` |
| Generated CAN, DTC, enumeration and parameter code | `firmware/dcu/gen/`, `libs/ls_common/gen/` |
| Visual State generated engines | `firmware/dcu/gen_vs/release/` (gating), `firmware/dcu/gen_vs/debug/` (not gating) |

Out of scope: vendored code in `third_party/` (CMSIS headers are used, not analysed), test code, host-only tools, and assembly sources. The CGW follows the MISRA-inspired subset in the [coding standard](../coding_standard.md).

## Compliance claim

The DCU firmware and the shared C libraries are developed to comply with **MISRA C:2012, including Amendments 1 to 4**, as defined by this GEP. Compliance is checked with IAR C-STAT using its MISRA C:2023 rule set, which consolidates MISRA C:2012 with Amendments 1 to 4, complemented by compiler diagnostics, project scripts and manual review. Deviations are handled according to the [deviation process](deviation_process.md).

LockSys is a bench demonstrator. The claim is not assessed by a third party.

## Enforcement methods

| Code | Method | Gating |
| --- | --- | --- |
| CS | IAR C-STAT check in the `iar-gate` build | Yes: zero unsuppressed findings |
| CC | Compiler diagnostic: IAR iccarm with warnings as errors and required prototypes; GCC shadow build with `-Werror -Wconversion -Wsign-conversion` | Yes |
| LK | Linker and map file check: no heap symbols, stack usage, call graph | Yes |
| SC | Project script: `tools/arch/check_layers.py`, `tools/vs/` checks, C-STAT gate cross-checks | Yes |
| CP | cppcheck MISRA addon on GitHub-hosted runners | No: cross-check, findings reviewed |
| RV | Manual review against the review items below | Yes: PR review checklist |
| CO | Visual State coder option in the committed option files | Yes: options are part of the reviewed configuration |
| MD | Visual State model check: Verificator and `tools/vs/check_vs_model.py` | Yes |
| NA | Not applicable: the language feature or library is not used | Usage is detected by CS or SC |

## Guideline re-categorisation plan

| MISRA category | Project category | Guidelines | Handling |
| --- | --- | --- | --- |
| Mandatory | Mandatory | All | Never deviated |
| Required | Required | All | Deviation only with an approved record |
| Advisory | Required (adopted) | Dir 4.9; Rules 1.2, 2.5, 2.7, 8.7, 8.9, 11.4, 12.1, 13.3, 15.1, 15.5, 17.8, 19.2 | Gating; deviation only with an approved record or permit |
| Advisory | Advisory | All other advisory guidelines | Not in the gating rule set; checked by an informative C-STAT run before each release and reported in the GCS |

The CERT C subset and the C-STAT standard checks selected in the C-STAT configuration (`firmware/dcu/cstat/`) are part of the gating rule set for project code. Their deviations use the same records as MISRA deviations.

## Analysis configuration

| Scope (C-STAT path match) | Checks | Gating |
| --- | --- | --- |
| `firmware/dcu/src/`, `firmware/dcu/cfg/`, `libs/*/src/`, `libs/*/include/` | Mandatory, Required and adopted Advisory guidelines; CERT C subset; standard checks | Zero unsuppressed findings |
| `firmware/dcu/gen/`, `libs/ls_common/gen/` | Mandatory and Required guidelines, under permit DP-03 (adopted generated code) | Zero unsuppressed findings |
| `firmware/dcu/gen_vs/release/` | Mandatory, Required and adopted Advisory guidelines; standard checks | Mandatory and Required: zero unsuppressed findings. Adopted Advisory: reported |
| `firmware/dcu/gen_vs/debug/` | As `gen_vs/release/` | Reported only (debug instrumentation) |
| `third_party/`, `test/`, `libs/*/test/` | None | Not analysed |

- C-STAT analyses the Release configuration and every other non-Debug target configuration with the same rule set.
- The C-STAT command line depends on the EWARM line recorded in `tools/versions.env`: `iarbuild -cstat_analyze` and `-cstat_report` on 9.70.x; a compilation database with `iarbuild -E cstat_analyze` (or `-cstat_cmds` with `icstat` and `ireport`) on 10.10.x. The build scripts detect the version.
- The rule set is stored in `firmware/dcu/cstat/` and reviewed like code.

## Enforcement per guideline

The table assigns an enforcement method to every guideline, grouped by section. A method listed for a range applies to every guideline in the range unless a narrower entry overrides it.

| Guidelines | Project code (`src`, `cfg`, `libs`) | Generated code (`firmware/dcu/gen/`, `libs/ls_common/gen/`) | Visual State engines (`gen_vs/release/`) | Notes |
| --- | --- | --- | --- | --- |
| Dir 1.1 | RV | RV | RV | Implementation-defined behaviour of the IAR compiler is documented in the DCU MISRA compliance record |
| Dir 2.1 | CC | CC | CC | Builds with warnings as errors |
| Dir 3.1 | SC, RV | NA | MD, RV | Trace tags checked by `tools/trace/trace.py`; engine transitions traced through the transition IDs of the model |
| Dir 4.1 | CS, RV | CS | CS, MD | Run-time failure review item R3 |
| Dir 4.2, Dir 4.3 | RV | NA | NA | Assembly only in `startup/`; intrinsics only through `ls_compiler.h` |
| Dir 4.4 to Dir 4.8 | CS, RV | CS | CS | Dir 4.6: coder option `-typestyle1` (CO) for the engines |
| Dir 4.9 | CS (adopted), permit DP-04 | CS | CS, DP-05 candidate | |
| Dir 4.10, Dir 4.11, Dir 4.13 | CS, RV | CS | CS | Review item R5 for resource sequences |
| Dir 4.12 | CS, LK | CS, LK | CO (`-useheap0`), LK | Map check rejects heap symbols |
| Dir 4.14 | RV | NA | NA | Review item R4: external inputs (CAN, UART, UDS, I2C, ADC) are validated |
| Dir 4.15 | NA | NA | NA | No floating point (coding standard) |
| Dir 5.1 to Dir 5.3 | RV | NA | NA | No C11 threads; ISR and task sharing reviewed under item R6 |
| Rules 1.1 to 1.5 | CC, CS; Rule 1.2 adopted with permit DP-02 | CS | CS | Language extensions only in `ls_compiler.h` |
| Rules 2.1 to 2.8 | CS; Rules 2.5 and 2.7 adopted | CS | CS; Rules 2.3, 2.4, 2.5 DP-05 candidates | |
| Rules 3.1, 3.2, 4.1, 4.2 | CS | CS | CS | |
| Rules 5.1 to 5.9 | CS, CC | CS | CS, SC | One generated system header per translation unit (`check_layers.py`) |
| Rules 6.1 to 6.3, 7.1 to 7.6 | CS | CS | CS | |
| Rules 8.1 to 8.17 | CS, CC; Rules 8.7 and 8.9 adopted | CS | CS; Rules 8.7, 8.9 DP-05 candidates | Prototypes required by the compiler |
| Rules 9.1 to 9.7 | CS | CS | CS | |
| Rules 10.1 to 10.8 | CS, CC | CS | CS; coder option `-D2` (CO) | GCC shadow build adds `-Wconversion -Wsign-conversion` |
| Rules 11.1 to 11.10 | CS; Rule 11.4 adopted with permit DP-01 | CS | CS, CO, SC | Readable Classic Coder output has no function pointers |
| Rules 12.1 to 12.6 | CS; Rule 12.1 adopted | CS | CS | |
| Rules 13.1 to 13.6 | CS; Rule 13.3 adopted | CS | CS | |
| Rules 14.1 to 14.4 | CS | CS | CS | |
| Rules 15.1 to 15.7 | CS; Rules 15.1 and 15.5 adopted | CS | CS; Rule 15.5 DP-05 candidate | |
| Rules 16.1 to 16.7 | CS | CS | CS | |
| Rules 17.1 to 17.13 | CS, LK (Rule 17.2); Rule 17.8 adopted | CS | CS; Rule 17.1: MD and SC, DP-05 candidate | Engines are declared variadic; review item V5 |
| Rules 18.1 to 18.10 | CS, RV | CS | CS | |
| Rules 19.1, 19.2 | CS; Rule 19.2 adopted | CS | CS | |
| Rules 20.1 to 20.14 | CS | CS | CS; DP-05 candidates | |
| Rules 21.1 to 21.26 | CS, LK | CS | CS, LK | No heap, no `<stdio.h>`, no `<signal.h>` in target code |
| Rules 22.1 to 22.20 | CS; NA for file and thread rules | CS | CS | No file I/O and no C11 threads in target code |
| Rules 23.1 to 23.8 | NA | NA | NA | No generic selections; usage detected by CS |

### Guidelines without C-STAT coverage

C-STAT implements checks for selected guidelines only. At the Lab Host day-1 check the list of enabled checks is exported (`iarbuild -cstat_checks` on EWARM 10.10.x, or the C-STAT check list exported from the project on 9.70.x). Every guideline in the table that relies on CS but has no check in that list is reassigned explicitly to CC, LK, SC or RV. The resulting per-guideline list is kept in the [DCU MISRA compliance record](../../04_software/dcu/misra_compliance.md) and reviewed at every IAR update.

## Review items

Guidelines assigned to RV are checked with these items in every PR that touches the code in scope.

| ID | Review item |
| --- | --- |
| R1 | Implementation-defined behaviour relied on by the code is listed in the compliance record (Dir 1.1). |
| R2 | Assembly and compiler intrinsics appear only in `startup/` and `ls_compiler.h` (Dir 4.2, 4.3). |
| R3 | Arithmetic cannot overflow, divide by zero or index out of bounds for the specified input ranges; the ranges are stated in the requirements or the interface contract (Dir 4.1, Rule 1.3). |
| R4 | Data from CAN, UART, UDS, I2C and ADC is range-checked and plausibility-checked before use (Dir 4.14). |
| R5 | Initialisation and shutdown sequences of peripherals and resources are complete and ordered (Dir 4.13). |
| R6 | Every object shared between an ISR and a task has a single writer or is accessed inside a critical section (Dir 5.1 to 5.3). |
| R7 | Error information returned by called functions is tested (Dir 4.7). |

## Visual State generated code

The engines generated by IAR Visual State ([ADR 0013](../../adr/0013-iar-visual-state-for-dcu-state-machines.md)) are analysed like project code; they are not excluded.

- Coder options that remove findings at the source are mandatory: `-api_type0 -readable1` (no function pointers), `-useheap0`, `-maximummisra1`, `-typestyle1`, `-D2`, `-generatetimeandversion0`, `-warnings_are_errors1`. The option files in `firmware/dcu/model/visualstate/options/` are the source of truth.
- In-code suppression comments are forbidden in `gen_vs/`, because regeneration removes them. Findings that remain are covered by the path-scoped permit **DP-05**, whose guideline list is fixed after the first C-STAT baseline of the generated code (Lab Host check U5).
- A finding is permitted under DP-05 only if no coder option or model change removes it and the guideline is not Mandatory.
- If a Mandatory guideline, or a Required guideline with real risk (for example Rules 13.2 or 17.2), is violated and cannot be removed, the affected state machine changes to "model as specification": the model remains the design and validation artefact, and the C code is written by hand from it.

Additional review items for the engines:

| ID | Review item | Automated by |
| --- | --- | --- |
| V1 | No `va_arg` in generated code | `tools/vs/check_vs_gen.py` |
| V2 | No `malloc` or `free` | Coder option `-useheap0`; map check |
| V3 | Every action prototype is implemented exactly once | Link; review |
| V4 | Generated enumeration values match the interface contract | `tools/vs/check_vs_constants.py`; static assertions |
| V5 | Callers of `<System>VSDeduct` pass exactly one argument; models have no event parameters | `tools/vs/check_vs_model.py`; review |

## Evidence

Each release attaches to its immutable GitHub release:

- the Guideline Compliance Summary, generated from the enabled-check list, this GEP, the deviation register and the zero-finding C-STAT result;
- the C-STAT HTML report and SARIF output for each analysed configuration;
- the list of approved deviations and permits in force.

## Tool confidence

The C-STAT certification stated by IAR covers functional-safety editions only; the standard EWARM licence used here is treated as unqualified. Mitigations:

- the cppcheck MISRA addon runs as a cross-check on GitHub-hosted runners;
- the GCC shadow build compiles all target sources with `-Werror`;
- a seeded-defect file with known violations is analysed after every EWARM update, and the expected findings are compared.

## Rationale

- C-STAT implements selected guidelines only, so a compliance claim needs an explicit method for every guideline. The table makes the gaps visible and assigns them to compiler, linker, scripts or review.
- Analysing generated code, rather than excluding it, keeps the claim valid for the complete image: the generated engines decide when actuators are driven.
- Adopting selected Advisory guidelines as Required supports the structural metrics (single exit, no `goto`) and the encapsulation rules the DCU design relies on.

## References

- [MISRA deviation process](deviation_process.md)
- [Deviation register](deviations.yaml)
- [Coding standard](../coding_standard.md)
- [DCU MISRA compliance record](../../04_software/dcu/misra_compliance.md)
- [ADR 0008: MISRA guideline enforcement plan](../../adr/0008-misra-guideline-enforcement-plan.md)
- [ADR 0013: IAR Visual State for DCU state machines](../../adr/0013-iar-visual-state-for-dcu-state-machines.md)
- MISRA C:2012 Guidelines for the use of the C language in critical systems, with Amendments 1 to 4
- MISRA C:2023, MISRA Compliance:2020
- [IAR C-STAT](https://www.iar.com/cstat)
