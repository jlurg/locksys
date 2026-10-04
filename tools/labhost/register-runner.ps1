# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Installs and registers one Lab Host runner instance (iar or hil) for jlurg/locksys.

.DESCRIPTION
    Run as the standard account labrunner (not elevated) after setup-labhost.ps1, once per
    role. The script is idempotent:

      1. Downloads the pinned GitHub Actions runner release into
         <LabhostRoot>\runners\<Role> and verifies its SHA-256.
      2. Writes the runner .env: job hooks in <LabhostRoot>\hooks, LABHOST_ROOT,
         LABHOST_ROLE and the role-specific settings (LS_EWARM_DIR for iar,
         LABHOST_PSU_* for hil). Other .env entries are kept.
      3. Registers the runner to the repository as labhost-<Role> with the custom label
         <Role> (the runner adds self-hosted, Windows and X64), unless it is already
         registered; -Replace re-registers.
      4. Installs the Python version used by CI gates and the HIL framework through uv.
      5. Starts the runner at logon of the account (Startup folder entry for run.cmd);
         the runner is not a Windows service, because the HIL runner needs the
         interactive session.

    The registration token is valid for one hour; obtain it on the workstation with
    gh api -X POST repos/jlurg/locksys/actions/runners/registration-token --jq .token
    It is read as a secure string, passed to config.cmd only and never stored.

.PARAMETER Role
    Runner role: iar (IAR build, C-STAT) or hil (bench).

.PARAMETER EwarmDir
    IAR Embedded Workbench for Arm installation root (role iar), written as LS_EWARM_DIR.

.PARAMETER PsuAddress
    SCPI endpoint host:port of the bench supply (role hil), written as LABHOST_PSU_ADDRESS.

.PARAMETER Token
    Registration token; prompted for when omitted.

.PARAMETER Replace
    Re-register an existing runner configuration.

.EXAMPLE
    .\tools\labhost\register-runner.ps1 -Role iar -EwarmDir 'C:\Program Files\IAR Systems\Embedded Workbench 9.70'
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [ValidateSet('iar', 'hil')] [string] $Role,
    [string] $LabhostRoot = 'C:\labhost',
    [string] $RepositoryUrl = 'https://github.com/jlurg/locksys',
    [string] $RunnerVersion = '2.337.0',
    [string] $RunnerSha256 = '1150692afa94e71f872017e254ea55b6eece1eece3fe7e3a6d4c93d0a1b85cfc',
    [string] $EwarmDir = '',
    [string] $PsuAddress = '',
    [string] $PythonVersion = '3.13',
    [System.Security.SecureString] $Token,
    [switch] $Replace
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'

function Write-Step {
    param([string] $Message)
    Write-Host "==> $Message"
}

function Write-EnvFile {
    <#
    .SYNOPSIS
        Sets KEY=VALUE entries in a runner .env file, keeping all other lines.
    #>
    param([Parameter(Mandatory = $true)] [string] $Path, [Parameter(Mandatory = $true)] [hashtable] $Values)
    $lines = @()
    if (Test-Path -LiteralPath $Path) { $lines = @(Get-Content -LiteralPath $Path) }
    $kept = @($lines | Where-Object {
            $key = ($_ -split '=', 2)[0].Trim()
            -not $Values.ContainsKey($key)
        })
    $added = @($Values.Keys | Sort-Object | ForEach-Object { "$_=$($Values[$_])" })
    Set-Content -LiteralPath $Path -Value ($kept + $added) -Encoding ASCII
}

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script as the standard runner account in a non-elevated session'
}
if ($Role -eq 'iar' -and -not $EwarmDir) {
    throw '-EwarmDir is required for the iar runner'
}
if ($Role -eq 'iar' -and -not (Test-Path -LiteralPath (Join-Path $EwarmDir 'arm\bin\iccarm.exe'))) {
    throw "iccarm.exe not found under $EwarmDir"
}

$runnerDir = Join-Path $LabhostRoot "runners\$Role"
$runnerName = "labhost-$Role"
if (-not (Test-Path -LiteralPath $runnerDir)) { throw "$runnerDir does not exist; run setup-labhost.ps1 first" }

# 1. Runner software ------------------------------------------------------------------------
Write-Step "GitHub Actions runner $RunnerVersion in $runnerDir"
$versionMarker = Join-Path $runnerDir '.locksys-runner-version'
$installed = if (Test-Path -LiteralPath $versionMarker) { (Get-Content -LiteralPath $versionMarker -TotalCount 1).Trim() } else { '' }
if (-not (Test-Path -LiteralPath (Join-Path $runnerDir 'config.cmd')) -or ($installed -ne $RunnerVersion -and $Replace)) {
    $archive = Join-Path $env:TEMP "actions-runner-win-x64-$RunnerVersion.zip"
    $url = "https://github.com/actions/runner/releases/download/v$RunnerVersion/actions-runner-win-x64-$RunnerVersion.zip"
    Invoke-WebRequest -Uri $url -OutFile $archive -UseBasicParsing -TimeoutSec 600
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    if ($hash -ne $RunnerSha256.ToUpperInvariant()) {
        Remove-Item -LiteralPath $archive -Force
        throw "runner archive SHA-256 mismatch: $hash"
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $runnerDir -Force
    Remove-Item -LiteralPath $archive -Force
    Set-Content -LiteralPath $versionMarker -Value $RunnerVersion -Encoding ASCII
    Write-Host '    runner extracted'
}
else {
    Write-Host '    runner present (auto-update keeps it current)'
}

# 2. Runner environment ---------------------------------------------------------------------
Write-Step 'Runner .env'
$values = @{
    'ACTIONS_RUNNER_HOOK_JOB_STARTED'   = (Join-Path $LabhostRoot 'hooks\Assert-TrustedJob.ps1')
    'ACTIONS_RUNNER_HOOK_JOB_COMPLETED' = (Join-Path $LabhostRoot 'hooks\Complete-Job.ps1')
    'LABHOST_ROOT'                      = $LabhostRoot
    'LABHOST_ROLE'                      = $Role
}
if ($Role -eq 'iar') { $values['LS_EWARM_DIR'] = $EwarmDir }
if ($Role -eq 'hil' -and $PsuAddress) { $values['LABHOST_PSU_ADDRESS'] = $PsuAddress }
foreach ($hook in @($values['ACTIONS_RUNNER_HOOK_JOB_STARTED'], $values['ACTIONS_RUNNER_HOOK_JOB_COMPLETED'])) {
    if (-not (Test-Path -LiteralPath $hook)) { throw "hook $hook is missing; run setup-labhost.ps1 first" }
}
Write-EnvFile -Path (Join-Path $runnerDir '.env') -Values $values

# 3. Registration ---------------------------------------------------------------------------
Write-Step "Registration as $runnerName"
$configured = Test-Path -LiteralPath (Join-Path $runnerDir '.runner')
if ($configured -and -not $Replace) {
    Write-Host '    already registered (use -Replace to re-register)'
}
else {
    if ($null -eq $Token) {
        $Token = Read-Host -AsSecureString 'Runner registration token'
    }
    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Token)
    try {
        $plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
        $arguments = @(
            '--unattended', '--url', $RepositoryUrl, '--token', $plain,
            '--name', $runnerName, '--labels', $Role, '--work', '_work'
        )
        if ($Replace) { $arguments += '--replace' }
        Push-Location -LiteralPath $runnerDir
        try {
            & (Join-Path $runnerDir 'config.cmd') @arguments
            if ($LASTEXITCODE -ne 0) { throw "config.cmd failed with exit code $LASTEXITCODE" }
        }
        finally {
            Pop-Location
        }
    }
    finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
        $plain = $null
    }
}

# 4. Python for CI gates and the HIL framework ----------------------------------------------
Write-Step "Python $PythonVersion through uv"
if (Get-Command uv -ErrorAction SilentlyContinue) {
    & uv python install $PythonVersion
    if ($LASTEXITCODE -ne 0) { throw "uv python install $PythonVersion failed" }
}
else {
    Write-Warning 'uv not found on PATH; run setup-labhost.ps1 and log on again'
}

# 5. Start at logon -------------------------------------------------------------------------
Write-Step 'Start at logon'
$startup = [Environment]::GetFolderPath('Startup')
$launcher = Join-Path $startup "locksys-runner-$Role.cmd"
$launcherLines = @(
    '@echo off',
    "start `"$runnerName`" /min /d `"$runnerDir`" cmd /c run.cmd"
)
Set-Content -LiteralPath $launcher -Value $launcherLines -Encoding ASCII
Write-Host "    $launcher"

Write-Host ''
Write-Host "Start the runner now with: $launcher"
Write-Host "Then check that $runnerName is listed as online under Settings > Actions > Runners."
