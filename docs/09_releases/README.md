# Release records

| Field | Value |
| --- | --- |
| Document ID | LS-REL-000 |
| Version | 1.0 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This directory holds one release record per published LockSys release. A release record states what the release contains, which interface versions it implements and where its verification evidence is, with a SHA-256 checksum for every evidence file. The process that produces releases is defined in [Release process](../08_process/release_process.md).

## Releases

| Version | Date | Type | Record |
| --- | --- | --- | --- |
| No release published yet | | | |

Types: `release` for a minor or major release from a `release/*` branch, `hotfix` for a patch release from a `hotfix/*` branch.

## Record rules

- File name `vX.Y.Z.md`, for example `v1.0.0.md`.
- The record is drafted in the release preparation PR on the release branch and completed after publication, when the SHA-256 values of the published assets are known.
- Release candidates do not get their own record; the record lists the candidates that were tested.
- Records are not changed after completion, except to add a link to a superseding release.

## Record template

```markdown
# LockSys vX.Y.Z

| Field | Value |
| --- | --- |
| Document ID | LS-REL-vX.Y.Z |
| Version | 1.0 |
| Status | Released |
| Owner | jlurg |

## Summary

Scope of the release, milestone, release type (release or hotfix).

## Compatibility

| System | CAN matrix | APP protocol | UART telemetry |
| --- | --- | --- | --- |
| X.Y.Z | M.m | M.m | N |

## Release candidates

| Tag | Commit | Result |
| --- | --- | --- |
| vX.Y.Z-rc.1 | <sha> | Superseded or released |

## Artefacts

| Asset | SHA-256 |
| --- | --- |
| locksys-dcu-vX.Y.Z.hex | <sha256> |

## Evidence

| Evidence | Asset | SHA-256 |
| --- | --- | --- |
| C-STAT report and SARIF | <asset> | <sha256> |
| MISRA Guideline Compliance Summary and deviations in force | <asset> | <sha256> |
| Unit test results and coverage | <asset> | <sha256> |
| HIL regression, soak and HIL-REAL reports | <asset> | <sha256> |
| Trace matrix | <asset> | <sha256> |
| Software bill of materials | <asset> | <sha256> |
| Build environment (`build-env.json`) | <asset> | <sha256> |

## Reviews

Safety review, security review and deviation review: date, findings, decisions.

## Known issues and limitations

Open defects accepted for this release, with issue links.
```

## References

- [Release process](../08_process/release_process.md)
- [Branching model](../08_process/branching.md)
- [ADR 0006: VERSION file, SemVer and interface versions](../adr/0006-version-file-semver-and-interface-versions.md)
