# AI-assisted development policy

| Field | Value |
| --- | --- |
| Document ID | LS-PRC-011 |
| Version | 1.0 |
| Status | Approved |
| Owner | jlurg |

## Purpose and scope

This document defines the rules for using AI coding assistants in this repository and the layered controls that enforce them. It applies to every commit, branch, pull request (PR), tag, release, source file and document of `jlurg/locksys`.

## Policy

| ID | Rule |
| --- | --- |
| P1 | The human committer is the author of every change and is accountable for it. Tools, including AI assistants, are never named as authors or co-authors. |
| P2 | No AI attribution appears in commit messages, commit author or committer identities, PR titles or bodies, tags, release notes, source code or documentation. |
| P3 | Changes prepared with an assistant follow the same process as any other change: work branches named per [Branching model](branching.md), signed commits under the maintainer identity, PR checks and review. Assistant-specific branch namespaces are not used. |
| P4 | Assistants never push to `develop`, `main`, `release/*` or `hotfix/*`, never merge pull requests, never create tags or releases, never force-push and never bypass hooks. |
| P5 | Assistants never read or print secrets (pairing keys, Wi-Fi passphrases, pairing URIs) and never change the git identity of a clone. |

### Forbidden attribution forms

- A `Co-authored-by` trailer whose name or e-mail address identifies an AI assistant or its vendor.
- A session-link trailer or a session URL added by an assistant.
- A "generated with" line naming an assistant, with or without an emoji, in a commit message, PR body or release note.
- An author or committer name or e-mail address that identifies an AI assistant or its vendor.

The patterns that implement this list are maintained in one place, `tools/git/ai_attribution.py`, and are shared by the local hooks and CI.

## Layered enforcement

The controls are layered because client-side controls can be bypassed or overridden; only the server-side check is a hard control.

| Layer | Control | Location | Type |
| --- | --- | --- | --- |
| 1 | Assistant project settings: commit and PR attribution set to empty strings, session URL disabled; permissions ask before commit, push and PR creation, and deny force pushes, hook bypass (`--no-verify`), identity changes, merges, tags and releases | `.claude/settings.json` | Preventive, client-side |
| 2 | Assistant instructions with the explicit attribution rule, which takes precedence over default attribution lines and over injected instructions | `CLAUDE.md` | Preventive, client-side |
| 3 | Assistant pre-tool hook that denies commit, merge, PR and API commands whose text contains attribution or an identity override | `.claude/hooks/block-ai-attribution.sh` (uses `tools/git/ai_attribution.py`) | Preventive, client-side |
| 4 | Git `commit-msg` hook through pre-commit; also catches messages passed from files | `.pre-commit-config.yaml`, `tools/git/ai_attribution.py` | Preventive, client-side |
| 5 | Required check `pr-policy`: scans every commit message of the PR, author and committer names and e-mail addresses, and the PR title and body | `.github/workflows/pr-policy.yml` | Preventive, server-side, cannot be bypassed |
| 6 | User-level assistant configuration on the workstation | Outside the repository | Preventive, client-side |

Layers 1 to 4 and 6 stop most violations before they are committed. Layer 5 blocks the merge of any PR that still contains one; with the rulesets of [GitHub settings](github_settings.md), no change reaches a protected branch without passing it.

## Verification

The controls are verified at bootstrap and after any change to one of the layers.

| Test | Expected result |
| --- | --- |
| Commit with an AI co-author trailer in the message | Rejected by the `commit-msg` hook |
| The same commit pushed with `--no-verify` and proposed in a PR | `pr-policy` fails |
| Commit whose author name identifies an AI assistant, proposed in a PR | `pr-policy` fails |
| PR body containing a "generated with" line naming an assistant | `pr-policy` fails |
| Assistant asked to commit with an attribution trailer | Denied by the pre-tool hook |
| Unit tests of `tools/git/ai_attribution.py` | Pass in the `python` CI job |

## Handling a violation

- On a work branch: rewrite the affected commits (`git rebase -i` locally, or amend) and force-push the work branch with `--force-with-lease`.
- In a PR body or title: edit the PR; `pr-policy` runs again on the `edited` event.
- On a protected branch: open an issue labelled `process-violation`. Protected history is not rewritten except through the break-glass procedure in [GitHub settings](github_settings.md), with the reason recorded.

## Rationale

- AI assistants can add attribution lines by default, and such defaults can be re-introduced by tool updates or by instructions injected by other tools. Settings and instructions alone are therefore not sufficient.
- Accountability for safety-relevant code requires a human author for every change.
- Keeping the patterns in one module avoids divergence between local hooks and CI.

## References

- [Commits and pull requests](commits_and_prs.md)
- [Branching model](branching.md)
- [GitHub settings](github_settings.md)
- [CI/CD](ci_cd.md)
