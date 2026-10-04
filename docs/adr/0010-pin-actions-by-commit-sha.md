---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# Pin actions by commit SHA

## Context and Problem Statement

Workflows use GitHub-owned and third-party actions. A tag such as `v4` can be moved by the action's owner, so a workflow that references a tag can execute different code tomorrow. Some workflows produce release artefacts and some lead to jobs on the Lab Host. GitHub offers a repository setting that requires every action, including actions nested inside composite actions, to be pinned to a full-length commit SHA; reusable workflows may still be referenced by tag. How are actions referenced?

## Decision Drivers

- Immutable references for all executed action code.
- Updates reviewed as explicit diffs.
- Enforcement by the platform, not only by convention.
- Readable workflows that still show the version in use.

## Considered Options

- Major version tags (for example `@v4`)
- Exact version tags (for example `@v4.2.1`)
- Full commit SHAs with a version comment, enforced by the repository setting and an allowlist
- Copies of the actions inside the repository

## Decision Outcome

Chosen option: "Full commit SHAs with a version comment, enforced by the repository setting and an allowlist", because only a full SHA is immutable, and the repository setting makes the rule binding for nested actions as well.

- Every `uses:` names a full 40-character commit SHA followed by a comment with the release version: `uses: actions/checkout@<sha> # vX.Y.Z`.
- SHAs are resolved from release tags with `gh api repos/<owner>/<repo>/git/ref/tags/<tag>`; annotated tags are dereferenced to the commit they point to.
- Repository settings: SHA pinning required; allowed actions limited to GitHub-owned actions and an explicit list.
- Composite actions whose nested actions are referenced by tag cannot be used, because the setting rejects them. Flutter is installed from its release archive with a SHA-256 check instead of a setup action.
- Container images are referenced by digest; downloaded archives are checked against SHA-256 values in `tools/versions.env`.
- Dependabot updates the SHAs and version comments weekly with a 7-day cooldown. Each update is reviewed, including the nested actions of the new version.
- `workflow-lint` runs actionlint and zizmor.

### Consequences

- Good, because a moved or compromised tag cannot change the executed code.
- Good, because each update is a visible, reviewed change.
- Bad, because Dependabot produces more update pull requests.
- Bad, because some third-party actions cannot be used until their nested references are pinned.

### Confirmation

The repository setting rejects unpinned references at run time; zizmor reports unpinned uses during `workflow-lint`.

## Pros and Cons of the Options

### Major version tags

- Good, because updates arrive automatically.
- Bad, because the referenced code can change without review.

### Exact version tags

- Good, because the version is visible.
- Bad, because tags remain mutable.

### Full commit SHAs, enforced

- Good, because references are immutable and enforcement covers nested actions.
- Neutral, because a version comment is needed for readability.

### Copies of the actions in the repository

- Good, because the code is fully under review.
- Bad, because every update requires copying and reviewing upstream code by hand.

## More Information

- [GitHub changelog: SHA pinning policy for actions](https://github.blog/changelog/2025-08-15-github-actions-policy-now-supports-blocking-and-sha-pinning-actions/)
- [CI/CD](../08_process/ci_cd.md)
- [GitHub settings](../08_process/github_settings.md)
