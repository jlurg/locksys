# Lab Host runner tooling

Scripts that prepare the Windows Lab Host, register its two self-hosted GitHub Actions runners and gate every job on the host. Process, accounts and day-1 checks: [Lab Host and workstation workflow](../../docs/08_process/lab_host.md). Trust model: [ADR 0004](../../docs/adr/0004-lab-host-self-hosted-runner-trust-model.md).

## Contents

| File | Runs as | Purpose |
| --- | --- | --- |
| `setup-labhost.ps1` | Administrator, elevated | Directories and ACLs, runner account, hooks, OpenSSH key-only, remote desktop with NLA, firewall scope, Git, PowerShell 7, uv, power settings. Idempotent. |
| `register-runner.ps1` | `labrunner`, not elevated | Installs the pinned runner release (SHA-256 checked), writes the runner `.env`, registers `labhost-iar` or `labhost-hil`, installs Python through uv, starts the runner at logon. Idempotent. |
| `hooks/Assert-TrustedJob.ps1` | Runner, before each job | Job-started hook; refuses untrusted jobs (fail closed). |
| `hooks/Complete-Job.ps1` | Runner, after each job | Job-completed hook; bench supply off (runner `hil`), stray processes stopped, workspace cleaned. |
| `Restore-ConsoleSession.ps1` | Administrator, elevated | Returns the `labrunner` session to the console after a remote desktop session. |
| `PSScriptAnalyzerSettings.psd1`, `tests/` | CI (`workflow-lint`) and workstation | Static analysis and Pester tests of the scripts. |

## Host layout

| Path | Owner and access | Content |
| --- | --- | --- |
| `C:\labhost\` | Administrators; `labrunner` read and execute | Root |
| `C:\labhost\hooks\` | Administrators; `labrunner` read and execute | Installed copies of `hooks\*.ps1` |
| `C:\labhost\trusted.txt` | Administrators; `labrunner` read | Trusted GitHub logins, one per line, `#` comments |
| `C:\labhost\tools\uv\` | Administrators; `labrunner` read and execute | Pinned uv release on the machine `PATH` |
| `C:\labhost\runners\iar\`, `C:\labhost\runners\hil\` | `labrunner` modify | Runner installations, `.env`, work directories `_work` |
| `C:\labhost\evidence\` | `labrunner` modify | HIL evidence before upload |
| `C:\labhost\bench.lock` | Created by the maintainer | Manual bench reservation; while it exists the `hil` runner refuses jobs |

The runner account can read but not change the hooks, `trusted.txt` and uv, so a job cannot weaken the gate for later jobs. Runtime locks of the HIL framework do not use `bench.lock`; they live in a directory writable by `labrunner`.

## Setup sequence

1. As administrator, in a clone of the repository:

   ```powershell
   .\tools\labhost\setup-labhost.ps1 -AllowedRemoteAddress <workstation address> -AdminPublicKeyFile <public key file>
   ```

2. Complete the manual steps printed by the script (automatic logon of `labrunner`, BitLocker, BIOS power-on after AC loss, IAR installation and licence).
3. As `labrunner`, with a registration token from `gh api -X POST repos/jlurg/locksys/actions/runners/registration-token --jq .token`:

   ```powershell
   .\tools\labhost\register-runner.ps1 -Role iar -EwarmDir '<EWARM installation root>'
   .\tools\labhost\register-runner.ps1 -Role hil -PsuAddress <host>:5025   # from milestone M5
   ```

4. Log off and on again (or run the Startup entry) to start the runners; check that they are online in the repository settings.
5. Set the repository variables `TRUSTED_ACTORS` and, from M5, `HIL_BENCH_STATE` ([GitHub settings](../../docs/08_process/github_settings.md)).

Hook changes take effect after re-running `setup-labhost.ps1`, which copies `hooks\*.ps1` to `C:\labhost\hooks\`. A change of a runner `.env` takes effect after a runner restart.

## Runner environment (`.env`)

| Key | Runner | Value |
| --- | --- | --- |
| `ACTIONS_RUNNER_HOOK_JOB_STARTED` | both | `C:\labhost\hooks\Assert-TrustedJob.ps1` |
| `ACTIONS_RUNNER_HOOK_JOB_COMPLETED` | both | `C:\labhost\hooks\Complete-Job.ps1` |
| `LABHOST_ROOT` | both | `C:\labhost` |
| `LABHOST_ROLE` | both | `iar` or `hil` |
| `LS_EWARM_DIR` | `iar` | EWARM installation root; `_iar-build.yml` runs `arm\bin\iccarm.exe --version` from it, and the IAR build scripts use it |
| `LABHOST_PSU_ADDRESS` | `hil` | SCPI raw-socket endpoint `host:port` of the bench supply |
| `LABHOST_PSU_OFF_COMMAND`, `LABHOST_PSU_STATE_QUERY` | `hil`, optional | Defaults `OUTP OFF` and `OUTP?`; the read-back must be `0` or `OFF` |

The `.env` holds no secret. The runner credentials written by `config.cmd` stay in the runner directory, protected by its ACL and BitLocker.

## Job-started hook

The runner dot-sources the script before the first step of every job. The job runs only if all checks pass; otherwise the hook exits with status 1 and the job fails in the "Set up runner" step:

| Check | Source |
| --- | --- |
| Repository is `jlurg/locksys` | `GITHUB_REPOSITORY` |
| Event is `push`, `schedule` or `workflow_dispatch` | `GITHUB_EVENT_NAME` |
| Triggering actor is listed in `trusted.txt` (case-insensitive) | `GITHUB_TRIGGERING_ACTOR` |
| Runner `hil` only: no `bench.lock` | `LABHOST_ROLE` or `RUNNER_NAME` |

A missing variable, a missing or empty `trusted.txt` or an unknown role refuses the job. The hook performs local checks only and needs no timeout. Dependabot pushes are refused; after reviewing the change, the maintainer re-runs the failed jobs, which makes the maintainer the triggering actor.

## Job-completed hook

1. Runner `hil` with `LABHOST_PSU_ADDRESS`: sends the off command and reads the output state back (3 s timeout per operation). A failure fails the job, so that a supply left on is visible.
2. Stops processes whose command line references the runner's own work directory. Both runners use the same account, so processes are never matched by name.
3. Cleans the workspace (`git reset --hard`, `git clean -ffdx`) after checking that it lies inside the runner's work directory, and empties the job temporary directory.

## Local checks

On a machine with PowerShell 7, Pester 5.9 and PSScriptAnalyzer 1.25:

```powershell
./tools/labhost/tests/Invoke-LabhostChecks.ps1
```

The hook tests run each hook in a child `pwsh` process with simulated `GITHUB_*` variables, including the dot-sourced form used by the runner, and use a loopback TCP stub in place of the bench supply. On the Lab Host the same hooks can be exercised by hand:

```powershell
$env:GITHUB_REPOSITORY = 'jlurg/locksys'; $env:GITHUB_EVENT_NAME = 'pull_request'; $env:GITHUB_TRIGGERING_ACTOR = 'jlurg'
pwsh -NoProfile -File C:\labhost\hooks\Assert-TrustedJob.ps1 -Role iar; $LASTEXITCODE   # 1: refused
```

## Removal

As `labrunner`, in the runner directory: `.\config.cmd remove --token <removal token>` (token from `gh api -X POST repos/jlurg/locksys/actions/runners/remove-token --jq .token`), then delete the Startup entry `locksys-runner-<role>.cmd`.

## Deferred (LATER)

- Signed-head check in the job-started hook (GitHub-verified signature of the head commit); it needs a network call with its own timeout.
- Separate hook sets per runner.
- Periodic bench idle-off task independent of the job-completed hook.
- Just-in-time runner registration.
