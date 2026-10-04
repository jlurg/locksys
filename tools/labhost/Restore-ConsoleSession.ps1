# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Reconnects the runner account's interactive session to the physical console.

.DESCRIPTION
    Windows client editions allow one interactive session. A remote desktop logon
    disconnects the session of the runner account, in which the runner instances and the
    logic analyser application run. After the remote desktop session has ended, run this
    script from an elevated shell (for example over SSH) to return that session to the
    console (lab_host.md, check D12).

    tscon needs no password when it runs as SYSTEM, so the script runs it once through a
    temporary scheduled task under the SYSTEM account and removes the task afterwards.

.PARAMETER RunnerUser
    Account whose session is returned to the console.

.EXAMPLE
    .\tools\labhost\Restore-ConsoleSession.ps1
#>
[CmdletBinding()]
param(
    [string] $RunnerUser = 'labrunner'
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'

$explorer = @(Get-Process -Name explorer -IncludeUserName -ErrorAction SilentlyContinue |
        Where-Object { $_.UserName -and $_.UserName.Split('\')[-1] -eq $RunnerUser })
if ($explorer.Count -eq 0) { throw "no interactive session of $RunnerUser found" }
$sessionId = $explorer[0].SessionId

$taskName = 'LockSys-RestoreConsoleSession'
$action = New-ScheduledTaskAction -Execute (Join-Path $env:WINDIR 'System32\tscon.exe') -Argument "$sessionId /dest:console"
$principal = New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
Register-ScheduledTask -TaskName $taskName -Action $action -Principal $principal -Force | Out-Null
try {
    Start-ScheduledTask -TaskName $taskName
    Start-Sleep -Seconds 3
    $result = (Get-ScheduledTaskInfo -TaskName $taskName).LastTaskResult
    if ($result -ne 0) { throw "tscon returned $result" }
    Write-Output "Session $sessionId of $RunnerUser is connected to the console"
}
finally {
    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
}
