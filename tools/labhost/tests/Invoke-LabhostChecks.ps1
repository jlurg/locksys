# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Static and unit checks of the repository PowerShell scripts (CI job workflow-lint).

.DESCRIPTION
    1. Parses every *.ps1, *.psm1 and *.psd1 file of the repository (vendored, generated
       and build directories excluded) and fails on any syntax error.
    2. Runs PSScriptAnalyzer (Warning and Error) on tools/labhost.
    3. Runs the Pester tests in tools/labhost/tests.

    Requires PowerShell 7 with Pester 5.9 and PSScriptAnalyzer 1.25 (preinstalled on the
    GitHub-hosted ubuntu-24.04 image).

.PARAMETER RepositoryRoot
    Repository root; default: three levels above this script.
#>
[CmdletBinding()]
param(
    [string] $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'

$failed = $false
$excluded = '[\\/](third_party|gen|gen_vs|build|node_modules|\.git|\.venv)[\\/]'

$files = @(Get-ChildItem -LiteralPath $RepositoryRoot -Recurse -File -Include '*.ps1', '*.psm1', '*.psd1' |
        Where-Object { $_.FullName -notmatch $excluded })
foreach ($file in $files) {
    $tokens = $null
    $errors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile($file.FullName, [ref]$tokens, [ref]$errors)
    foreach ($parseError in @($errors)) {
        Write-Output ("::error file={0},line={1}::{2}" -f $file.FullName, $parseError.Extent.StartLineNumber, $parseError.Message)
        $failed = $true
    }
}
Write-Output "Parsed $($files.Count) PowerShell file(s)"

Import-Module PSScriptAnalyzer -MinimumVersion 1.25.0
$labhost = Join-Path $RepositoryRoot 'tools/labhost'
$settings = Join-Path $labhost 'PSScriptAnalyzerSettings.psd1'
$findings = @(Invoke-ScriptAnalyzer -Path $labhost -Recurse -Settings $settings)
foreach ($finding in $findings) {
    Write-Output ("::error file={0},line={1}::{2}: {3}" -f $finding.ScriptPath, $finding.Line, $finding.RuleName, $finding.Message)
    $failed = $true
}
Write-Output "PSScriptAnalyzer: $($findings.Count) finding(s)"

Import-Module Pester -MinimumVersion 5.9.0
$configuration = New-PesterConfiguration
$configuration.Run.Path = Join-Path $labhost 'tests'
$configuration.Run.PassThru = $true
$configuration.Output.Verbosity = 'Detailed'
$result = Invoke-Pester -Configuration $configuration
if ($result.FailedCount -gt 0 -or $result.Result -ne 'Passed') {
    $failed = $true
}

if ($failed) { exit 1 }
exit 0
