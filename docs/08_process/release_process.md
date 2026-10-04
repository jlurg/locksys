# Release process

| Field | Value |
| --- | --- |
| Document ID | LS-PRC-006 |
| Version | 1.0 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This document defines how LockSys versions are numbered, how a release is prepared, validated, tagged and published, which artefacts and evidence a release contains, and how hotfix releases are handled. Branch handling during a release is defined in [Branching model](branching.md).

## Versions

### System version

- The `VERSION` file at the repository root is the single source of the system version ([ADR 0006](../adr/0006-version-file-semver-and-interface-versions.md)). It currently holds `0.1.0-dev`.
- Versions follow Semantic Versioning 2.0.0:
  - `develop` carries `X.Y.0-dev`, the next minor version.
  - A release branch `release/X.Y.Z` sets `VERSION` to `X.Y.Z` in its preparation PR.
  - Release candidates are tagged `vX.Y.Z-rc.N` on the release or hotfix head; final releases are tagged `vX.Y.Z` on `main`.
  - A tag must equal `VERSION` after removing the `-rc.N` suffix; `release.yml` rejects any other tag.
- The APP version in `app/pubspec.yaml` (`version: X.Y.Z+<build number>`) follows `VERSION`; CI checks that both agree.

### Build identification

Every DCU, CGW and APP build reports its software version, the short git commit hash, a dirty flag and its build type.

| Build type | Produced by |
| --- | --- |
| `DEV` | Any build that is not made from a release tag |
| `RC` | Builds made by `release.yml` from a `vX.Y.Z-rc.N` tag |
| `RELEASE` | Builds made by `release.yml` from a `vX.Y.Z` tag |

- DCU: the build information header `ls_build_info_gen.h` is generated at build time by `firmware/dcu/scripts/iar/New-BuildInfo.ps1` (IAR pre-build step) and is not committed. Generation in the CMake shadow build and in Ceedling is planned for M1.
- CGW: `PROJECT_VER` is derived from `VERSION`.
- Fault-injection code is compiled out of `RC` and `RELEASE` images; CI rejects a release image that contains `Fi_` (DCU) or `cgw_fi_` (CGW) symbols.

### Interface versions

Interface versions are independent of the system version and use `major.minor`:

| Interface | Version carried in | Contract |
| --- | --- | --- |
| CAN matrix | DBC version attribute and `ComMatrixVersion`; transmitted in the NodeSts and Version frames | [CAN matrix](../03_interfaces/can_matrix.md) |
| APP protocol | Protobuf package (`locksys.app.v1`), WebSocket subprotocol and session hello messages | [APP protocol](../03_interfaces/app_protocol.md) |
| UART telemetry | `$LSVER` sentence | [UART telemetry](../03_interfaces/uart_telemetry.md) |

- A major change is incompatible and requires `!` and a `BREAKING CHANGE:` footer in the commit. A minor change is additive.
- A node that receives a different interface major version rejects commands and reports the mismatch.
- Every release states the interface versions it contains in a compatibility table, for example: system 1.0.0 contains CAN matrix 1.0, APP protocol 1.0 and telemetry 1.

## Release entry criteria

A release branch is cut only when:

1. The milestone scope is closed: all `scope:MVP` issues of the milestone are done or moved explicitly.
2. The nightly HIL run on `develop` has been green on 3 consecutive nights.
3. The trace report shows no gap for the MVP requirements of the release.
4. No open bug with priority P0 or P1 affects the release scope.
5. The HIL gate exists. No release is cut before milestone M5.

## Release checklist

| Step | Action | Evidence |
| --- | --- | --- |
| 1 | Open a release issue in the milestone and confirm the entry criteria | Issue |
| 2 | Create `release/X.Y.Z` from `develop` and push it | Branch |
| 3 | Merge the preparation PR `build(release): prepare X.Y.Z`: `VERSION`, `app/pubspec.yaml`, `CHANGELOG.md` (`git cliff --unreleased --tag vX.Y.Z --prepend CHANGELOG.md`), compatibility table, release record draft `docs/09_releases/vX.Y.Z.md` | PR |
| 4 | Confirm on the release head: `iar-gate` green, regression HIL suite green, trace gate blocking and green | Workflow runs |
| 5 | Tag `vX.Y.Z-rc.1` (signed) on the release head; `release.yml` builds the pre-release artefacts | Pre-release |
| 6 | Run the 8 h HIL-SIM soak on the release candidate (`hil.yml`, manual dispatch) and the attended HIL-REAL run defined in the [HIL test catalogue](../07_verification/hil_test_catalog.md) | Soak and HIL-REAL reports |
| 7 | For each fix: merge it into the release branch, repeat step 4 and tag the next `rc.N` | Pre-release per candidate |
| 8 | Record the safety review (safety goals and mechanisms, open DTCs), the security review (TARA controls) and the MISRA Guideline Compliance Summary with the deviation review | Release record |
| 9 | Merge `release/X.Y.Z` into `main` with a merge commit; all required checks pass on the release head | Merge commit |
| 10 | Tag the merge commit and push the tag: `git tag -s vX.Y.Z <merge-commit> -m "LockSys X.Y.Z"` | Signed tag |
| 11 | Approve the deployment of the `release` environment; `release.yml` publishes the immutable release | Release |
| 12 | Verify the published release: `SHA256SUMS`, attestations, assets, notes | Release record |
| 13 | Back-merge `main` into `develop` and open the PR that sets `VERSION` on `develop` to `X.(Y+1).0-dev` | PRs |
| 14 | Complete the release record and the [release index](../09_releases/README.md); close the release issue and the milestone | Release record |

## Release workflow

`release.yml` runs on tags `vX.Y.Z` and `vX.Y.Z-rc.N`.

1. **Verify** (GitHub-hosted):
   - the tag is annotated and its SSH signature verifies against the maintainer's signing key;
   - the tag matches `VERSION` after removing `-rc.N`;
   - for final tags: the tag is on `main`, the tagged commit is a merge commit whose tree equals the tree of its second parent, and `iar-gate` and `hil-gate` succeeded on that second parent.
2. **Build** without caches: DCU (IAR build on the Lab Host, image after the ROM checksum has been inserted), CGW (container pinned by digest), APP (Android package), documentation archive.
3. **Acceptance**: HIL smoke suite on the exact release binaries, including a `$LSVER` check of build type and commit hash.
4. **Publish** (environment `release`, required reviewer `jlurg`, tags only):
   - collect artefacts and evidence, write `build-env.json` (toolchain versions, container digests, runner versions) and `SHA256SUMS`;
   - generate the release notes with `git-cliff` from `cliff.toml`, including the compatibility table;
   - create build provenance attestations for artefacts built on GitHub-hosted runners;
   - create the release as a draft, upload all assets, then publish it. Publishing makes it immutable.

Self-hosted jobs in this workflow hold read-only tokens; uploads and publication run in GitHub-hosted jobs.

## Artefacts

| Artefact | Content |
| --- | --- |
| `locksys-dcu-vX.Y.Z.{elf,hex,bin,map}` | DCU image; `bin` and `hex` contain the inserted ROM checksum |
| `locksys-cgw-vX.Y.Z.{bin,elf,map}`, `flash_args` | CGW merged image, symbols, map, flashing arguments |
| `locksys-app-vX.Y.Z.apk` | Android package (MVP: debug-signed bench build) |
| `docs-vX.Y.Z.zip` | Documentation at the release commit |
| `SHA256SUMS`, `build-env.json` | Checksums of all assets; build environment |
| Release notes | Changes since the previous release and the compatibility table |

## Evidence pack

Each release carries its verification evidence as release assets, because workflow runs, artefacts and check results are deleted after 90 days in a public repository.

- C-STAT HTML report and SARIF output, Guideline Compliance Summary, deviation list in force
- Unit test results (JUnit), coverage reports, complexity report
- HIL regression report, soak report and HIL-REAL report, with CAN logs, telemetry logs and logic analyser exports
- Trace matrix
- CGW software bill of materials
- `build-env.json` and `SHA256SUMS`

The release record `docs/09_releases/vX.Y.Z.md` indexes every evidence file with its SHA-256.

## Release candidates and final binaries

The final tag is built again from the merge commit on `main`. Its source tree is identical to the last release candidate, but the build type and commit hash differ, so the binaries are not byte-identical. The acceptance smoke run on the final binaries, with the `$LSVER` check, covers this difference in the MVP. Proving that code and constant sections are byte-identical to the last release candidate is LATER.

## Hotfix releases

- Hotfixes follow the hotfix procedure in [Branching model](branching.md) and the release checklist from step 3, with `hotfix/X.Y.Z` instead of `release/X.Y.Z`.
- Only the latest minor release line receives hotfixes. There are no backports.
- Security fixes follow `SECURITY.md`.

## Immutability

- Release immutability is enabled in the repository settings before the first release; it does not apply to earlier releases.
- A published release cannot change its tag or assets; its title and notes remain editable.
- The tag name of a published release cannot be reused, even after deletion. A defective release is superseded by the next patch version.

## Rationale

- One `VERSION` file and tag checks in `release.yml` remove ambiguity between source, tag and binaries.
- Independent interface versions let the nodes detect incompatible peers at run time while the system version tracks releases.
- Release evidence in immutable releases survives the 90-day retention of workflow data and cannot be altered after publication.

## References

- [Branching model](branching.md)
- [CI/CD](ci_cd.md)
- [GitHub settings](github_settings.md)
- [Release records](../09_releases/README.md)
- [Verification strategy](../07_verification/verification_strategy.md)
- [ADR 0006: VERSION file, SemVer and interface versions](../adr/0006-version-file-semver-and-interface-versions.md)
- [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)
- [GitHub: immutable releases](https://docs.github.com/en/code-security/concepts/supply-chain-security/immutable-releases)
