# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Post-build step of the DCU IAR project: ROM fill, ROM CRC and HEX output.

.DESCRIPTION
    Runs ielftool on the linked image:
    1. --fill "0xFF;0x08000000-0x0801EFFB" and
       --checksum "ls_rom_crc:4,crc32:Li,0xFFFFFFFF;0x08000000-0x0801EFFB"
       (CRC-32/MPEG-2 over little-endian words, as the STM32F1 CRC unit computes it);
    2. --ihex: dcu.hex next to the image, produced from the checksummed image.
    With uv on PATH the HEX is checked with tools/romcrc/verify_rom_crc.py.

    Project setting (Build Actions, post-build command line):
    pwsh -NoProfile -ExecutionPolicy Bypass -File "$PROJ_DIR$\..\scripts\iar\Invoke-PostLink.ps1" -Elf "$TARGET_PATH$" -Config "$CONFIG_NAME$"

.PARAMETER Elf
    Linked image (dcu.out); rewritten in place.

.PARAMETER Config
    Build configuration name (reported only).

.PARAMETER EwarmDir
    EWARM installation directory; default LS_EWARM_DIR, else the installation that contains
    the running ielftool on PATH.

.EXAMPLE
    ./firmware/dcu/scripts/iar/Invoke-PostLink.ps1 -Elf firmware/dcu/iar/Release/Exe/dcu.out -Config Release
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $Elf,
    [string] $Config = '',
    [string] $EwarmDir = $env:LS_EWARM_DIR
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../DcuTools.psm1') -Force

# LS-DCU-SAD-001 section 11.1: application region and ROM CRC word.
$range = '0x08000000-0x0801EFFB'
$fill = "0xFF;$range"
$checksum = "ls_rom_crc:4,crc32:Li,0xFFFFFFFF;$range"

if (-not (Test-Path -LiteralPath $Elf -PathType Leaf)) { throw "image not found: $Elf" }
$image = (Resolve-Path -LiteralPath $Elf).Path
$hex = [System.IO.Path]::ChangeExtension($image, '.hex')

if ($EwarmDir) {
    $ielftool = Get-DcuEwarmTool -EwarmDir (Resolve-DcuEwarmDir -EwarmDir $EwarmDir) -Tool ielftool
}
else {
    $ielftool = (Get-Command ielftool -ErrorAction Stop).Source
}

Invoke-DcuNative -FilePath $ielftool -ArgumentList @('--fill', $fill, '--checksum', $checksum, '--verbose', $image, $image)
Invoke-DcuNative -FilePath $ielftool -ArgumentList @('--ihex', '--verbose', $image, $hex)
Write-Output "Post-link $Config`: $hex"

if (Get-Command uv -ErrorAction SilentlyContinue) {
    $verify = Join-Path (Get-DcuRepoRoot) 'tools/romcrc/verify_rom_crc.py'
    Invoke-DcuNative -FilePath 'uv' -ArgumentList @('run', '--no-project', $verify, $hex)
}
else {
    Write-Warning 'uv not found: ROM CRC not cross-checked'
}
