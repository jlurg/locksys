---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# VERSION file, SemVer and interface versions

## Context and Problem Statement

A LockSys release contains DCU, CGW and APP binaries plus documentation and evidence. The interfaces between the nodes (CAN matrix, APP protocol, UART telemetry) evolve at different rates than the system. At run time each node must detect an incompatible peer, and every binary must identify the exact source it was built from. How are the system version, build identification and interface versions defined?

## Decision Drivers

- One unambiguous system version for source, tag and binaries.
- Release tags verifiable against the source.
- Run-time detection of incompatible interface versions.
- Build identification: version, commit, dirty flag and build type.
- Simple enough to keep consistent by hand and by CI checks.

## Considered Options

- A `VERSION` file with Semantic Versioning plus independent interface versions
- Versions derived from `git describe` at build time
- Independent versions per component

## Decision Outcome

Chosen option: "A `VERSION` file with Semantic Versioning plus independent interface versions", because a committed file can be checked against tags and embedded deterministically in all builds, and independent interface versions let nodes check compatibility at run time.

- `VERSION` is the single source of the system version (Semantic Versioning 2.0.0). `develop` carries `X.Y.0-dev`; a release branch sets `X.Y.Z`; tags are `vX.Y.Z-rc.N` on release heads and `vX.Y.Z` on `main`, and must equal `VERSION` after removing `-rc.N`.
- Build types: `DEV` for builds that are not made from a release tag, `RC` for release candidate tags, `RELEASE` for final tags.
- Every build embeds the version, the short commit hash, a dirty flag and the build type. The DCU header `ls_build_info_gen.h` is generated at build time by `firmware/dcu/scripts/iar/New-BuildInfo.ps1` in the IAR pre-build step (CMake shadow build and Ceedling generation is planned for M1); the CGW derives `PROJECT_VER` from `VERSION`; the APP version in `pubspec.yaml` follows `VERSION`.
- Interfaces carry `major.minor` versions in their own artefacts: CAN matrix (DBC version and `ComMatrixVersion`, transmitted in the NodeSts and Version frames), APP protocol (protobuf package, WebSocket subprotocol and session hello), UART telemetry (`$LSVER`). A major change is incompatible and is marked with `!` in the commit; a minor change is additive.
- A node that receives a different interface major version rejects commands.
- Release notes contain a compatibility table: system version and the interface versions it contains.

### Consequences

- Good, because tag, source and binaries can be checked against each other automatically.
- Good, because incompatible nodes fail safe at run time instead of misinterpreting data.
- Bad, because version bumps are explicit pull requests.
- Bad, because `VERSION` and `pubspec.yaml` can drift; CI checks them.

### Confirmation

- `release.yml` rejects a tag that does not match `VERSION`.
- CI checks that the APP version matches `VERSION`.
- Interface changes are labelled `interface-change`; review checks the version change and the `!` marker.

## Pros and Cons of the Options

### `VERSION` file, Semantic Versioning and independent interface versions

- Good, because the version is reviewable and deterministic.
- Good, because interface compatibility is explicit.
- Neutral, because bumping requires a small pull request.

### Versions from `git describe`

- Good, because no file needs updating.
- Bad, because builds from archives or shallow clones lose the version, and the result depends on tag availability at build time.

### Independent versions per component

- Good, because components can be released separately.
- Bad, because a system release would need a compatibility matrix between component versions, which the single system version avoids.

## More Information

- [Release process](../08_process/release_process.md)
- [System architecture and interface contract](../02_system/LS-SAIC.md)
- [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)
