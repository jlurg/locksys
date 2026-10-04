# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Builds the DCU IAR project and optionally runs the C-STAT analysis.

.DESCRIPTION
    Builds firmware/dcu/iar/dcu.ewp with iarbuild for each configuration and copies the
    images and map files to <OutDir>/<Config>/ (dcu.out, dcu.hex, dcu.map). With -CStat the
    C-STAT analysis runs for each configuration; SARIF files go to <OutDir>/cstat/ and HTML
    reports to <OutDir>/cstat/html/<Config>/.

    The command forms depend on the EWARM line reported by iccarm:
    - 9.70.x: iarbuild -build, iarbuild -cstat_analyze and iarbuild -cstat_report.
    - 10.10.x and later: iarbuild -build, iarbuild -compdb, then
      iarbuild -E cstat_analyze --compile_commands ... --sarif --generate_report.
    SARIF output of the 9.70.x line is collected from the configuration directory when the
    analysis writes it (Lab Host check D11).

.PARAMETER Configs
    Build configurations (Debug, Hil, Release).

.PARAMETER CStat
    Runs the C-STAT analysis after the build.

.PARAMETER OutDir
    Output directory, relative to the current directory unless absolute.

.PARAMETER EwarmDir
    EWARM installation directory; default LS_EWARM_DIR.

.PARAMETER Project
    IAR project file; default firmware/dcu/iar/dcu.ewp.

.PARAMETER Parallel
    Number of parallel compiler processes.

.EXAMPLE
    ./firmware/dcu/scripts/iar/Invoke-DcuBuild.ps1 -Configs Debug,Hil,Release -CStat
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Hil', 'Release')] [string[]] $Configs = @('Debug', 'Hil', 'Release'),
    [switch] $CStat,
    [string] $OutDir = 'out',
    [string] $EwarmDir = $env:LS_EWARM_DIR,
    [string] $Project,
    [ValidateRange(1, 64)] [int] $Parallel = 4
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../DcuTools.psm1') -Force

$repoRoot = Get-DcuRepoRoot
if (-not $Project) { $Project = Join-Path $repoRoot 'firmware/dcu/iar/dcu.ewp' }
if (-not (Test-Path -LiteralPath $Project -PathType Leaf)) {
    throw "IAR project not found: $Project (create it as described in docs/04_software/dcu/iar_project_setup.md)"
}
$projectDir = Split-Path -Parent (Resolve-Path -LiteralPath $Project).Path
$EwarmDir = Resolve-DcuEwarmDir -EwarmDir $EwarmDir
$iarbuild = Get-DcuEwarmTool -EwarmDir $EwarmDir -Tool iarbuild
$version = Get-DcuEwarmVersion -EwarmDir $EwarmDir
$modernCli = $version.Major -ge 10
Write-Output "EWARM $version ($EwarmDir), C-STAT command form: $(if ($modernCli) { '10.10' } else { '9.70' })"

$out = if ([System.IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path (Get-Location) $OutDir }
$cstatOut = Join-Path $out 'cstat'
New-Item -ItemType Directory -Force -Path $out | Out-Null

foreach ($config in $Configs) {
    Write-Output "::group::Build $config"
    Invoke-DcuNative -FilePath $iarbuild -ArgumentList @($Project, '-build', $config, '-log', 'warnings', '-parallel', "$Parallel")
    $target = Join-Path $out $config
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    foreach ($item in @(@('Exe', 'dcu.out'), @('Exe', 'dcu.hex'), @('List', 'dcu.map'))) {
        $source = Join-Path $projectDir (Join-Path $config (Join-Path $item[0] $item[1]))
        if (-not (Test-Path -LiteralPath $source)) { throw "build output missing: $source" }
        Copy-Item -LiteralPath $source -Destination $target -Force
    }
    Write-Output '::endgroup::'
}

if (-not $CStat) { exit 0 }

New-Item -ItemType Directory -Force -Path $cstatOut | Out-Null
foreach ($config in $Configs) {
    Write-Output "::group::C-STAT $config"
    $html = Join-Path $cstatOut (Join-Path 'html' $config)
    New-Item -ItemType Directory -Force -Path $html | Out-Null
    if ($modernCli) {
        $work = Join-Path $out (Join-Path 'cstat-work' $config)
        New-Item -ItemType Directory -Force -Path $work | Out-Null
        $compdb = Join-Path $work 'compile_commands.json'
        Invoke-DcuNative -FilePath $iarbuild -ArgumentList @($Project, '-compdb', $config, '-output', $compdb)
        $analyze = @('-E', 'cstat_analyze', '--compile_commands', $compdb, '--output_dir', $work, '--sarif', '--generate_report')
        $cstatConfig = Join-Path $repoRoot 'firmware/dcu/cstat/cstat_config.yaml'
        if (Test-Path -LiteralPath $cstatConfig) { $analyze += @('--cstat_config_file', $cstatConfig) }
        Invoke-DcuNative -FilePath $iarbuild -ArgumentList ($analyze + @('dcu'))
        $searchRoot = $work
    }
    else {
        Invoke-DcuNative -FilePath $iarbuild -ArgumentList @($Project, '-cstat_analyze', $config, '-parallel', "$Parallel")
        Invoke-DcuNative -FilePath $iarbuild -ArgumentList @($Project, '-cstat_report', $config)
        $searchRoot = Join-Path $projectDir $config
    }
    $sarif = @(Get-ChildItem -LiteralPath $searchRoot -Recurse -File -Filter '*.sarif' -ErrorAction SilentlyContinue)
    foreach ($file in $sarif) {
        Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $cstatOut ("{0}-{1}" -f $config, $file.Name)) -Force
    }
    if ($sarif.Count -eq 0) { Write-Warning "no SARIF output found for $config under $searchRoot" }
    foreach ($file in @(Get-ChildItem -LiteralPath $searchRoot -Recurse -File -Filter '*.html' -ErrorAction SilentlyContinue)) {
        Copy-Item -LiteralPath $file.FullName -Destination $html -Force
    }
    Write-Output '::endgroup::'
}
exit 0
