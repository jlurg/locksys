# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
<#
.SYNOPSIS
    Job-completed hook of the Lab Host runners: bench supply off, stray processes stopped,
    workspace cleaned.

.DESCRIPTION
    Installed as ACTIONS_RUNNER_HOOK_JOB_COMPLETED for both runner instances.

    1. Runner hil only: when LABHOST_PSU_ADDRESS is configured, the bench supply output is
       switched off over SCPI (raw TCP, normally port 5025) and the output state is read
       back. A failure makes the hook exit non-zero so that it is visible in the job.
    2. Processes whose command line references this runner's work directory are stopped
       (both runners run under the same account, so processes are never matched by name).
    3. The job workspace is cleaned (git clean and reset) after checking that it lies
       inside this runner's work directory; the job temporary directory is emptied.

    Every network operation has its own timeout; the runner enforces none for hooks.

.PARAMETER LabhostRoot
    Lab Host root directory. Default: $env:LABHOST_ROOT, else C:\labhost.

.PARAMETER Role
    Runner role (iar or hil). Default: $env:LABHOST_ROLE, else derived from $env:RUNNER_NAME.

.PARAMETER PsuAddress
    SCPI endpoint of the bench supply as host:port. Default: $env:LABHOST_PSU_ADDRESS.

.PARAMETER PsuOffCommand
    SCPI command that switches the output off. Default: $env:LABHOST_PSU_OFF_COMMAND, else 'OUTP OFF'.

.PARAMETER PsuStateQuery
    SCPI query of the output state. Default: $env:LABHOST_PSU_STATE_QUERY, else 'OUTP?'.

.PARAMETER TimeoutMilliseconds
    Timeout of each network operation.
#>
[CmdletBinding()]
param(
    [string] $LabhostRoot = $(if ($env:LABHOST_ROOT) { $env:LABHOST_ROOT } else { 'C:\labhost' }),
    [string] $Role = $env:LABHOST_ROLE,
    [string] $PsuAddress = $env:LABHOST_PSU_ADDRESS,
    [string] $PsuOffCommand = $(if ($env:LABHOST_PSU_OFF_COMMAND) { $env:LABHOST_PSU_OFF_COMMAND } else { 'OUTP OFF' }),
    [string] $PsuStateQuery = $(if ($env:LABHOST_PSU_STATE_QUERY) { $env:LABHOST_PSU_STATE_QUERY } else { 'OUTP?' }),
    [int] $TimeoutMilliseconds = 3000
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'

function Resolve-RunnerRole {
    <#
    .SYNOPSIS
        Returns the runner role from the parameter or the runner name, or an empty string.
    #>
    param([string] $Role, [string] $RunnerName)
    if ($Role) { return $Role.ToLowerInvariant() }
    if ($RunnerName -match '^labhost-(iar|hil)$') { return $Matches[1] }
    return ''
}

function Test-PathInside {
    <#
    .SYNOPSIS
        True when Path is the directory Parent or lies below it (case-insensitive).
    #>
    param([Parameter(Mandatory = $true)] [string] $Path, [Parameter(Mandatory = $true)] [string] $Parent)
    $full = [System.IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    $root = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
    return $full.Equals($root, [StringComparison]::OrdinalIgnoreCase) -or
        $full.StartsWith($root + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
}

function Invoke-ScpiExchange {
    <#
    .SYNOPSIS
        Sends a command and a query to a SCPI raw-socket endpoint; returns the query response.
    #>
    param(
        [Parameter(Mandatory = $true)] [string] $Address,
        [Parameter(Mandatory = $true)] [string] $Command,
        [Parameter(Mandatory = $true)] [string] $Query,
        [Parameter(Mandatory = $true)] [int] $TimeoutMilliseconds
    )
    if ($Address -notmatch '^(?<host>[^:]+):(?<port>\d{1,5})$') { throw "PSU address '$Address' is not host:port" }
    $client = New-Object System.Net.Sockets.TcpClient
    try {
        if (-not $client.ConnectAsync($Matches['host'], [int]$Matches['port']).Wait($TimeoutMilliseconds)) {
            throw "connection to $Address timed out"
        }
        $stream = $client.GetStream()
        $stream.ReadTimeout = $TimeoutMilliseconds
        $stream.WriteTimeout = $TimeoutMilliseconds
        $writer = New-Object System.IO.StreamWriter($stream, [System.Text.Encoding]::ASCII)
        $writer.NewLine = "`n"
        $writer.AutoFlush = $true
        $reader = New-Object System.IO.StreamReader($stream, [System.Text.Encoding]::ASCII)
        $writer.WriteLine($Command)
        $writer.WriteLine($Query)
        return [string]$reader.ReadLine()
    }
    finally {
        $client.Dispose()
    }
}

function Invoke-StrayProcessCleanup {
    <#
    .SYNOPSIS
        Stops processes whose command line references the given work directory.
    #>
    param([Parameter(Mandatory = $true)] [string] $WorkRoot)
    if (-not (Get-Command Get-CimInstance -ErrorAction SilentlyContinue)) { return }
    $candidates = @(Get-CimInstance -ClassName Win32_Process -ErrorAction SilentlyContinue | Where-Object {
            $_.ProcessId -ne $PID -and $_.CommandLine -and
            $_.CommandLine.IndexOf($WorkRoot, [StringComparison]::OrdinalIgnoreCase) -ge 0
        })
    foreach ($process in $candidates) {
        Write-Output ("Stopping stray process {0} ({1})" -f $process.ProcessId, $process.Name)
        Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue
    }
}

$failed = $false
$resolvedRole = Resolve-RunnerRole -Role $Role -RunnerName ([string]$env:RUNNER_NAME)

if ($resolvedRole -eq 'hil') {
    if ($PsuAddress) {
        try {
            $state = (Invoke-ScpiExchange -Address $PsuAddress -Command $PsuOffCommand -Query $PsuStateQuery -TimeoutMilliseconds $TimeoutMilliseconds).Trim()
            if (@('0', 'OFF') -contains $state.ToUpperInvariant()) {
                Write-Output 'Bench supply output is off'
            }
            else {
                Write-Output "::error title=Lab Host cleanup::bench supply reports output state '$state' after '$PsuOffCommand'"
                $failed = $true
            }
        }
        catch {
            Write-Output "::error title=Lab Host cleanup::bench supply could not be switched off: $($_.Exception.Message)"
            $failed = $true
        }
    }
    else {
        Write-Output '::warning title=Lab Host cleanup::LABHOST_PSU_ADDRESS is not configured; bench supply not switched off'
    }
}

if (@('iar', 'hil') -contains $resolvedRole) {
    $workRoot = Join-Path (Join-Path (Join-Path $LabhostRoot 'runners') $resolvedRole) '_work'
    try {
        Invoke-StrayProcessCleanup -WorkRoot $workRoot
        $workspace = [string]$env:GITHUB_WORKSPACE
        if ($workspace -and (Test-Path -LiteralPath $workspace) -and (Test-PathInside -Path $workspace -Parent $workRoot)) {
            if (Test-Path -LiteralPath (Join-Path $workspace '.git')) {
                & git -C $workspace reset --hard --quiet
                & git -C $workspace clean -ffdx --quiet
            }
            else {
                Get-ChildItem -LiteralPath $workspace -Force | Remove-Item -Recurse -Force
            }
            Write-Output "Workspace cleaned: $workspace"
        }
        elseif ($workspace) {
            Write-Output "::warning title=Lab Host cleanup::workspace '$workspace' is outside $workRoot; not cleaned"
        }
        $temp = [string]$env:RUNNER_TEMP
        if ($temp -and (Test-Path -LiteralPath $temp) -and (Test-PathInside -Path $temp -Parent $workRoot)) {
            Get-ChildItem -LiteralPath $temp -Force | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    catch {
        Write-Output "::warning title=Lab Host cleanup::workspace cleanup incomplete: $($_.Exception.Message)"
    }
}
else {
    Write-Output "::warning title=Lab Host cleanup::unknown runner role '$resolvedRole'; nothing cleaned"
}

if ($failed) { exit 1 }
exit 0
