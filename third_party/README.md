# Vendored third-party code

| Field | Value |
|---|---|
| Owner | jlurg |
| Scope | Everything under `third_party/` |

## Policy

- Each component lives in `third_party/<name>-<version>/` and keeps its upstream licence file and
  source notices. Files are copied unmodified from the upstream release archive listed below; only
  the subset named in the table is kept.
- Vendored code is never edited in place. An update is a new directory imported from a new
  release, a new row in this table, matching pins in `tools/versions.env` and an entry in
  `THIRD_PARTY_NOTICES.md`.
- The SHA-256 is that of the upstream archive the files were taken from; re-download the archive
  and compare before accepting a change to a vendored file.
- Vendored code is excluded from formatting, linting, coverage and C-STAT scopes of LockSys code.

## Components

| Component | Version | Licence | Kept subset | Used by | Upstream archive | SHA-256 of the archive |
|---|---|---|---|---|---|---|
| CMSIS-Core (Cortex-M) | 5.9.0 | Apache-2.0 | `CMSIS/Core/Include/` | DCU | <https://github.com/ARM-software/CMSIS_5/archive/refs/tags/5.9.0.tar.gz> | `9ce693dd15c41f02506adda79323c077912b3211303ca0d9b28b8ebe735a7031` |
| STM32F1 CMSIS device headers | v4.3.5 | Apache-2.0 | `Include/`, `License.md` | DCU | <https://github.com/STMicroelectronics/cmsis-device-f1/archive/refs/tags/v4.3.5.tar.gz> | `2994ffe58af1f819928f11840359565e0293e91dfb840661da77c2e02de24ca3` |
| nanopb | 0.4.9.2 | Zlib | `pb.h`, `pb_common.c/.h`, `pb_encode.c/.h`, `pb_decode.c/.h`, `generator/`, `LICENSE.txt` | CGW (`cgw_proto`), `tools/codegen` | <https://github.com/nanopb/nanopb/releases/download/nanopb-0.4.9.2/nanopb-0.4.9.2.tar.gz> | `98b8cadce538f37230ca0d5d8796894e3067d58dd2fb2618e6712c7362bdd8bb` |
| QR Code generator library (C) | 1.8.0 | MIT | `c/qrcodegen.c`, `c/qrcodegen.h`, `LICENSE` | CGW (pairing QR code) | <https://github.com/nayuki/QR-Code-generator/archive/refs/tags/v1.8.0.tar.gz> | `2ec0a4d33d6f521c942eeaf473d42d5fe139abcfa57d2beffe10c5cf7d34ae60` |

## Notes

- **nanopb.** The runtime and the generator come from the same release, so the generated code in
  `firmware/cgw/components/cgw_proto/gen/` always matches the runtime. `tools/codegen/regen.py`
  runs `generator/nanopb_generator.py` with the pinned protoc (`PROTOC` in `tools/versions.env`,
  fetched by `tools/codegen/fetch_protoc.py`); the upstream `generator/protoc` wrapper
  (grpcio-tools) is not used.
- **qrcodegen.** Upstream ships the MIT licence only inside the source file headers; `LICENSE`
  holds that text without the comment markers.
- **CMSIS.** Both CMSIS components are header-only; the DCU firmware build adds their include
  directories.

## Verification

```sh
curl -sSL -o archive.tar.gz <upstream archive>
shasum -a 256 archive.tar.gz        # must equal the table value
tar -xzf archive.tar.gz
diff -r <extracted subset> third_party/<name>-<version>/<subset>
```
