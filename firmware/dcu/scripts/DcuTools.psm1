# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Helper functions of the DCU Lab Host scripts (IAR EWARM, Visual State, flashing).
#>

Set-StrictMode -Version 3.0

function Get-DcuRepoRoot {
    <#
    .SYNOPSIS
        Returns the repository root (three levels above firmware/dcu/scripts).
    #>
    [CmdletBinding()]
    [OutputType([string])]
    param()
    return (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
}

function Get-DcuPin {
    <#
    .SYNOPSIS
        Returns the value of a KEY=VALUE pin of tools/versions.env, or $null.
    .PARAMETER Name
        Pin name, for example IAR_EWARM_BASELINE.
    #>
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [Parameter(Mandatory)] [string] $Name
    )
    $file = Join-Path (Get-DcuRepoRoot) 'tools/versions.env'
    foreach ($line in Get-Content -LiteralPath $file) {
        if ($line -match "^$([regex]::Escape($Name))=(.*)$") { return $Matches[1].Trim() }
    }
    return $null
}

function Resolve-DcuEwarmDir {
    <#
    .SYNOPSIS
        Returns the EWARM installation directory (parameter, else LS_EWARM_DIR).
    .PARAMETER EwarmDir
        Installation directory; empty to use LS_EWARM_DIR.
    #>
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [string] $EwarmDir
    )
    if (-not $EwarmDir) { $EwarmDir = $env:LS_EWARM_DIR }
    if (-not $EwarmDir) { throw 'LS_EWARM_DIR is not set and no -EwarmDir was given' }
    if (-not (Test-Path -LiteralPath $EwarmDir -PathType Container)) {
        throw "EWARM directory not found: $EwarmDir"
    }
    return (Resolve-Path -LiteralPath $EwarmDir).Path
}

function Get-DcuEwarmTool {
    <#
    .SYNOPSIS
        Returns the full path of an EWARM tool and fails when it does not exist.
    .PARAMETER EwarmDir
        EWARM installation directory.
    .PARAMETER Tool
        Tool name: iarbuild, iccarm, ielftool or icstat.
    #>
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [Parameter(Mandatory)] [string] $EwarmDir,
        [Parameter(Mandatory)] [ValidateSet('iarbuild', 'iccarm', 'ielftool', 'icstat')] [string] $Tool
    )
    $relative = @{
        iarbuild = 'common/bin/IarBuild.exe'
        iccarm   = 'arm/bin/iccarm.exe'
        ielftool = 'arm/bin/ielftool.exe'
        icstat   = 'arm/bin/icstat.exe'
    }[$Tool]
    $path = Join-Path $EwarmDir $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "$Tool not found: $path" }
    return $path
}

function Get-DcuEwarmVersion {
    <#
    .SYNOPSIS
        Returns the compiler version reported by iccarm --version (for example 9.70.4).
    .PARAMETER EwarmDir
        EWARM installation directory.
    #>
    [CmdletBinding()]
    [OutputType([version])]
    param(
        [Parameter(Mandatory)] [string] $EwarmDir
    )
    $iccarm = Get-DcuEwarmTool -EwarmDir $EwarmDir -Tool iccarm
    $text = (& $iccarm --version 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "iccarm --version failed with exit code $LASTEXITCODE" }
    if ($text -notmatch 'V(\d+)\.(\d+)\.(\d+)') { throw "cannot read the compiler version from: $text" }
    return [version]::new([int]$Matches[1], [int]$Matches[2], [int]$Matches[3])
}

function Invoke-DcuNative {
    <#
    .SYNOPSIS
        Runs a native command and throws when its exit code is not 0.
    .PARAMETER FilePath
        Executable.
    .PARAMETER ArgumentList
        Arguments, passed unchanged.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] [string] $FilePath,
        [string[]] $ArgumentList = @()
    )
    Write-Verbose ("{0} {1}" -f $FilePath, ($ArgumentList -join ' '))
    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw ("{0} failed with exit code {1}" -f (Split-Path -Leaf $FilePath), $LASTEXITCODE)
    }
}

Export-ModuleMember -Function Get-DcuRepoRoot, Get-DcuPin, Resolve-DcuEwarmDir, Get-DcuEwarmTool,
    Get-DcuEwarmVersion, Invoke-DcuNative
