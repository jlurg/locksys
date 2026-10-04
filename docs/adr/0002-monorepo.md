---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# Monorepo

## Context and Problem Statement

LockSys consists of the DCU firmware, the CGW firmware, the Flutter APP, the HIL framework, shared interface definitions (CAN matrix, APP protocol, enumerations, timing parameters, DTC catalogue, test vectors), shared C libraries used by both firmwares, repository tools and documentation. Generated code is committed and checked by a regenerate-and-compare gate. Interface changes affect a producer and a consumer at the same time. How is the code organised in repositories?

## Decision Drivers

- One source of truth for every interface, consumed by all nodes.
- Interface changes and their implementations on both nodes reviewed and merged atomically.
- One CI configuration and one set of required checks.
- One system version and one release containing all components.
- Requirement-to-code-to-test traceability across all levels in one place.
- Low administrative overhead for a single maintainer.

## Considered Options

- One repository for all components (monorepo)
- One repository per node plus an interface repository
- Several repositories joined by git submodules

## Decision Outcome

Chosen option: "One repository for all components (monorepo)", because it is the only option in which an interface change, its regenerated code and the changes on both nodes form one reviewed, atomic change checked by one code generation gate.

The top-level layout is `interfaces/`, `libs/`, `third_party/`, `firmware/dcu/`, `firmware/cgw/`, `app/`, `hil/`, `tools/` and `docs/`. CI selects jobs by path inside the workflows, and `interfaces/` changes are labelled `interface-change`.

### Consequences

- Good, because the code generation check (`uv run tools/codegen/regen.py --check`) covers every consumer of every interface.
- Good, because one `VERSION` file and one release describe the whole system.
- Bad, because one repository combines several toolchains (IAR, ESP-IDF, Flutter, Python); path-based job selection keeps CI time acceptable.
- Bad, because committed generated code increases the repository size; it is marked `linguist-generated` and no binaries are committed.

### Confirmation

The `codegen` job of `ci.yml` fails on any drift between interface sources and generated code. The repository tree is reviewed against the layout above.

## Pros and Cons of the Options

### One repository for all components (monorepo)

- Good, because atomic cross-node changes are possible.
- Good, because tooling, CI and documentation are shared.
- Bad, because the CI must avoid running every job for every change.

### One repository per node plus an interface repository

- Good, because each repository has a single toolchain.
- Bad, because interface changes need coordinated changes in several repositories and version pinning between them.
- Bad, because traceability and releases span several repositories.

### Several repositories joined by git submodules

- Good, because components can be versioned separately.
- Bad, because submodule pointers add a manual synchronisation step to every interface change.

## More Information

- [Documentation index](../README.md)
- [CI/CD](../08_process/ci_cd.md)
