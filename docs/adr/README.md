# Architecture decision records

| Field | Value |
| --- | --- |
| Document ID | LS-ADR-000 |
| Version | 1.0 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This directory holds the architecture decision records (ADRs) of LockSys in MADR 4.0.0 format, as decided in [ADR 0001](0001-record-architecture-decisions-with-madr.md). New ADRs start from the [template](adr-template.md), take the next free number and are added to the index below in the same pull request.

## Index

| ADR | Title | Status | Date |
| --- | --- | --- | --- |
| [0001](0001-record-architecture-decisions-with-madr.md) | Record architecture decisions with MADR | Accepted | 2026-10-03 |
| [0002](0002-monorepo.md) | Monorepo | Accepted | 2026-10-03 |
| [0003](0003-gitflow-lite-with-develop-as-default-branch.md) | GitFlow-lite with develop as default branch | Accepted | 2026-10-03 |
| [0004](0004-lab-host-self-hosted-runner-trust-model.md) | Lab Host self-hosted runner trust model | Accepted | 2026-10-03 |
| [0005](0005-apache-2-0-licence.md) | Apache-2.0 licence | Accepted | 2026-10-03 |
| [0006](0006-version-file-semver-and-interface-versions.md) | VERSION file, SemVer and interface versions | Accepted | 2026-10-03 |
| [0007](0007-pin-esp-idf-v5-5-5.md) | Pin ESP-IDF v5.5.5 | Accepted | 2026-10-03 |
| [0008](0008-misra-guideline-enforcement-plan.md) | MISRA guideline enforcement plan | Accepted | 2026-10-03 |
| [0009](0009-vendor-nanopb-and-qrcodegen.md) | Vendor nanopb and qrcodegen | Accepted | 2026-10-03 |
| [0010](0010-pin-actions-by-commit-sha.md) | Pin actions by commit SHA | Accepted | 2026-10-03 |
| [0011](0011-markdown-docs-now-sphinx-later.md) | Markdown docs now, Sphinx later | Accepted | 2026-10-03 |
| [0012](0012-mac-workstation-with-windows-lab-host-for-iar.md) | Mac workstation with Windows Lab Host for IAR (VS Code Remote-SSH) | Accepted | 2026-10-03 |
| [0013](0013-iar-visual-state-for-dcu-state-machines.md) | IAR Visual State for DCU state machines (Classic Coder, readable code) | Accepted | 2026-10-03 |
| [0014](0014-module-based-bench-hardware-and-free-spinning-encoder-motor.md) | Module-based bench hardware and free-spinning encoder motor for stage A | Accepted | 2026-10-03 |

## Rules

- File name: `NNNN-title-with-dashes.md`, numbered sequentially.
- Status values: `proposed`, `accepted`, `rejected`, `deprecated`, `superseded by ADR-NNNN`.
- Accepted ADRs are not rewritten. A changed decision is recorded in a new ADR that supersedes the earlier one; the earlier ADR receives only the status change.
