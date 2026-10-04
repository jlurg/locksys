# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Programs a DCU image into the NUCLEO-F103RB through the on-board ST-LINK.

.DESCRIPTION
    Checks the ROM CRC of the image with tools/romcrc/verify_rom_crc.py, then runs
    STM32_Programmer_CLI: connect under reset over SWD, write, verify and reset.
    Bench rule: the 12 V supply output is off or limited while the target is halted.

.PARAMETER Image
    Image to program (.hex).

.PARAMETER Programmer
    STM32_Programmer_CLI executable; default LS_STM32_PROGRAMMER_CLI, else PATH.

.PARAMETER ProbeSerial
    ST-LINK serial number when several probes are connected.

.PARAMETER SkipCrcCheck
    Programs an image without a valid ROM CRC (negative tests only).

.EXAMPLE
    ./firmware/dcu/scripts/iar/Invoke-DcuFlash.ps1 -Image out/Release/dcu.hex
#>
[CmdletBinding(SupportsShouldProcess)]
param(
    [string] $Image = 'out/Release/dcu.hex',
    [string] $Programmer = $env:LS_STM32_PROGRAMMER_CLI,
    [string] $ProbeSerial,
    [switch] $SkipCrcCheck
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot '../DcuTools.psm1') -Force

if (-not (Test-Path -LiteralPath $Image -PathType Leaf)) { throw "image not found: $Image" }
$imagePath = (Resolve-Path -LiteralPath $Image).Path
if (-not $Programmer) { $Programmer = (Get-Command STM32_Programmer_CLI -ErrorAction Stop).Source }

if (-not $SkipCrcCheck) {
    $verify = Join-Path (Get-DcuRepoRoot) 'tools/romcrc/verify_rom_crc.py'
    Invoke-DcuNative -FilePath 'uv' -ArgumentList @('run', '--no-project', $verify, $imagePath)
}

$connect = @('-c', 'port=SWD', 'mode=UR', 'reset=HWrst')
if ($ProbeSerial) { $connect += "sn=$ProbeSerial" }
if ($PSCmdlet.ShouldProcess($imagePath, 'program DCU')) {
    Invoke-DcuNative -FilePath $Programmer -ArgumentList ($connect + @('-w', $imagePath, '-v', '-rst'))
}
