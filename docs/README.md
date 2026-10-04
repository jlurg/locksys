# LockSys documentation

| Field | Value |
| --- | --- |
| Document ID | LS-DOC-001 |
| Version | 1.1 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This is the index of the LockSys documentation. It lists every document, defines the documentation conventions and the identifier formats, and states how documents are changed. The documentation is organised by development process, following the V-model.

## Structure

| Folder | Content | Process area |
| --- | --- | --- |
| `00_project/` | Roadmap, glossary, bill of materials | Project management (MAN.3) |
| `01_stakeholder/` | Stakeholder requirements | Requirements elicitation (SYS.1) |
| `02_system/` | System architecture and interface contract, system requirements, amendment register | System requirements and architecture (SYS.2, SYS.3) |
| `03_interfaces/` | CAN matrix, APP protocol, UART telemetry, DTC catalogue | Interface specification |
| `04_software/` | Software requirements and architecture per node and for the shared libraries | Software requirements, architecture and design (SWE.1 to SWE.3) |
| `05_safety/` | Safety concept, FMEA-lite | Functional safety (ISO 26262-inspired) |
| `06_security/` | Security concept, TARA-lite | Cybersecurity (ISO/SAE 21434-inspired) |
| `07_verification/` | Verification strategy, HIL architecture, HIL test catalogue, manual procedures | Verification (SWE.4 to SWE.6, SYS.4, SYS.5) |
| `08_process/` | Branching, commits and PRs, coding standard, MISRA, release, CI/CD, GitHub settings, Lab Host, toolchains, AI policy | Supporting processes (SUP.1, SUP.8, SUP.9, SUP.10) |
| `09_releases/` | Release records | Configuration management (SUP.8) |
| `adr/` | Architecture decision records | Decision records |

## Document index

### Project

| Document | ID | Content |
| --- | --- | --- |
| [Roadmap](00_project/roadmap.md) | LS-PRJ-001 | Milestones M0 to M6, estimates, window stages B to E, MVP and LATER, project risks |
| [Glossary](00_project/glossary.md) | LS-PRJ-002 | Terms and abbreviations |
| [Bench and HIL bill of materials](00_project/bom.md) | LS-PRJ-003 | Node and bench hardware, USB-CAN purchase requirements, stimulus MCU recommendation, motor buying checklist, bench safety checklist |

### Stakeholder and system

| Document | ID | Content |
| --- | --- | --- |
| [Stakeholder requirements](01_stakeholder/stakeholder_requirements.md) | LS-STK-001 | Stakeholder requirements `STK-nnn` |
| [System architecture and interface contract](02_system/LS-SAIC.md) | LS-SAIC-001 | Nodes, hardware architecture, pin allocation, clocks and CAN timing, functional chains and timing budgets, modes, interfaces |
| [System requirements](02_system/system_requirements.md) | LS-SRS-001 | System requirements `SYS-nnn` and hardware requirements |
| [Amendment register](02_system/amendment_register.md) | LS-SAIC-001-AR | Consolidated changes to the interface contract |

### Interfaces

The normative interface sources are in `interfaces/`; generated code is derived from them. These documents describe the interfaces.

| Document | ID | Content |
| --- | --- | --- |
| [CAN matrix](03_interfaces/can_matrix.md) | LS-IF-001 | Frames, signals, E2E protection, timing, bus-off handling |
| [APP protocol](03_interfaces/app_protocol.md) | LS-IF-002 | Wi-Fi access point, WebSocket framing, session, pairing, messages |
| [UART telemetry](03_interfaces/uart_telemetry.md) | LS-IF-003 | DCU telemetry sentences on the ST-LINK virtual COM port |
| [DTC catalogue](03_interfaces/dtc_catalog.md) | LS-IF-004 | Diagnostic trouble codes, severities and reactions |

### Software

| Document | ID | Content |
| --- | --- | --- |
| [DCU software architecture](04_software/dcu/architecture.md) | LS-DCU-SAD-001 | Layers, modules, scheduler, state machines, resources |
| [DCU software requirements](04_software/dcu/software_requirements.md) | LS-DCU-SRS-001 | `SWR-DCU-nnn` |
| [Visual State modelling guide](04_software/dcu/visual_state_guide.md) | LS-DCU-GDE-001 | Modelling and generation rules for WinCtrl, DoorCtrl and ModeMgr |
| [IAR project setup guide](04_software/dcu/iar_project_setup.md) | LS-DCU-GDE-002 | Creating the IAR workspace and project on the Lab Host |
| [DCU MISRA compliance record](04_software/dcu/misra_compliance.md) | LS-DCU-MCR-001 | Implementation-defined behaviour, per-guideline coverage, compliance status |
| [CGW software architecture](04_software/cgw/architecture.md) | LS-CGW-SAD-001 | Components, tasks, ports and adapters, security functions |
| [CGW software requirements](04_software/cgw/software_requirements.md) | LS-CGW-SRS-001 | `SWR-CGW-nnn` |
| [APP software architecture](04_software/app/architecture.md) | LS-APP-SAD-001 | Features, layers, state management, platform integration |
| [APP software requirements](04_software/app/software_requirements.md) | LS-APP-SRS-001 | `SWR-APP-nnn` |
| [Shared libraries architecture](04_software/libs/architecture.md) | LS-LIB-SAD-001 | `ls_e2e` and `ls_common`, `SWR-LIB-nnn` |

### Safety and security

| Document | ID | Content |
| --- | --- | --- |
| [Safety concept](05_safety/safety_concept.md) | LS-SAF-001 | Hazards, safety goals, safe states, safety mechanisms |
| [FMEA-lite](05_safety/fmea_lite.md) | LS-SAF-002 | Failure modes and effects of the safety-relevant functions |
| [Security concept](06_security/security_concept.md) | LS-SEC-001 | Assets, threats, cybersecurity goals and requirements, controls |
| [TARA-lite](06_security/tara_lite.md) | LS-SEC-002 | Threat analysis and risk assessment |

### Verification

| Document | ID | Content |
| --- | --- | --- |
| [Verification strategy](07_verification/verification_strategy.md) | LS-VER-001 | Test levels, methods, coverage targets, trace gates |
| [HIL architecture](07_verification/hil_architecture.md) | LS-HIL-001 | Bench topologies, configurations, framework, CI integration |
| [HIL test catalogue](07_verification/hil_test_catalog.md) | LS-HIL-002 | Automated HIL tests and their requirements |
| [Manual test procedures](07_verification/procedures/README.md) | LS-VER-002 | Index of manual and semi-automatic procedures |

### Process

| Document | ID | Content |
| --- | --- | --- |
| [Branching model](08_process/branching.md) | LS-PRC-001 | Branch types, merge methods, release freeze, hotfix, back-merge, tags, definition of stable |
| [Commits and pull requests](08_process/commits_and_prs.md) | LS-PRC-002 | Conventional Commits, PR rules, review checklist, DoR and DoD |
| [Coding standard](08_process/coding_standard.md) | LS-PRC-003 | C, Dart, Python, PowerShell and shell rules; comment rules |
| [MISRA Guideline Enforcement Plan](08_process/misra/gep.md) | LS-PRC-004 | Enforcement method per guideline, including generated code |
| [MISRA deviation process](08_process/misra/deviation_process.md) | LS-PRC-005 | Deviation and permit workflow and record format |
| [Deviation register](08_process/misra/deviations.yaml) | LS-PRC-005 | Approved deviations and permits |
| [Release process](08_process/release_process.md) | LS-PRC-006 | Versions, release checklist, artefacts, evidence |
| [CI/CD](08_process/ci_cd.md) | LS-PRC-007 | Workflows, required checks, workflow security, pinning |
| [GitHub settings](08_process/github_settings.md) | LS-PRC-008 | Repository settings, rulesets, security features, bootstrap order, break-glass |
| [Lab Host](08_process/lab_host.md) | LS-PRC-009 | Workstation and Lab Host workflow, runners, runner security, day-1 checks |
| [Toolchains](08_process/toolchains.md) | LS-PRC-010 | Version pins and update policy |
| [AI policy](08_process/ai_policy.md) | LS-PRC-011 | Rules for AI-assisted development and their enforcement |

### Releases and decisions

| Document | ID | Content |
| --- | --- | --- |
| [Release records](09_releases/README.md) | LS-REL-000 | Index of releases and record template |
| [Architecture decision records](adr/README.md) | LS-ADR-000 | Index of ADRs |

| ADR | Title |
| --- | --- |
| [0001](adr/0001-record-architecture-decisions-with-madr.md) | Record architecture decisions with MADR |
| [0002](adr/0002-monorepo.md) | Monorepo |
| [0003](adr/0003-gitflow-lite-with-develop-as-default-branch.md) | GitFlow-lite with develop as default branch |
| [0004](adr/0004-lab-host-self-hosted-runner-trust-model.md) | Lab Host self-hosted runner trust model |
| [0005](adr/0005-apache-2-0-licence.md) | Apache-2.0 licence |
| [0006](adr/0006-version-file-semver-and-interface-versions.md) | VERSION file, SemVer and interface versions |
| [0007](adr/0007-pin-esp-idf-v5-5-5.md) | Pin ESP-IDF v5.5.5 |
| [0008](adr/0008-misra-guideline-enforcement-plan.md) | MISRA guideline enforcement plan |
| [0009](adr/0009-vendor-nanopb-and-qrcodegen.md) | Vendor nanopb and qrcodegen |
| [0010](adr/0010-pin-actions-by-commit-sha.md) | Pin actions by commit SHA |
| [0011](adr/0011-markdown-docs-now-sphinx-later.md) | Markdown docs now, Sphinx later |
| [0012](adr/0012-mac-workstation-with-windows-lab-host-for-iar.md) | Mac workstation with Windows Lab Host for IAR (VS Code Remote-SSH) |
| [0013](adr/0013-iar-visual-state-for-dcu-state-machines.md) | IAR Visual State for DCU state machines (Classic Coder, readable code) |
| [0014](adr/0014-module-based-bench-hardware-and-free-spinning-encoder-motor.md) | Module-based bench hardware and free-spinning encoder motor for stage A |

## Documentation conventions

- Documents are Markdown, compatible with MyST, and render on GitHub ([ADR 0011](adr/0011-markdown-docs-now-sphinx-later.md)). Diagrams use Mermaid.
- Language is English; the register is that of engineering documents: requirements, design, rationale and decisions. Documents contain no tutorial or educational prose.
- Each document starts with a header table (Document ID, Version, Status, Owner), followed by purpose and scope, the normative content, the rationale and the references. ADRs use the MADR 4.0.0 structure instead.
- Status values: `Draft`, `Approved`, `Released` (release records), `Superseded`. ADRs use the MADR status values.
- Versions are `major.minor`. Documents below version 1.0 are drafts.
- Requirements, safety and security items and tests are written with their identifiers in canonical form, so that the trace tool can find them.

## Identifiers

### Document identifiers

| Prefix | Documents |
| --- | --- |
| `LS-DOC` | Documentation index |
| `LS-PRJ` | Project documents in `00_project/` |
| `LS-STK` | Stakeholder requirements |
| `LS-SAIC` | System architecture and interface contract; `LS-SAIC-001-AR` is its amendment register |
| `LS-SRS` | System requirements specification |
| `LS-IF` | Interface descriptions in `03_interfaces/` |
| `LS-DCU-SAD`, `LS-CGW-SAD`, `LS-APP-SAD`, `LS-LIB-SAD` | Software architecture documents |
| `LS-DCU-SRS`, `LS-CGW-SRS`, `LS-APP-SRS` | Software requirements specifications |
| `LS-DCU-GDE` | DCU development guides (Visual State modelling, IAR project setup) |
| `LS-DCU-MCR` | DCU MISRA compliance record |
| `LS-SAF` | Safety concept (`-001`) and FMEA-lite (`-002`) |
| `LS-SEC` | Security concept (`-001`) and TARA-lite (`-002`) |
| `LS-VER` | Verification strategy (`-001`) and manual procedure index (`-002`) |
| `LS-HIL` | HIL architecture (`-001`) and HIL test catalogue (`-002`) |
| `LS-PRC` | Process documents in `08_process/` |
| `LS-REL` | Release records |
| `LS-ADR` | ADR index; individual ADRs are identified by their number |

A new document takes the next free number under the prefix of its folder and is added to this index in the same pull request.

### Requirement, safety, security and test identifiers

| Item | Format |
| --- | --- |
| Stakeholder requirement | `STK-nnn` |
| System requirement | `SYS-nnn` |
| Hardware requirement | `HWR-nnn` |
| Software requirement | `SWR-DCU-nnn`, `SWR-CGW-nnn`, `SWR-APP-nnn`, `SWR-LIB-nnn` |
| Hazard, safety goal, safety mechanism | `HAZ-nn`, `SG-nn`, `SM-nn`; safety requirements carry the tag `[SAF]` |
| Asset, threat scenario, cybersecurity goal, cybersecurity requirement | `AS-nn`, `TS-nn`, `CSG-nn`, `CSR-nnn`; security requirements carry the tag `[SEC]` |
| Test | `TST-<level>-<scope>-nnn`, level `UT`, `IT`, `HIL` or `MAN`, scope `DCU`, `CGW`, `APP`, `SYS` or `LIB` |
| MISRA deviation and permit | `DEV-DCU-nnn`, `DEV-LIB-nnn`, `DEV-CGW-nnn`, `DP-nn` |
| Diagnostic trouble code | 3 bytes: SAE J2012 code and failure type byte; DCU `B1A`/`U1A`, CGW `B1B`/`U1B` |

### Trace tags

| Context | Form |
| --- | --- |
| C implementation | `/* @satisfies SWR-DCU-012 */` |
| C test | `/* @verifies SWR-DCU-012 */` |
| Python test | `@pytest.mark.verifies("SYS-021")` |
| Dart test | `// @verifies SWR-APP-012` |

The trace matrix is produced with `uv run tools/trace/trace.py --report`.

## Change control

- Documents change through pull requests, like code; documentation-only changes use the commit type `docs`.
- A requirement change is merged as a documentation PR, with its impact analysis, before the implementation PR.
- The interface sources in `interfaces/` are normative. A change to them updates the generated code and the interface documents in the same PR.
- Decisions with long-term consequences are recorded as ADRs.
- CI checks Markdown lint and spelling in the `lint` job, and relative links and the trace report in the `docs` job ([CI/CD](08_process/ci_cd.md)).

## References

- [ADR 0001: Record architecture decisions with MADR](adr/0001-record-architecture-decisions-with-madr.md)
- [ADR 0011: Markdown docs now, Sphinx later](adr/0011-markdown-docs-now-sphinx-later.md)
- [Commits and pull requests](08_process/commits_and_prs.md)
- [Coding standard](08_process/coding_standard.md)
