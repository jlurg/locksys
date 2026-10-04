---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# Apache-2.0 licence

## Context and Problem Statement

The repository is public. Its code may be read, reused and possibly contributed to. It vendors third-party code (CMSIS-Core and the STM32F1 CMSIS device files under Apache-2.0, nanopb under the zlib licence, the Nayuki QR Code generator under the MIT licence) and builds on ESP-IDF (Apache-2.0) and Flutter (BSD-style licences). Which licence applies to the project's own code and documentation?

## Decision Drivers

- Permissive reuse, including in commercial contexts.
- An explicit patent licence from contributors.
- Compatibility with all vendored and linked components.
- A clear mechanism for attribution notices.

## Considered Options

- Apache License 2.0
- MIT licence
- BSD 3-Clause licence
- GNU GPL v3
- No licence (all rights reserved)

## Decision Outcome

Chosen option: "Apache License 2.0", because it is permissive, contains an explicit patent grant and a NOTICE mechanism, and is the licence of the main embedded dependencies (ESP-IDF, CMSIS).

Rules:

- `LICENSE` contains the licence text; `NOTICE` holds the project notice; `THIRD_PARTY_NOTICES.md` lists vendored components and their licences.
- Authored source files carry SPDX headers as defined in the [coding standard](../08_process/coding_standard.md). Markdown documentation carries no header.
- Vendored and generated files keep their upstream or generator notices.
- Contributions are accepted under the same licence (Apache-2.0, section 5). No contributor licence agreement and no sign-off are required during the MVP.

### Consequences

- Good, because users and contributors get a clear patent grant.
- Good, because the licence is compatible with every vendored component.
- Bad, because every authored file needs a header and the notice files must be kept current.

### Confirmation

New files are checked for SPDX headers in review. Every import into `third_party/` updates `third_party/README.md` and `THIRD_PARTY_NOTICES.md` in the same pull request.

## Pros and Cons of the Options

### Apache License 2.0

- Good, because of the explicit patent licence and termination clause.
- Good, because of the NOTICE mechanism for attribution.
- Neutral, because it is longer than MIT or BSD.

### MIT licence

- Good, because it is short and permissive.
- Bad, because it contains no explicit patent licence.

### BSD 3-Clause licence

- Good, because it is permissive and adds a no-endorsement clause.
- Bad, because it contains no explicit patent licence.

### GNU GPL v3

- Good, because derivatives stay open.
- Bad, because the copyleft obligations discourage reuse of the firmware components.

### No licence

- Bad, because public code without a licence cannot be reused legally.

## More Information

- [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)
- [ADR 0009: Vendor nanopb and qrcodegen](0009-vendor-nanopb-and-qrcodegen.md)
