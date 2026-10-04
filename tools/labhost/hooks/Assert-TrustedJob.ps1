# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Job-started hook of the Lab Host runners: admits only trusted jobs (fail closed).

.DESCRIPTION
    Installed as ACTIONS_RUNNER_HOOK_JOB_STARTED for both runner instances (iar, hil).
    A job is admitted only when all of the following hold; otherwise the script exits
    non-zero and the runner marks the job failed before any step runs:

      - GITHUB_REPOSITORY equals the expected repository;
      - GITHUB_EVENT_NAME is push, schedule or workflow_dispatch;
      - GITHUB_TRIGGERING_ACTOR is listed in <LabhostRoot>\trusted.txt;
      - on the hil runner, the bench is not reserved (<LabhostRoot>\bench.lock absent).

    Every unexpected condition (missing variable, unreadable or empty trusted.txt,
    unknown runner role) refuses the job. The script performs local checks only and
    no network access. The runner dot-sources the script, so parameters keep their
    defaults there; direct calls may override them for tests.

.PARAMETER LabhostRoot
    Lab Host root directory. Default: $env:LABHOST_ROOT, else C:\labhost.

.PARAMETER Repository
    Repository the runners are registered to.

.PARAMETER Role
    Runner role (iar or hil). Default: $env:LABHOST_ROLE, else derived from
    $env:RUNNER_NAME (labhost-iar, labhost-hil).

.EXAMPLE
    $env:GITHUB_REPOSITORY = 'jlurg/locksys'
    $env:GITHUB_EVENT_NAME = 'push'
    $env:GITHUB_TRIGGERING_ACTOR = 'jlurg'
    pwsh -NoProfile -File .\Assert-TrustedJob.ps1 -LabhostRoot .\test-root -Role iar
#>
[CmdletBinding()]
param(
    [string] $LabhostRoot = $(if ($env:LABHOST_ROOT) { $env:LABHOST_ROOT } else { 'C:\labhost' }),
    [string] $Repository = 'jlurg/locksys',
    [string] $Role = $env:LABHOST_ROLE
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'

$AllowedEvents = @('push', 'schedule', 'workflow_dispatch')

function Get-TrustedActor {
    <#
    .SYNOPSIS
        Returns the actors listed in a trusted-actor file (one per line, '#' comments).
    #>
    param([Parameter(Mandatory = $true)] [string] $Path)
    $lines = @(Get-Content -LiteralPath $Path -ErrorAction Stop)
    return @($lines | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') })
}

function Resolve-RunnerRole {
    <#
    .SYNOPSIS
        Returns the runner role from the parameter or the runner name, or an empty string.
    #>
    param([string] $Role, [string] $RunnerName)
    if ($Role) { return $Role.ToLowerInvariant() }
    if ($RunnerName -match '^labhost-(iar|hil)$') { return $Matches[1] }
    return ''
}

function Get-RefusalReason {
    <#
    .SYNOPSIS
        Returns $null when the job is admitted, otherwise the reason for refusing it.
    #>
    param(
        [Parameter(Mandatory = $true)] [string] $LabhostRoot,
        [Parameter(Mandatory = $true)] [string] $Repository,
        [string] $Role
    )
    $repo = [string]$env:GITHUB_REPOSITORY
    $eventName = [string]$env:GITHUB_EVENT_NAME
    $actor = [string]$env:GITHUB_TRIGGERING_ACTOR

    if ($repo -ne $Repository) { return "repository '$repo' is not '$Repository'" }
    if ($AllowedEvents -notcontains $eventName) { return "event '$eventName' is not allowed on the Lab Host" }

    $resolvedRole = Resolve-RunnerRole -Role $Role -RunnerName ([string]$env:RUNNER_NAME)
    if (@('iar', 'hil') -notcontains $resolvedRole) { return "unknown runner role '$resolvedRole'" }

    $trustedFile = Join-Path $LabhostRoot 'trusted.txt'
    if (-not (Test-Path -LiteralPath $trustedFile -PathType Leaf)) { return "trusted actor list $trustedFile is missing" }
    $trusted = @(Get-TrustedActor -Path $trustedFile)
    if ($trusted.Count -eq 0) { return "trusted actor list $trustedFile is empty" }
    if (-not $actor) { return 'triggering actor is not set' }
    if ($trusted -notcontains $actor) { return "triggering actor '$actor' is not trusted" }

    if ($resolvedRole -eq 'hil' -and (Test-Path -LiteralPath (Join-Path $LabhostRoot 'bench.lock'))) {
        return 'the bench is reserved (bench.lock present)'
    }
    return $null
}

$exitCode = 1
try {
    $reason = Get-RefusalReason -LabhostRoot $LabhostRoot -Repository $Repository -Role $Role
    if ($null -eq $reason) {
        Write-Output ("Lab Host trust gate: admitted {0} {1} triggered by {2}" -f $env:GITHUB_REPOSITORY, $env:GITHUB_EVENT_NAME, $env:GITHUB_TRIGGERING_ACTOR)
        $exitCode = 0
    }
    else {
        Write-Output "::error title=Lab Host trust gate::job refused: $reason"
    }
}
catch {
    Write-Output "::error title=Lab Host trust gate::job refused: hook error: $($_.Exception.Message)"
}
exit $exitCode
