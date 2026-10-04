# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Runs the C-STAT gate (tools/ci/cstat_gate.py) on the output of Invoke-DcuBuild.ps1 -CStat.

.DESCRIPTION
    Same checks and arguments as the C-STAT gate step of .github/workflows/_iar-build.yml:
    SARIF results, suppression directives against the deviation register, forbidden symbols
    in the map files (Release maps: no fault-injection symbols) and the compiler version pin.

.PARAMETER OutDir
    Output directory of Invoke-DcuBuild.ps1.

.PARAMETER Deviations
    Deviation register.

.PARAMETER EwarmDir
    EWARM installation directory used for the compiler version; default LS_EWARM_DIR.

.EXAMPLE
    ./firmware/dcu/scripts/iar/Test-CstatGate.ps1 -OutDir out
#>
[CmdletBinding()]
param(
    [string] $OutDir = 'out',
    [string] $Deviations,
    [string] $EwarmDir = $env:LS_EWARM_DIR
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../DcuTools.psm1') -Force

$repoRoot = Get-DcuRepoRoot
if (-not $Deviations) { $Deviations = Join-Path $repoRoot 'docs/08_process/misra/deviations.yaml' }
if (-not (Test-Path -LiteralPath $OutDir -PathType Container)) { throw "output directory not found: $OutDir" }

$versionFile = Join-Path $OutDir 'iccarm-version.txt'
if (-not (Test-Path -LiteralPath $versionFile)) {
    $iccarm = Get-DcuEwarmTool -EwarmDir (Resolve-DcuEwarmDir -EwarmDir $EwarmDir) -Tool iccarm
    & $iccarm --version | Set-Content -LiteralPath $versionFile -Encoding utf8
}

$maps = @(Get-ChildItem -LiteralPath $OutDir -Recurse -File -Filter '*.map' | ForEach-Object { $_.FullName })
$releaseMaps = @($maps | Where-Object { $_ -match '[\\/]Release[\\/]' })
$otherMaps = @($maps | Where-Object { $_ -notmatch '[\\/]Release[\\/]' })
if ($releaseMaps.Count -eq 0) { throw "no Release map file under $OutDir" }

$sources = @('firmware/dcu/src', 'firmware/dcu/cfg') | ForEach-Object { Join-Path $repoRoot $_ }
foreach ($lib in @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'libs') -Directory)) {
    foreach ($sub in @('src', 'include')) {
        $path = Join-Path $lib.FullName $sub
        if (Test-Path -LiteralPath $path) { $sources += $path }
    }
}

$gate = @(
    'run', (Join-Path $repoRoot 'tools/ci/cstat_gate.py'),
    '--sarif', (Join-Path $OutDir 'cstat'),
    '--sarif-out', (Join-Path $OutDir 'cstat-merged.sarif'),
    '--deviations', $Deviations,
    '--compiler-version-file', $versionFile,
    '--versions-env', (Join-Path $repoRoot 'tools/versions.env'),
    '--source-root', $repoRoot
)
$gate += @('--sources') + $sources
foreach ($map in $releaseMaps) { $gate += @('--release-map', $map) }
foreach ($map in $otherMaps) { $gate += @('--map', $map) }
& uv @gate
exit $LASTEXITCODE
