# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Checks the DCU toolchain of the Lab Host against tools/versions.env.

.DESCRIPTION
    Required: EWARM (LS_EWARM_DIR) with iccarm, IarBuild and ielftool, and an iccarm version
    on the IAR_EWARM_BASELINE line. Reported when present: icstat, IAR Visual State
    (LS_VS_DIR, at least IAR_VISUAL_STATE_MIN), STM32_Programmer_CLI, uv and git.
    Exit code 0 when every required item passes, else 1.

.PARAMETER EwarmDir
    EWARM installation directory; default LS_EWARM_DIR.

.PARAMETER VsDir
    IAR Visual State installation directory; default LS_VS_DIR.

.EXAMPLE
    ./firmware/dcu/scripts/iar/Test-Toolchain.ps1
#>
[CmdletBinding()]
param(
    [string] $EwarmDir = $env:LS_EWARM_DIR,
    [string] $VsDir = $env:LS_VS_DIR
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../DcuTools.psm1') -Force

$results = [System.Collections.Generic.List[object]]::new()
function Add-Result([string] $Item, [bool] $Required, [bool] $Ok, [string] $Detail) {
    $results.Add([pscustomobject]@{ Item = $Item; Required = $Required; Ok = $Ok; Detail = $Detail })
}

$baseline = Get-DcuPin -Name 'IAR_EWARM_BASELINE'
try {
    $dir = Resolve-DcuEwarmDir -EwarmDir $EwarmDir
    foreach ($tool in @('iarbuild', 'ielftool')) {
        try { Add-Result $tool $true $true (Get-DcuEwarmTool -EwarmDir $dir -Tool $tool) }
        catch { Add-Result $tool $true $false $_.Exception.Message }
    }
    try { Add-Result 'icstat' $false $true (Get-DcuEwarmTool -EwarmDir $dir -Tool icstat) }
    catch { Add-Result 'icstat' $false $false $_.Exception.Message }
    $version = Get-DcuEwarmVersion -EwarmDir $dir
    $line = '{0}.{1}' -f $version.Major, $version.Minor
    Add-Result 'iccarm version' $true ($line -eq $baseline) "$version (pin IAR_EWARM_BASELINE=$baseline)"
}
catch {
    Add-Result 'EWARM' $true $false $_.Exception.Message
}

$vsMin = Get-DcuPin -Name 'IAR_VISUAL_STATE_MIN'
if ($VsDir -and (Test-Path -LiteralPath $VsDir)) {
    $coder = @(Get-ChildItem -LiteralPath $VsDir -Recurse -File -Filter 'Coder.exe' -ErrorAction SilentlyContinue)
    if ($coder.Count -gt 0) {
        $fileVersion = $coder[0].VersionInfo.FileVersion
        $ok = $fileVersion -and ([version]($fileVersion -replace '[^0-9.].*$', '') -ge [version]$vsMin)
        Add-Result 'Visual State Coder' $false ([bool]$ok) "$($coder[0].FullName) $fileVersion (minimum $vsMin)"
    }
    else { Add-Result 'Visual State Coder' $false $false "Coder.exe not found under $VsDir" }
}
else { Add-Result 'Visual State Coder' $false $false 'LS_VS_DIR not set' }

foreach ($command in @('STM32_Programmer_CLI', 'uv', 'git')) {
    $found = Get-Command $command -ErrorAction SilentlyContinue
    Add-Result $command $false ([bool]$found) $(if ($found) { $found.Source } else { 'not on PATH' })
}

$results | Format-Table -AutoSize | Out-String -Width 200 | Write-Output
$failed = @($results | Where-Object { $_.Required -and -not $_.Ok })
if ($failed.Count -gt 0) { exit 1 }
exit 0
