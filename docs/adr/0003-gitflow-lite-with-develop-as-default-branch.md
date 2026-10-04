---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# GitFlow-lite with develop as default branch

## Context and Problem Statement

`main` must hold only states that passed the full verification, including HIL, so that every commit on it is a usable release. Integration of ongoing work needs its own branch. Several GitHub behaviours depend on the default branch: closing keywords close issues only when a PR merges into it, Dependabot opens its PRs against it, and scheduled workflows run on it. Which branch model and which default branch are used?

## Decision Drivers

- `main` contains only verified releases.
- Integration branch receives nightly HIL runs and dependency updates.
- Issues close when their change is integrated.
- Every change goes through a PR with required checks; signed commits only.
- Low process overhead for a single maintainer.

## Considered Options

- Trunk-based development on `main` with release tags
- GitHub flow: `main` as default branch with short-lived feature branches
- Full GitFlow with long-lived feature branches and release branches merged back into `develop`
- GitFlow-lite with `develop` as default branch

## Decision Outcome

Chosen option: "GitFlow-lite with `develop` as default branch", because it keeps `main` release-only while the default-branch behaviours of GitHub (issue closing, Dependabot, schedules) apply to the integration branch.

The model is defined in the [branching model](../08_process/branching.md): squash merges for work branches into `develop`; `release/X.Y.Z` and `hotfix/X.Y.Z` merged into `main` with merge commits; back-merges through `sync/*` branches; release candidate tags on release heads and final tags on `main`.

### Consequences

- Good, because every first-parent commit of `main` is a tagged, verified release.
- Good, because the nightly HIL runs test the integration state.
- Good, because issues close automatically when their PR merges into `develop`.
- Bad, because every release and hotfix needs a back-merge into `develop`.
- Bad, because visitors see `develop` by default; the README points to the latest release.

### Confirmation

- `pr-policy` enforces branch names and allowed base and head combinations.
- The `main` ruleset allows merge commits only and requires strict status checks.
- `release.yml` verifies that a final tag is on `main` and that the tagged tree equals the release head.

## Pros and Cons of the Options

### Trunk-based development on `main` with release tags

- Good, because there is a single long-lived branch.
- Bad, because `main` would contain states that never passed the full HIL suite.

### GitHub flow with `main` as default branch

- Good, because it is simple and widely known.
- Bad, because there is no separate release-only branch.

### Full GitFlow

- Good, because the release line is separated from integration.
- Bad, because long-lived feature branches and extra merges add overhead without benefit for a single maintainer.

### GitFlow-lite with `develop` as default branch

- Good, because it separates the release line from integration with the least number of long-lived branches.
- Neutral, because back-merges are a routine step after each release.

## More Information

- [Branching model](../08_process/branching.md)
- [GitHub settings](../08_process/github_settings.md)
- [GitHub: linking a pull request to an issue](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/linking-a-pull-request-to-an-issue)
