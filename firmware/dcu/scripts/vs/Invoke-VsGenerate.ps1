# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Generates the Visual State engines of the DCU from the committed model and option files.

.DESCRIPTION
    Runs the Classic Coder for each variant:
        Coder.exe dcu_fsm.vsp --@options/coder_<variant>.opt
    in firmware/dcu/model/visualstate (the -path option of each file selects
    firmware/dcu/gen_vs/<variant>/). The README.md of each variant directory is kept; every
    other file there is replaced.

    Without -Check the manifest firmware/dcu/gen_vs/VS_MANIFEST.json is updated
    (tools/vs/vs_manifest.py --update). With -Check the regenerated code is compared with the
    committed code (git diff and untracked files under firmware/dcu/gen_vs) and with the
    manifest; the script exits with 1 on any difference.

.PARAMETER Check
    Regenerates and fails on a difference to the committed code.

.PARAMETER VsDir
    IAR Visual State installation directory; default LS_VS_DIR.

.PARAMETER VsVersion
    Visual State version recorded in the manifest; default LS_VS_VERSION.

.PARAMETER Variants
    Variants to generate.

.EXAMPLE
    ./firmware/dcu/scripts/vs/Invoke-VsGenerate.ps1

.EXAMPLE
    ./firmware/dcu/scripts/vs/Invoke-VsGenerate.ps1 -Check
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [switch] $Check,
    [string] $VsDir = $env:LS_VS_DIR,
    [string] $VsVersion = $env:LS_VS_VERSION,
    [ValidateSet('release', 'debug')] [string[]] $Variants = @('release', 'debug')
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../DcuTools.psm1') -Force

$repoRoot = Get-DcuRepoRoot
$modelDir = Join-Path $repoRoot 'firmware/dcu/model/visualstate'
$genDir = Join-Path $repoRoot 'firmware/dcu/gen_vs'
$project = Join-Path $modelDir 'dcu_fsm.vsp'
if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
    throw "Visual State project not found: $project (procedure A of docs/04_software/dcu/visual_state_guide.md)"
}
if (-not $VsDir) { throw 'LS_VS_DIR is not set and no -VsDir was given' }
$coder = @(Get-ChildItem -LiteralPath $VsDir -Recurse -File -Filter 'Coder.exe' -ErrorAction SilentlyContinue)
if ($coder.Count -eq 0) { throw "Coder.exe not found under $VsDir" }
$coderExe = $coder[0].FullName

foreach ($variant in $Variants) {
    $options = Join-Path $modelDir "options/coder_$variant.opt"
    if (-not (Test-Path -LiteralPath $options -PathType Leaf)) { throw "option file not found: $options" }
    $target = Join-Path $genDir $variant
    if ($PSCmdlet.ShouldProcess($target, "generate $variant engines")) {
        New-Item -ItemType Directory -Force -Path $target | Out-Null
        Get-ChildItem -LiteralPath $target -Force | Where-Object { $_.Name -ne 'README.md' } |
            Remove-Item -Recurse -Force
        Push-Location $modelDir
        try {
            Invoke-DcuNative -FilePath $coderExe -ArgumentList @($project, "--@$options")
        }
        finally {
            Pop-Location
        }
    }
}

$manifestTool = Join-Path $repoRoot 'tools/vs/vs_manifest.py'
if ($Check) {
    $relative = 'firmware/dcu/gen_vs'
    & git -C $repoRoot diff --exit-code --stat -- $relative
    $changed = $LASTEXITCODE -ne 0
    $untracked = @(& git -C $repoRoot ls-files --others --exclude-standard -- $relative)
    foreach ($file in $untracked) { Write-Output "not committed: $file" }
    & uv run --no-project $manifestTool --check
    $manifestDrift = $LASTEXITCODE -ne 0
    if ($changed -or $untracked.Count -gt 0 -or $manifestDrift) {
        Write-Output '::error title=vs-gen::regenerated Visual State code differs from the committed code'
        exit 1
    }
    Write-Output 'Visual State code is up to date'
    exit 0
}

$update = @('run', '--no-project', $manifestTool, '--update')
if ($VsVersion) { $update += @('--vs-version', $VsVersion) }
else { Write-Warning 'LS_VS_VERSION not set: the manifest keeps its recorded Visual State version' }
Invoke-DcuNative -FilePath 'uv' -ArgumentList $update
