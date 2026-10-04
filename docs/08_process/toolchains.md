# Toolchains and version pins

| Field | Value |
| --- | --- |
| Document ID | LS-PRC-010 |
| Version | 1.0 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This document lists the toolchains and tools used to build, test and release LockSys, where each version is pinned, how the pins are verified and how they are changed.

## Single source of versions

`tools/versions.env` is the single source of toolchain versions. It is a `KEY=VALUE` file read by CI workflows and by repository scripts. If this document and `tools/versions.env` disagree, `tools/versions.env` is correct and this document is updated.

## Pinned toolchains

| Area | Tool | Key in `tools/versions.env` | Pin | Used by | Installed on |
| --- | --- | --- | --- | --- | --- |
| System | System version | `LS_SYSTEM_VERSION` | Value of `VERSION` (`0.1.0-dev`) | Builds, release checks | Not applicable |
| DCU | IAR Embedded Workbench for Arm with C-STAT | `IAR_EWARM_BASELINE` | 9.70, exact build recorded on the Lab Host; 10.10.x only if the licence allows | `iar-gate`, releases | Lab Host |
| DCU | IAR Visual State | `IAR_VISUAL_STATE_MIN` | 11.2.1 (minimum) | Model generation and verification | Lab Host |
| DCU | CMSIS-Core | `CMSIS_CORE` | 5.9.0 (`CMSIS/Core/Include` only) | DCU builds | Vendored in `third_party/` |
| DCU | CMSIS device support for STM32F1 | `CMSIS_DEVICE_F1` | v4.3.5 | DCU builds | Vendored in `third_party/` |
| DCU | Arm GNU Toolchain | `ARM_GCC` | 13.3.rel1 | GCC shadow build | Mac (STM32CubeCLT 1.19.0), CI |
| DCU | STM32CubeCLT (Mac bundle of Arm GCC) | `STM32CUBECLT`, `ARM_GCC_MAC_BIN` | 1.19.0; binaries in `/opt/ST/STM32CubeCLT_1.19.0/GNU-tools-for-STM32/bin` | GCC shadow build on the Mac | Mac |
| CGW | ESP-IDF | `ESP_IDF_VERSION`, `ESP_IDF_IMAGE` | v5.5.5; container `espressif/idf:v5.5.5` pinned by the digest in `ESP_IDF_IMAGE` | CGW builds | Container on Mac and CI |
| CGW | Nayuki QR Code generator (C) | `QRCODEGEN` | v1.8.0 | CGW pairing | Vendored in `third_party/` |
| Shared | nanopb | `NANOPB`, `NANOPB_SRC_URL`, `NANOPB_SRC_SHA256` | 0.4.9.2 (GitHub release, SHA-256 recorded) | Protocol code generation | Vendored in `third_party/` |
| Shared | protoc | `PROTOC`, `PROTOC_SHA256_<platform>` | 33.6, compatible with Python protobuf 6.33.x; downloaded with SHA-256 check by `tools/codegen/fetch_protoc.py` | Protocol code generation | Download cache |
| Tests | Ceedling | `CEEDLING`, `CEEDLING_IMAGE` | 1.1.9 (Ruby 3.0 or later), image `locksys/ceedling:1.1.9` built from `tools/docker/ceedling/Dockerfile` | C unit tests | Docker |
| Tests | gcovr | `GCOVR` | 8.6 | Coverage | Ceedling image |
| Python | Python | `PYTHON` | 3.13, managed by uv | Tools, HIL | Mac, Lab Host, CI |
| Python | uv | `UV` | 0.12.23 | Python environments and lock file | Mac, Lab Host, CI |
| Python | cantools | `CANTOOLS` | 44.1.0 | CAN code generation, HIL | uv workspace |
| Python | Python protobuf runtime | `PY_PROTOBUF` | 6.33.6 (6.33.x constrained by logic2-automation 1.0.11) | HIL, tools | uv workspace |
| HIL | python-can | `PYTHON_CAN` | 4.6.1 | HIL | uv workspace |
| HIL | PyVISA and pyvisa-py | `PYVISA`, `PYVISA_PY` | 1.16.2 and 0.8.1 | Instrument control | uv workspace |
| HIL | logic2-automation | `LOGIC2_AUTOMATION` | 1.0.11 | Logic analyser automation | uv workspace |
| HIL | udsoncan and can-isotp | `UDSONCAN`, `CAN_ISOTP` | 1.26.1 and 2.0.7 | UDS tests | uv workspace |
| HIL | esptool | `ESPTOOL_HIL` | 5.4.0 (HIL flashing; ESP-IDF builds use the esptool bundled with ESP-IDF) | CGW flashing on the bench | uv workspace |
| APP | Flutter (Dart) | `FLUTTER`, `DART`, `FLUTTER_SHA256_<platform>` | 3.47.6 (Dart 3.13.5), through FVM; CI installs the release archive with a SHA-256 check | APP | Mac (FVM), CI |
| APP | Dart protobuf and protoc_plugin | `DART_PROTOBUF`, `DART_PROTOC_PLUGIN` | 6.1.0 and 25.1.0 | Dart protocol code generation | pub |
| CI | actionlint | `ACTIONLINT` | 1.7.12 | Workflow lint | pre-commit, CI |
| CI | ShellCheck | `SHELLCHECK` | 0.11.0 | Shell lint | pre-commit, CI |
| CI | pre-commit | `PRE_COMMIT` | 4.6.2 | Local and CI hooks | uv workspace |
| CI | clang-format | `CLANG_FORMAT` | 23.1.2 (hook revision in `.pre-commit-config.yaml`) | C formatting | pre-commit, CI |
| CI | zizmor | `ZIZMOR` | 1.30.1 | GitHub Actions security lint | pre-commit, CI |
| Release | git-cliff | `GIT_CLIFF` | 2.14.2 | Changelog and release notes | CI |

## Versions pinned in other files

| Item | Location |
| --- | --- |
| Python packages (ruff, mypy, pytest, lizard and others) | `uv.lock` of the root workspace |
| pre-commit hook revisions (clang-format, markdownlint-cli2, typos and others) | `.pre-commit-config.yaml` |
| Flutter version for FVM | `app/.fvmrc` |
| Dart and Flutter packages | `app/pubspec.lock` |
| ESP-IDF managed components | Dependency lock file in `firmware/cgw/` |
| GitHub Actions | Full commit SHAs in `.github/workflows/` |
| Ceedling base image | Digest in `tools/docker/ceedling/Dockerfile` |
| Vendored sources | Origin, version and SHA-256 in `third_party/README.md` |

## Installation

### MacBook workstation

```bash
brew install uv actionlint shellcheck
brew tap leoafarias/fvm && brew install fvm
uv python install 3.13
uv sync --locked
uv run pre-commit install --install-hooks
cd app && fvm install
```

`pre-commit install` installs both the `pre-commit` and the `commit-msg` hooks, as configured in `.pre-commit-config.yaml`.

- Docker Desktop provides the ESP-IDF and Ceedling containers.
- STM32CubeCLT 1.19.0 provides Arm GCC 13.3.rel1 (`/opt/ST/STM32CubeCLT_1.19.0/GNU-tools-for-STM32/bin`).
- JDK 17 and the Android SDK are needed for Android builds; Xcode for iOS builds.

### Lab Host

The Lab Host software and its versions are listed in [Lab Host](lab_host.md).

### CI

- Python through the pinned uv setup action and `uv sync --locked`.
- Containers by digest; Flutter from the release archive with SHA-256 check; Arm GCC from the Arm GNU Toolchain release named in `tools/versions.env`.

## Verification of pins

| Pin | Verified by |
| --- | --- |
| IAR compiler build | The C-STAT gate in `iar-gate` compares the `iccarm --version` output with `tools/versions.env` |
| Arm GCC | `dcu-gcc` compares `arm-none-eabi-gcc --version` with `tools/versions.env` |
| Python dependencies | `uv sync --locked` fails on an outdated lock file |
| Flutter | The `app` job compares the installed version with `app/.fvmrc` and `tools/versions.env` |
| Generators | `uv run tools/codegen/regen.py --check` regenerates with the pinned generators and fails on drift |
| Visual State | Generated engines record the generator version in `firmware/dcu/gen_vs/VS_MANIFEST.json`; the manifest check compares it with the minimum |

## Update policy

- Toolchain updates are batched once per milestone, never in the middle of a milestone.
- Changing IAR EWARM, IAR Visual State, ESP-IDF, CMSIS, nanopb or qrcodegen requires an ADR and a pin-change PR (`build(deps): ...`).
- Dependabot proposes updates for GitHub Actions, uv, pub, pre-commit and the Ceedling Dockerfile, weekly and grouped, with a 7-day cooldown.
- Container digests referenced in workflows are reviewed monthly and recorded in `tools/versions.env`.
- After an IAR update the day-1 checks D1, D2, D7, D8, D9 and D11 in [Lab Host](lab_host.md) and the C-STAT seeded-defect check in the [Guideline Enforcement Plan](misra/gep.md) are repeated.

## Rationale

- One machine-readable pin file lets CI and scripts verify the installed toolchains and keeps documentation from becoming a second source of truth.
- Batching toolchain changes per milestone separates tool-induced changes from functional changes.

## References

- [Lab Host](lab_host.md)
- [CI/CD](ci_cd.md)
- [ADR 0007: Pin ESP-IDF v5.5.5](../adr/0007-pin-esp-idf-v5-5-5.md)
- [ADR 0009: Vendor nanopb and qrcodegen](../adr/0009-vendor-nanopb-and-qrcodegen.md)
- [ADR 0010: Pin actions by commit SHA](../adr/0010-pin-actions-by-commit-sha.md)
