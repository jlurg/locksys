---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# Lab Host self-hosted runner trust model

## Context and Problem Statement

The IAR toolchain (node-locked licence) and the HIL bench exist only on the Windows Lab Host, so IAR builds, C-STAT and HIL tests must run there as self-hosted GitHub Actions jobs. The repository is public. GitHub's guidance is that self-hosted runners should almost never be used with public repositories, because anyone can open a pull request whose workflow code would run on the runner. The runner cannot be ephemeral: the licence and the bench are bound to one physical machine. How can the Lab Host run CI jobs without executing untrusted code?

## Decision Drivers

- No code from outside the maintainer's control runs on the Lab Host.
- The IAR and HIL results still gate pull requests as required checks.
- Few moving parts, maintainable by one person.
- Physical safety of the bench (12 V supply, motor, actuator).
- Workflow logs and artefacts of a public repository are world-readable.

## Considered Options

- No self-hosted runners; IAR results attached manually to pull requests
- Self-hosted jobs triggered by `pull_request`, protected by labels or approvals
- Self-hosted jobs triggered only by trusted events, with a host-side trust gate
- A private mirror repository that owns the runners

## Decision Outcome

Chosen option: "Self-hosted jobs triggered only by trusted events, with a host-side trust gate", because GitHub attaches the check runs of a `push`-triggered workflow to the pushed commit, so the IAR result gates the pull request whose head is that commit, while no pull-request-supplied workflow definition ever reaches the Lab Host.

Controls:

1. Workflows with self-hosted jobs are triggered only by `push` to repository branches, `schedule` and `workflow_dispatch`; never by `pull_request`, `pull_request_target`, `workflow_run` or `issue_comment`. `tools/ci/check_self_hosted.py` enforces this in CI.
2. During the MVP only collaborators can open pull requests; workflows from forks require approval for all external contributors.
3. A job-started hook on the Lab Host fails closed unless the repository is `jlurg/locksys`, the event is `push`, `schedule` or `workflow_dispatch`, and the triggering actor is listed in the host's `trusted.txt`.
4. Both runner instances (`iar`, `hil`) run under a standard, non-administrator account; runners are registered to the repository only.
5. Self-hosted jobs hold `contents: read` tokens, check out with `persist-credentials: false` into a clean workspace; every write operation (code scanning upload, issues, releases) runs in a GitHub-hosted job.
6. A job-completed hook cleans the workspace; on the HIL runner it first switches the bench supply off.
7. No GitHub secrets or tokens are stored on the host. The only secret is the bench pairing key, kept in the operating system keyring. HIL logs are redacted and scanned before upload.
8. Inbound network access to the host is limited to SSH and remote desktop from the workstation.
9. Dependabot changes reach the host only when the maintainer re-runs the job after reviewing the diff.

### Consequences

- Good, because fork and pull request code cannot reach the Lab Host by any trigger.
- Good, because `iar-gate` and `hil-gate` remain required checks on the pull request head commit.
- Bad, because the Lab Host is a single point of failure: when it is offline, self-hosted jobs wait 24 hours, are cancelled, and the gates fail until they are re-run.
- Bad, because a persistent runner keeps residual risk: a compromised maintainer account could push code that runs on the host. Signed commits, two-factor authentication and the empty bypass lists reduce it; the residual risk is recorded in the TARA.

### Confirmation

- `workflow-lint` runs `tools/ci/check_self_hosted.py` on every change to `.github/workflows/`.
- At setup, the hook is tested with simulated environments for another repository, a `pull_request` event and an untrusted actor; each must be rejected.
- The Lab Host day-1 checks are recorded in [Lab Host](../08_process/lab_host.md).

## Pros and Cons of the Options

### No self-hosted runners

- Good, because no code runs on the Lab Host from CI.
- Bad, because IAR and C-STAT results become manual attestations that cannot gate merges reliably.

### Self-hosted jobs triggered by `pull_request`

- Good, because the workflow runs directly on the pull request.
- Bad, because the pull request supplies the workflow definition, so `if:` guards and labels are not a control; `pull_request_target` runs regardless of approval settings.

### Trusted events with a host-side trust gate

- Good, because the host decides for itself which jobs it runs, independent of workflow content.
- Neutral, because required checks must be re-run from the `push` run, not by a manual dispatch.

### Private mirror repository

- Good, because the runner is not attached to a public repository.
- Bad, because the results must be copied back as statuses, adding a synchronisation mechanism with its own credentials.

## More Information

- Deferred hardening (LATER): host check that the head commit has a GitHub-verified signature; separate hook scripts per runner; just-in-time runner registration; headless logic analyser automation.
- [Lab Host](../08_process/lab_host.md), [CI/CD](../08_process/ci_cd.md), [TARA-lite](../06_security/tara_lite.md)
- [GitHub: security hardening for self-hosted runners](https://docs.github.com/en/actions/reference/security/secure-use)
- [GitHub: running scripts before or after a job](https://docs.github.com/en/actions/how-tos/manage-runners/self-hosted-runners/run-scripts)
