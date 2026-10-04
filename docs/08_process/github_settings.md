# GitHub settings

| Field | Value |
| --- | --- |
| Document ID | LS-PRC-008 |
| Version | 1.1 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This document defines the configuration of the GitHub repository `jlurg/locksys`: repository settings, Actions settings, security features, rulesets, labels, milestones, the bootstrap order that avoids locking the maintainer out, and the break-glass procedure. The executable form of these settings is `tools/github/apply-repo-settings.sh` with the ruleset, label and milestone definitions in `tools/github/`. A difference between this document and those files is a defect.

## Repository

| Setting | Value | Reason |
| --- | --- | --- |
| Owner and name | `jlurg/locksys` | Personal account |
| Visibility | Public | Rulesets, environment protection rules and code scanning are available to public repositories at no cost |
| Licence | Apache-2.0 | [ADR 0005](../adr/0005-apache-2-0-licence.md) |
| Default branch | `develop` | [ADR 0003](../adr/0003-gitflow-lite-with-develop-as-default-branch.md) |
| Issues, Projects, Discussions | Enabled | Discussions for questions and ideas |
| Wiki | Disabled | Documentation lives in `docs/` |
| PR creation | `collaborators_only` (`pull_request_creation_policy`) | Forks cannot open PRs during the MVP |
| Merge methods | Squash: on; merge commit: on; rebase: off | Rulesets restrict each target branch further; rebase merges cannot be signed by GitHub |
| Squash commit message | Title `PR_TITLE`, message `PR_BODY` | The reviewed PR text becomes the commit text |
| Merge commit message | Title `PR_TITLE`, message `PR_BODY` | Release log stays Conventional |
| Auto-merge | On | Merge when checks pass |
| Delete head branches after merge | On | |
| Suggest updating PR branches | On | |
| Web commit sign-off | Off | No DCO in the MVP |

## Actions

| Setting | Value |
| --- | --- |
| Actions | Enabled; allowed actions: GitHub-owned actions plus an explicit allowlist |
| Require actions pinned to a full-length commit SHA | On (`sha_pinning_required`) |
| Default `GITHUB_TOKEN` permissions | Read |
| Allow Actions to create and approve pull requests | Off |
| Approval for fork PR workflows | Required for all external contributors (`all_external_contributors`) |
| Artifact and log retention | 90 days |

Repository variables:

| Variable | Value | Use |
| --- | --- | --- |
| `TRUSTED_ACTORS` | `["jlurg"]` | Optional workflow-level guard that avoids queueing self-hosted jobs the Lab Host hook would reject. The host keeps the authoritative list. |
| `HIL_BENCH_STATE` | `online` or `maintenance` | From milestone M5: marks the bench as unavailable without changing workflows |

There are no repository secrets in the MVP.

## Environments

| Environment | Protection | Use |
| --- | --- | --- |
| `release` | Required reviewer `jlurg`; deployments from tags only | Publication step of `release.yml` |

## Security features

| Feature | Setting |
| --- | --- |
| Dependabot alerts and security updates | On |
| Dependabot version updates | `.github/dependabot.yml`: GitHub Actions, uv, pub, pre-commit and the Ceedling Dockerfile; weekly; grouped; cooldown 7 days; Conventional commit prefixes |
| Secret scanning and push protection | On; non-provider patterns on if available for the account |
| Code scanning | CodeQL advanced setup through `codeql.yml` (Python, GitHub Actions); C-STAT SARIF uploaded by the IAR workflow |
| Private vulnerability reporting | On; process in `SECURITY.md` |
| Release immutability | On, before the first release |

## Rulesets

All rulesets have an empty bypass list. Required checks name the GitHub Actions app as their source (`integration_id` 15368).

| Ruleset | Target | Rules |
| --- | --- | --- |
| `main` | Branch `main` | Restrict deletion; block force pushes; require signed commits; require a PR with 0 approvals, merge method merge commit only, conversation resolution; required checks in strict mode: `pr-policy`, `ci-gate`, `iar-gate`, and `hil-gate` from milestone M5 |
| `develop` | Branch `develop` | Restrict deletion; block force pushes; require signed commits; require a PR with 0 approvals, merge methods squash and merge commit; required checks, not strict: `pr-policy`, `ci-gate`, `iar-gate` |
| `release-hotfix` | Branches `release/*`, `hotfix/*` | Block force pushes; require signed commits; require a PR with 0 approvals, merge methods squash and merge commit; required checks in strict mode, not enforced on branch creation: `pr-policy`, `ci-gate`, `iar-gate`. No deletion rule, so merged branches can be deleted automatically. |
| `tags` | Tags `v*` | Restrict updates; restrict deletion; block force pushes |

Notes:

- **Approvals.** GitHub does not let an author approve their own PR, so the required approval count is 0 while there is one maintainer. Requiring a PR still blocks direct pushes. When collaborators join: 1 approval, code owner review, approval of the most recent push, and dismissal of stale approvals.
- **Strict mode.** Strict mode on `main` guarantees that the merged tree equals the tested release head. `develop` uses non-strict mode: back-merge PRs would otherwise need repeated updates, and the post-merge CI and nightly HIL on `develop` compensate.
- **Order of `iar-gate` and `hil-gate`.** The rulesets are first applied without `iar-gate` and `hil-gate`. `iar-gate` is added when the Lab Host runner reports, and `hil-gate` at milestone M5.
- **Not available or not used:**
  - Branch-name, commit-message and tag-name pattern rules (metadata restrictions) are limited to GitHub Enterprise plans. `pr-policy` enforces these rules.
  - Push rulesets are limited to private and internal repositories. Pre-commit hooks and CI checks replace them.
  - Required linear history conflicts with the merge commits on `main` and the back-merges.
  - Merge queue. If adopted later, the gate workflows need a `merge_group` trigger and the Lab Host hook allowlist must include it.
- Tag creation is not restricted by a ruleset in the MVP; `release.yml` verifies the tag signature against the maintainer's key.

## Labels

| Group | Labels |
| --- | --- |
| Type | `type:feature`, `type:bug`, `type:requirement`, `type:hil-failure`, `type:docs`, `type:ci`, `type:chore`, `type:security` |
| Area | `area:dcu`, `area:cgw`, `area:app`, `area:hil`, `area:can`, `area:proto`, `area:libs`, `area:docs`, `area:ci`, `area:tools` |
| Priority | `prio:P0`, `prio:P1`, `prio:P2`, `prio:P3` |
| Scope | `scope:MVP`, `scope:LATER` |
| Status | `status:blocked`, `status:needs-info`, `status:ready` |
| Size | `size/XS`, `size/S`, `size/M`, `size/L`, `size/XL` |
| Flags | `safety`, `security`, `interface-change`, `breaking`, `dependencies`, `process-violation`, `break-glass` |

`interface-change` is applied to PRs that change `interfaces/`.

## Milestones and issue forms

- Milestones M0 to M6 as defined in the [roadmap](../00_project/roadmap.md). Each milestone has an epic issue with sub-issues.
- Issue forms in `.github/ISSUE_TEMPLATE/`: `bug.yml`, `feature.yml`, `requirement_change.yml`, `hil_failure.yml`; blank issues are disabled in `config.yml`, which links private vulnerability reporting and Discussions.
- `.github/CODEOWNERS` assigns all paths to `@jlurg`.

## Bootstrap order

The order avoids locking the maintainer out and gives the rulesets checks that have already reported.

| Step | Action |
| --- | --- |
| 1 | Create the SSH signing key on the workstation and register it on GitHub as a signing key. Enable "Keep my email addresses private". |
| 2 | Initialise the local repository on `main` with the repository-local identity (`jlurg`, noreply address), SSH signing for commits and tags, and `core.autocrlf false`. Install the pre-commit hooks. |
| 3 | Create the signed initial commit on `main` with a Conventional message and no attribution trailers. The branch-protection hook of pre-commit is skipped for this one commit. |
| 4 | Create the public repository and push: `gh repo create jlurg/locksys --public --source . --remote origin --push`. |
| 5 | Create and push `develop`. |
| 6 | Apply the base settings, which set `develop` as default branch before any ruleset exists: `tools/github/apply-repo-settings.sh base`, then `tools/github/apply-repo-settings.sh labels`. |
| 7 | Create issue 1 "M0 bootstrap" so that the seed PR can link an issue. |
| 8 | Open the seed PR from `chore/1-seed-ci` into `develop` and let `pr-policy` and `ci-gate` report at least once. Trigger a Dependabot version-update check and confirm that Dependabot can open PRs under `collaborators_only`; if it cannot, set the PR creation policy to `all` and rely on fork approval and the Lab Host hook. |
| 9 | Apply the rulesets without `iar-gate` and `hil-gate`: `tools/github/apply-repo-settings.sh rulesets`. |
| 10 | Verify the protections (see below). |
| 11 | Merge the seed PR through the protected path. |
| 12 | After the Lab Host runner `iar` is online and `iar-gate` has reported on a pushed branch: add `iar-gate` to the rulesets with `INCLUDE_IAR_GATE=1 tools/github/apply-repo-settings.sh rulesets`. |
| 13 | At milestone M5, after the runner `hil` is online: add `hil-gate` to the `main` ruleset with `INCLUDE_IAR_GATE=1 INCLUDE_HIL_GATE=1 tools/github/apply-repo-settings.sh rulesets`. |

The script converges the rulesets to the definition files on every run. `INCLUDE_IAR_GATE` and `INCLUDE_HIL_GATE` default to `0`, so every later run of `rulesets` (or `all`) sets the variables for the gates already in force; omitting them removes those required checks. `--dry-run` (or `DRY_RUN=1`) prints the requests without sending them and is used before each change.

Verification of step 10:

- `gh ruleset check main` and `gh ruleset check develop` list the expected rules.
- A direct `git push origin develop` is rejected.
- A PR with an unsigned commit cannot be merged.
- A commit whose message carries an AI co-author trailer is rejected by the local hook, and, pushed with `--no-verify`, fails `pr-policy`.
- A commit whose author identity names an AI assistant fails `pr-policy`.
- Creating and deleting a test branch `release/0.0.1` is possible.
- `pre-commit run actionlint --all-files` passes.

## Break-glass procedure

Used only when a ruleset blocks a necessary action that cannot be performed through the normal path (for example a broken required check after a GitHub change).

1. Open an issue labelled `break-glass` that states the reason and the intended action.
2. Set the enforcement of the affected ruleset to `disabled`.
3. Perform the action.
4. Set the enforcement back to `active`.
5. Link the ruleset history (`GET /repos/jlurg/locksys/rulesets/{id}/history`) in the issue, record a short post-review and close the issue.

## Auditing

- `tools/github/apply-repo-settings.sh` is idempotent and is re-run after any change to its definition files.
- Drift is detected by comparing `gh api repos/jlurg/locksys/rulesets` and the repository settings with the definition files before each release.

## Rationale

- All protections are server-side and have no bypass actors, so the maintainer account cannot skip them by accident; the break-glass procedure leaves an auditable record when it must.
- Base settings come before rulesets so that the default branch and merge settings are correct before force pushes are blocked.
- Rulesets are applied after the first CI run because required checks reference checks the GitHub Actions app has already reported.

## References

- [Branching model](branching.md)
- [CI/CD](ci_cd.md)
- [Lab Host](lab_host.md)
- [AI policy](ai_policy.md)
- [GitHub: about rulesets](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/about-rulesets)
- [GitHub: REST API for rules](https://docs.github.com/en/rest/repos/rules)
- [GitHub: managing GitHub Actions settings for a repository](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository)
