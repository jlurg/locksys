# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Runs the Visual State Verificator on each DCU system and records the result.

.DESCRIPTION
    Runs, in firmware/dcu/model/visualstate:
        Verificator.exe dcu_fsm.vsp <System> --@options/verificator.opt
    for WinCtrl, DoorCtrl and ModeMgr, writes the console output to
    reports/<System>.txt (not committed) and records the exit status per system in
    firmware/dcu/gen_vs/VS_MANIFEST.json. Exits with 1 when the Verificator reports a failure
    for any system.

.PARAMETER VsDir
    IAR Visual State installation directory; default LS_VS_DIR.

.PARAMETER Systems
    Systems to verify.

.EXAMPLE
    ./firmware/dcu/scripts/vs/Invoke-VsVerify.ps1
#>
[CmdletBinding()]
param(
    [string] $VsDir = $env:LS_VS_DIR,
    [ValidateSet('WinCtrl', 'DoorCtrl', 'ModeMgr')] [string[]] $Systems = @('WinCtrl', 'DoorCtrl', 'ModeMgr')
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../DcuTools.psm1') -Force

$repoRoot = Get-DcuRepoRoot
$modelDir = Join-Path $repoRoot 'firmware/dcu/model/visualstate'
$project = Join-Path $modelDir 'dcu_fsm.vsp'
$options = Join-Path $modelDir 'options/verificator.opt'
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) { throw "Visual State project not found: $project" }
if (-not $VsDir) { throw 'LS_VS_DIR is not set and no -VsDir was given' }
$verificator = @(Get-ChildItem -LiteralPath $VsDir -Recurse -File -Filter 'Verificator.exe' -ErrorAction SilentlyContinue)
if ($verificator.Count -eq 0) { throw "Verificator.exe not found under $VsDir" }

$reports = Join-Path $modelDir 'reports'
New-Item -ItemType Directory -Force -Path $reports | Out-Null
$summary = @()
$failed = $false
Push-Location $modelDir
try {
    foreach ($system in $Systems) {
        $report = Join-Path $reports "$system.txt"
        & $verificator[0].FullName $project $system "--@$options" *>&1 | Tee-Object -FilePath $report
        $code = $LASTEXITCODE
        $summary += "$system=exit $code"
        if ($code -ne 0) { $failed = $true }
    }
}
finally {
    Pop-Location
}

$update = @('run', '--no-project', (Join-Path $repoRoot 'tools/vs/vs_manifest.py'), '--update')
foreach ($item in $summary) { $update += @('--verificator', $item) }
Invoke-DcuNative -FilePath 'uv' -ArgumentList $update
if ($failed) { exit 1 }
exit 0
