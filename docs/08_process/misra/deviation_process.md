# MISRA deviation process

| Field | Value |
| --- | --- |
| Document ID | LS-PRC-005 |
| Version | 1.0 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This document defines how deviations from MISRA C guidelines, from the CERT C checks in the gating rule set and from the project design rules of the [coding standard](../coding_standard.md) are requested, approved, applied in code, checked and retired. It applies to the DCU firmware, the shared libraries and the CGW components. The enforcement method for each guideline is defined in the [Guideline Enforcement Plan](gep.md).

## Terms

| Term | Meaning |
| --- | --- |
| Deviation | An approved, documented exception for one or a few specific instances of a guideline violation |
| Deviation permit | An approved, documented exception for a recurring use case, scoped by path or construct |
| Guideline re-categorisation plan | The project categories of the MISRA guidelines, part of the GEP |
| Guideline Compliance Summary (GCS) | Per-release statement of the compliance status of every guideline |

The terms follow MISRA Compliance:2020.

## Identifiers and register

| ID form | Use |
| --- | --- |
| `DEV-DCU-nnn` | Deviation in DCU code |
| `DEV-LIB-nnn` | Deviation in shared library code |
| `DEV-CGW-nnn` | Deviation from the CGW subset |
| `DP-nn` | Deviation permit (project-wide, path-scoped) |

- Numbers are assigned sequentially and never reused.
- All records live in one register: [deviations.yaml](deviations.yaml). Retired and rejected records stay in the register.

## When a deviation is possible

| Guideline category (after re-categorisation) | Deviation |
| --- | --- |
| Mandatory | Never |
| Required, including adopted Advisory guidelines | Allowed with an approved record |
| Advisory, not adopted | No record needed; violations are reported in the GCS |
| Project design rule (no heap, no recursion, no function pointers, no floating point) | Allowed with an approved record and an ADR when the exception is systemic |

A deviation is requested only when all of the following hold:

1. The violation cannot be removed by a reasonable change to the code.
2. For generated code, no generator option and no change to the model or interface source removes it.
3. The consequences of the violation are analysed and a mitigation is defined.
4. The verification of the mitigation is defined (test, review, analysis).

## Record fields

| Field | Content |
| --- | --- |
| `id` | `DEV-DCU-nnn`, `DEV-LIB-nnn`, `DEV-CGW-nnn` or `DP-nn` |
| `kind` | `deviation` or `permit` |
| `guideline` | For example `MISRA C:2012 Rule 11.4`, `CERT C INT31-C`, or `Project rule: no function pointers` |
| `category` | `required`, `advisory` or `project` |
| `cstat_tag` | C-STAT check tag, or a list of tags; glob patterns are allowed, for example `MISRAC2012-Rule-10.4_*`; omitted for CGW records and project rules |
| `scope` | `paths` (list of path patterns) and optional `symbols` |
| `rationale` | Why the guideline cannot be followed in this case |
| `risk` | What can go wrong because of the violation |
| `mitigation` | How the risk is controlled |
| `verification` | How the mitigation is verified |
| `approved_by` | Approver (`jlurg`) |
| `date` | Date of the last status change, `YYYY-MM-DD` |
| `status` | `proposed`, `approved`, `rejected` or `retired` |
| `references` | Optional: issues, ADRs, requirement IDs |

## Procedure

1. **Request.** Add a record with status `proposed` to the register in the PR that needs it, or in a separate `docs(dcu)` PR for a permit. The PR summary explains the request.
2. **Review.** Check the preconditions and the record content. For safety-relevant code the diff is read again at least 12 hours after the last change, as in the [review checklist](../commits_and_prs.md).
3. **Approve or reject.** Set `status`, `approved_by` and `date`. Only `approved` records can be referenced from code or configuration.
4. **Apply.**
   - A deviation in project code is applied with a single-line C-STAT suppression comment on the affected line, naming the check and the record ID:

     ```c
     /*cstat !MISRAC2012-Rule-11.4 : DEV-DCU-003 CMSIS peripheral base address*/
     ```

   - Range suppressions and `#pragma` suppressions are not used.
   - A permit is applied in the C-STAT configuration (`firmware/dcu/cstat/`) as a path-scoped rule change that references the permit ID. Permits are never applied with in-code comments.
   - Generated code never carries suppression comments; it is covered by a permit (DP-03, DP-05).
   - CGW suppressions use the analyser's inline syntax with the record ID in the comment text.
5. **Gate.** The C-STAT gate (`tools/ci/cstat_gate.py`, run in `iar-gate`) fails if:
   - a suppression cites no record ID, an unknown ID, or a record that is not approved;
   - the cited record's `cstat_tag` does not cover the suppressed check;
   - a suppression appears in generated code;
   - an approved `DEV-DCU` or `DEV-LIB` record is not referenced by any suppression.

   The scope of each suppression and the reference of each permit in the C-STAT configuration are checked in review.
6. **Periodic review.** Before each release all approved records are reviewed for continued need and listed in the GCS.
7. **Retire.** When the code no longer needs a record, set `status: retired`. The record stays in the register.

## Planned permits

The following permits are planned for the code written in milestones M1 and M2. They are added to the register, with status `proposed`, by the PR that introduces the code needing them.

| ID | Guideline | Scope | Rationale |
| --- | --- | --- | --- |
| DP-01 | Rule 11.4 (adopted) | `firmware/dcu/src/mcal/`, `firmware/dcu/startup/`, the SafeMon ROM checksum pointer | Peripheral register access through CMSIS base addresses; ROM checksum over a fixed address range |
| DP-02 | Rule 1.2 (adopted) | `ls_compiler.h` | Language extensions for `__no_init`, section placement, call-graph roots and intrinsics, encapsulated in one header |
| DP-03 | Adopted generated code | `firmware/dcu/gen/`, `libs/ls_common/gen/` | Code generated from the DBC and YAML sources; analysed for Mandatory and Required guidelines only |
| DP-04 | Dir 4.9 (adopted) | `ls_compiler.h`, `ls_static_assert.h`, `det.h`, MCAL register-instance macros | Function-like macros needed for compile-time checks, development error reporting and register-instance mapping on target and host |
| DP-05 | Visual State generated engines | `firmware/dcu/gen_vs/release/` | Remaining findings that no coder option removes; guideline list fixed after the first C-STAT baseline (Lab Host check U5); never a Mandatory guideline |

Planned CGW deviations, added at milestone M3 when the code exists:

| ID | Guideline | Scope |
| --- | --- | --- |
| DEV-CGW-001 | Rule 15.1 (advisory) | `goto` only in the ESP-IDF error-cleanup macro pattern inside `*_esp.c` adapters |
| DEV-CGW-002 | Rule 19.2 (advisory) | Tagged unions for active-object event payloads |
| DEV-CGW-003 | Project rule: shared state | C11 atomics in the documented inter-task handoffs |

### Contingency: function pointers in generated engines

If a future Visual State version no longer produced readable code without function pointers, a project-rule deviation would be required for `firmware/dcu/gen_vs/`. It would require: constant dispatch tables in flash covered by the ROM checksum, a complete generated stack-usage description for the linker, a build error on any unresolved indirect call, and host tests that reach every table entry. This contingency is inactive; activating it requires an ADR.

## Rationale

- A single register with machine-readable records lets the C-STAT gate check suppressions and records in both directions, so no suppression exists without an approved justification and no stale record survives unnoticed.
- Single-line suppressions keep each exception visible at its point of use; permits keep recurring, justified patterns out of the code.
- Generated code is covered by permits because suppression comments in it would be lost on regeneration.

## References

- [Guideline Enforcement Plan](gep.md)
- [Deviation register](deviations.yaml)
- [Coding standard](../coding_standard.md)
- [DCU MISRA compliance record](../../04_software/dcu/misra_compliance.md)
- [ADR 0008: MISRA guideline enforcement plan](../../adr/0008-misra-guideline-enforcement-plan.md)
- MISRA Compliance:2020, Achieving compliance with MISRA coding guidelines
