---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# MISRA guideline enforcement plan

## Context and Problem Statement

The DCU firmware implements the safety interlocks of the system and is written in C. The IAR licence on the Lab Host is expected to include C-STAT (confirmed by the day-1 checks). C-STAT offers rule sets for MISRA C:2012 and MISRA C:2023 (MISRA C:2012 with Amendments 1 to 4 consolidated), but implements checks for selected guidelines only. Part of the DCU image is generated: CAN and DTC code by cantools and the state machine engines by IAR Visual State. C-STAT is not qualified for the standard licence. Which MISRA claim is made, and how is it enforced?

## Decision Drivers

- A compliance claim that can be backed for every guideline, not only for those a tool checks.
- A zero-finding gate that is easy to audit.
- Generated code covered by the claim, because it runs on the target.
- A small number of analysers for a single maintainer.
- Cloud CI that keeps working without the IAR licence.

## Considered Options

- Claim MISRA C:2012 with Amendments 1 to 4, enforced through a Guideline Enforcement Plan with C-STAT as authoritative checker
- Claim MISRA C:2023
- Use MISRA as guidance only, without a compliance claim
- Rely on the cppcheck MISRA addon

## Decision Outcome

Chosen option: "Claim MISRA C:2012 with Amendments 1 to 4, enforced through a Guideline Enforcement Plan with C-STAT as authoritative checker", because it states a precise, widely recognised edition, uses the consolidated C-STAT rule set that implements it, and the plan assigns every guideline to a defined method, so gaps in tool coverage are closed explicitly.

- The [Guideline Enforcement Plan](../08_process/misra/gep.md) maps every directive and rule to C-STAT, compiler diagnostics, linker checks, project scripts or manual review, with a separate column for Visual State generated code.
- Selected Advisory guidelines are adopted as Required (re-categorisation plan in the GEP).
- The gate in `iar-gate` is zero unsuppressed findings. Deviations use `DEV-DCU-nnn`, `DEV-LIB-nnn` and `DEV-CGW-nnn` records and `DP-nn` permits in a single register, following the [deviation process](../08_process/misra/deviation_process.md).
- Generated code is analysed, not excluded: cantools output for Mandatory and Required guidelines (permit DP-03); Visual State engines with the full gating set and a path-scoped permit DP-05 fixed after the first baseline.
- C-STAT is treated as unqualified: the cppcheck MISRA addon runs as a cross-check, the GCC shadow build compiles all target sources with `-Werror`, and a seeded-defect file is analysed after every EWARM update.
- If the day-1 checks show that C-STAT is not available, the claim is suspended until resolved; the GCC shadow build and the cppcheck addon remain as interim checks.

### Consequences

- Good, because the claim covers every guideline and the complete image.
- Good, because deviations are machine-checked against the code in both directions.
- Bad, because guidelines without a C-STAT check need manual review effort.
- Bad, because the MISRA documents are not freely distributable; rule texts are not stored in the repository.

### Confirmation

`iar-gate` runs the C-STAT gate on every DCU-relevant push. Each release publishes a Guideline Compliance Summary, the C-STAT reports and the deviations in force.

## Pros and Cons of the Options

### MISRA C:2012 with Amendments 1 to 4 and a GEP

- Good, because the edition is precise and matches the C-STAT rule set used.
- Good, because the GEP closes tool coverage gaps explicitly.
- Neutral, because the plan needs maintenance at each IAR update.

### MISRA C:2023

- Good, because it is the newest consolidated edition supported by C-STAT.
- Neutral, because its guideline content is the same as MISRA C:2012 with Amendments 1 to 4, so the choice affects the wording of the claim only.

### MISRA as guidance only

- Good, because it avoids the effort of a plan and deviation records.
- Bad, because there is no verifiable statement about the code.

### cppcheck MISRA addon

- Good, because it runs without a licence on any host.
- Bad, because its coverage is partial and it needs rule texts supplied by the user.

## More Information

- [Guideline Enforcement Plan](../08_process/misra/gep.md)
- [MISRA deviation process](../08_process/misra/deviation_process.md)
- [DCU MISRA compliance record](../04_software/dcu/misra_compliance.md)
- [IAR C-STAT](https://www.iar.com/cstat)
