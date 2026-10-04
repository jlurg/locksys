---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# Pin ESP-IDF v5.5.5

## Context and Problem Statement

The CGW firmware runs on an ESP32-S3 with ESP-IDF. It uses a WPA3-SAE SoftAP, the HTTP server with WebSocket support and the TWAI (CAN) driver. The workstation has ESP-IDF v5.5.1 installed natively. Relevant facts, checked on 2026-10-03:

- ESP-IDF v5.5.5 (released 2026-07-17) is the latest bugfix release of the v5.5 line. The v6.0 and v6.1 lines also exist.
- Espressif advisory AR2026-003 (2026-04-29) is a functional and interoperability advisory on WPA3-SAE H2E configuration, covering the SoftAP among others. v5.5.1 is affected by two of its issues: an unset H2E identifier in NVS treated as valid (fixed from v5.5.2) and a default `sae_pwe_h2e` value that rejects H2E-only peers (fixed from v5.5.3).
- v5.5.5 contains WebSocket server fixes and defines the TWAI advanced timing structure with exactly the fields the CGW design uses.

Which ESP-IDF version is used, and how is it pinned?

## Decision Drivers

- No known defects in the features the CGW uses (SoftAP security, WebSocket server, TWAI).
- Reproducible builds on the workstation and in CI.
- Minimal workaround code.
- No API migration during the MVP.

## Considered Options

- v5.5.1 with workarounds for the known defects
- v5.5.5, pinned by container image digest
- Any version from v5.5.3 onwards (floating)
- v6.1

## Decision Outcome

Chosen option: "v5.5.5, pinned by container image digest", because it fixes the advisory issues and the WebSocket defects at the same API level as v5.5.1, and a digest-pinned container makes every build reproducible.

- Builds use the container image `espressif/idf:v5.5.5`, referenced by digest; the digest is recorded in `tools/versions.env`. The canonical build command is `docker run --rm -v "$PWD":/project -w /project/firmware/cgw espressif/idf:v5.5.5 idf.py build`.
- A native installation on the workstation, if used, must be v5.5.5.
- The SoftAP configuration sets `sae_pwe_h2e = WPA3_SAE_PWE_BOTH` explicitly, independent of defaults.
- A move to v6.x is LATER and requires a new ADR.

### Consequences

- Good, because the advisory issues and WebSocket defects are fixed without project workarounds.
- Good, because CI and workstation builds use the same image.
- Bad, because local builds depend on Docker or an updated native installation.
- Bad, because the image digest has no automated update; it is reviewed monthly.

### Confirmation

The `cgw-build` job uses the digest from `tools/versions.env`; the build log states the ESP-IDF version. A change of the pin requires a pull request that references this ADR or a successor.

## Pros and Cons of the Options

### v5.5.1 with workarounds

- Good, because it matches the existing native installation.
- Bad, because it carries known advisory issues and defects that need workaround code.

### v5.5.5 pinned by digest

- Good, because it is the latest bugfix release of the same API line.
- Good, because digest pinning makes builds reproducible.

### Floating from v5.5.3

- Good, because fixes arrive automatically.
- Bad, because builds are not reproducible.

### v6.1

- Good, because it is the newest line.
- Bad, because it requires API migration without a functional need in the MVP.

## More Information

- [Espressif advisory AR2026-003](https://documentation.espressif.com/AR2026-003_OTA_Bug_Advisory_for_WPA3-SAE_H2E_Configuration_Issues_in_ESP-IDF_EN.html)
- [ESP-IDF v5.5.5 release](https://github.com/espressif/esp-idf/releases/tag/v5.5.5)
- [Toolchains](../08_process/toolchains.md)
- [CGW software architecture](../04_software/cgw/architecture.md)
