---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# Vendor nanopb and qrcodegen

## Context and Problem Statement

The CGW encodes and decodes the APP protocol in C with nanopb. Release 0.4.9.2 of nanopb is published on GitHub, while the Python package index still serves 0.4.9.1, so the generator cannot be installed at the required version from the package index. The CGW also displays the pairing QR code on its console. The ESP-IDF registry QR component prints its payload to the log, which would expose the pairing key and the Wi-Fi passphrase in console captures. How are these libraries obtained, pinned and updated?

## Decision Drivers

- Reproducible builds without network access to package registries.
- Exact, reviewable versions with verified checksums.
- No secret material in logs.
- Static memory only in the CGW core.
- Licence notices preserved.

## Considered Options

- Vendor pinned release sources in `third_party/` with recorded SHA-256
- Git submodules
- Package managers (Python package index for nanopb, ESP-IDF component registry for the QR component)
- Download at build time (for example CMake FetchContent)

## Decision Outcome

Chosen option: "Vendor pinned release sources in `third_party/` with recorded SHA-256", because it is the only option that provides the required nanopb release, works offline, and lets the QR library be chosen for its static-buffer API without logging.

- `third_party/nanopb-0.4.9.2/`: runtime sources and generator from the GitHub release archive.
- `third_party/qrcodegen-1.8.0/`: the C implementation of the Nayuki QR Code generator v1.8.0. The CGW prints the QR code itself, between redaction markers.
- `third_party/README.md` records origin URL, version, licence and SHA-256 of every vendored archive. Upstream notices are kept; `THIRD_PARTY_NOTICES.md` lists the components.
- Vendored code is not modified. A required patch needs an ADR and is kept as a separate, documented patch.
- Vendored code is excluded from formatting, linting and MISRA analysis.
- The same rules apply to the vendored CMSIS-Core 5.9.0 (`CMSIS/Core/Include` only) and the STM32F1 CMSIS device files v4.3.5.

### Consequences

- Good, because builds are reproducible and independent of registry availability.
- Good, because version changes are visible as reviewed diffs.
- Bad, because updates are manual: Dependabot does not track vendored code. Each update is a pull request that references this ADR.
- Bad, because the repository grows by the size of the vendored sources.

### Confirmation

The `codegen` job uses the vendored nanopb generator; a drift between the vendored generator and committed generated code fails the job. Changes under `third_party/` are reviewed against `third_party/README.md`.

## Pros and Cons of the Options

### Vendor pinned release sources

- Good, because of offline, reproducible builds and exact versions.
- Neutral, because updates are infrequent and deliberate.

### Git submodules

- Good, because the upstream history stays linked.
- Bad, because clones need an extra step and a submodule can point to an unreleased commit.

### Package managers

- Good, because updates can be automated.
- Bad, because the package index lacks nanopb 0.4.9.2, and the registry QR component logs the payload.

### Download at build time

- Good, because the repository stays small.
- Bad, because builds depend on network access and on the continued availability of upstream archives.

## More Information

- [nanopb releases](https://github.com/nanopb/nanopb/releases)
- [QR Code generator library](https://www.nayuki.io/page/qr-code-generator-library)
- [ADR 0005: Apache-2.0 licence](0005-apache-2-0-licence.md)
