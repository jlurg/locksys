# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Prepares the Windows Lab Host for the LockSys self-hosted runners (idempotent).

.DESCRIPTION
    Run from an elevated Windows PowerShell 5.1 or PowerShell 7 session in a clone of the
    repository. Every step checks the current state first, so the script can be re-run. It
    stores no secret: the password of the runner account is read interactively and passed
    to Windows only.

    Steps:
      1. Lab Host directories (C:\labhost\{hooks,runners\iar,runners\hil,evidence,tools}).
      2. Standard (non-administrator) account for both runner instances.
      3. ACLs: C:\labhost, its hooks, tools and trusted.txt are writable only by
         Administrators and SYSTEM; runners\iar, runners\hil and evidence are writable by
         the runner account.
      4. Hook scripts copied from tools\labhost\hooks; trusted.txt created if missing.
      5. OpenSSH Server with public-key authentication only.
      6. Remote desktop with Network Level Authentication.
      7. Windows Firewall: inbound SSH and remote desktop only from the workstation address.
      8. Git for Windows and PowerShell 7 through winget when missing; uv from the pinned
         release archive (SHA-256 checked) into C:\labhost\tools\uv on the machine PATH;
         PowerShell 7 as the OpenSSH default shell.
      9. Machine settings: script execution policy, long paths, power plan without sleep or
         hibernation, USB selective suspend off.

    The remaining manual steps are printed at the end.

.PARAMETER LabhostRoot
    Lab Host root directory.

.PARAMETER RunnerUser
    Local standard account that runs both runner instances.

.PARAMETER TrustedActors
    GitHub logins written to trusted.txt when the file does not exist yet.

.PARAMETER AllowedRemoteAddress
    Workstation address(es) or tailnet range(s) allowed to reach SSH and remote desktop,
    for example 192.168.1.20 or 100.64.0.0/10. Without it the firewall rules are unchanged.

.PARAMETER AdminPublicKeyFile
    OpenSSH public key of the maintainer, added to administrators_authorized_keys.

.PARAMETER UvVersion
    uv release installed into C:\labhost\tools\uv (keep equal to the repository pin).

.PARAMETER UvSha256
    SHA-256 of uv-x86_64-pc-windows-msvc.zip of UvVersion.

.PARAMETER SkipSoftware
    Do not install Git, PowerShell 7 or uv.

.EXAMPLE
    .\tools\labhost\setup-labhost.ps1 -AllowedRemoteAddress 192.168.1.20 -AdminPublicKeyFile .\id_ed25519.pub
#>
[CmdletBinding()]
param(
    [string] $LabhostRoot = 'C:\labhost',
    [string] $RunnerUser = 'labrunner',
    [string[]] $TrustedActors = @('jlurg'),
    [string[]] $AllowedRemoteAddress = @(),
    [string] $AdminPublicKeyFile = '',
    [string] $UvVersion = '0.12.23',
    [string] $UvSha256 = '75d05de6762778c31ee183398de7dd15093fad0ed90b1f236d8205ea5ec00c90',
    [switch] $SkipSoftware
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'

$SidAdministrators = 'S-1-5-32-544'
$SidUsers = 'S-1-5-32-545'
$SidSystem = 'S-1-5-18'
$RemoteDesktopGroup = '@FirewallAPI.dll,-28752'
$UsbSettingsGroup = '2a737441-1930-4402-8d77-b2bebba308a3'
$UsbSelectiveSuspend = '48e6b7a6-50f5-4782-a5d4-53bb8f07e226'

function Write-Step {
    param([string] $Message)
    Write-Host "==> $Message"
}

function Invoke-Native {
    <#
    .SYNOPSIS
        Runs a native command and throws on a non-zero exit code.
    #>
    param([Parameter(Mandatory = $true)] [string] $FilePath, [string[]] $Arguments = @())
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$FilePath $($Arguments -join ' ') failed with exit code $LASTEXITCODE" }
}

function Grant-DirectoryAccess {
    <#
    .SYNOPSIS
        Replaces the ACL of a directory: Administrators and SYSTEM full control plus the
        given access of the runner account, inherited by children.
    #>
    param([Parameter(Mandatory = $true)] [string] $Path, [Parameter(Mandatory = $true)] [string] $RunnerAccess)
    Invoke-Native -FilePath icacls.exe -Arguments @(
        $Path, '/inheritance:r',
        '/grant:r', "*${SidAdministrators}:(OI)(CI)F",
        '/grant:r', "*${SidSystem}:(OI)(CI)F",
        '/grant:r', "${RunnerUser}:(OI)(CI)$RunnerAccess"
    ) | Out-Null
}

function Merge-SshdOption {
    <#
    .SYNOPSIS
        Returns a copy of the sshd_config lines with a global keyword set before the first
        Match block.
    #>
    param([Parameter(Mandatory = $true)] [string[]] $Lines, [string] $Name, [string] $Value)
    $result = [System.Collections.Generic.List[string]]::new()
    $result.AddRange($Lines)
    $end = $result.FindIndex([Predicate[string]] { param($line) $line -match '^\s*Match\s' })
    if ($end -lt 0) { $end = $result.Count }
    $wanted = "$Name $Value"
    for ($i = 0; $i -lt $end; $i++) {
        if ($result[$i] -match "^\s*#?\s*$Name\s") {
            $result[$i] = $wanted
            return , $result.ToArray()
        }
    }
    $result.Insert($end, $wanted)
    return , $result.ToArray()
}

function Install-WingetPackage {
    <#
    .SYNOPSIS
        Installs a winget package machine-wide when the given command is not available.
    #>
    param([Parameter(Mandatory = $true)] [string] $Command, [Parameter(Mandatory = $true)] [string] $PackageId)
    if (Get-Command $Command -ErrorAction SilentlyContinue) {
        Write-Host "    $Command present"
        return
    }
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Write-Warning "$Command missing and winget unavailable: install $PackageId manually"
        return
    }
    Invoke-Native -FilePath winget -Arguments @(
        'install', '--id', $PackageId, '--exact', '--scope', 'machine', '--silent',
        '--accept-package-agreements', '--accept-source-agreements'
    )
}

function Install-Uv {
    <#
    .SYNOPSIS
        Installs a uv release (SHA-256 checked) into Target and adds Target to the machine PATH.
    #>
    param(
        [Parameter(Mandatory = $true)] [string] $Version,
        [Parameter(Mandatory = $true)] [string] $Sha256,
        [Parameter(Mandatory = $true)] [string] $Target
    )
    $target = $Target
    $uv = Join-Path $target 'uv.exe'
    if ((Test-Path -LiteralPath $uv) -and ((& $uv --version) -match "^uv $([regex]::Escape($Version))(\s|$)")) {
        Write-Host "    uv $Version present"
    }
    else {
        $archive = Join-Path $env:TEMP "uv-$Version-x86_64-pc-windows-msvc.zip"
        $url = "https://github.com/astral-sh/uv/releases/download/$Version/uv-x86_64-pc-windows-msvc.zip"
        Invoke-WebRequest -Uri $url -OutFile $archive -UseBasicParsing -TimeoutSec 300
        $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
        if ($hash -ne $Sha256.ToUpperInvariant()) {
            Remove-Item -LiteralPath $archive -Force
            throw "uv archive SHA-256 mismatch: $hash"
        }
        New-Item -ItemType Directory -Force -Path $target | Out-Null
        Expand-Archive -LiteralPath $archive -DestinationPath $target -Force
        Remove-Item -LiteralPath $archive -Force
        Write-Host "    uv $Version installed in $target"
    }
    $machinePath = [Environment]::GetEnvironmentVariable('Path', 'Machine')
    if (@($machinePath -split ';') -notcontains $target) {
        [Environment]::SetEnvironmentVariable('Path', ($machinePath.TrimEnd(';') + ';' + $target), 'Machine')
    }
}

# 1. Directories ----------------------------------------------------------------------------
Write-Step "Lab Host directories under $LabhostRoot"
$runnerDirs = @(
    (Join-Path $LabhostRoot 'runners\iar'),
    (Join-Path $LabhostRoot 'runners\hil'),
    (Join-Path $LabhostRoot 'evidence')
)
$adminDirs = @($LabhostRoot, (Join-Path $LabhostRoot 'hooks'), (Join-Path $LabhostRoot 'runners'), (Join-Path $LabhostRoot 'tools'))
foreach ($dir in $adminDirs + $runnerDirs) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

# 2. Runner account -------------------------------------------------------------------------
Write-Step "Standard account $RunnerUser"
if (-not (Get-LocalUser -Name $RunnerUser -ErrorAction SilentlyContinue)) {
    $password = Read-Host -AsSecureString "Password for the new account $RunnerUser"
    New-LocalUser -Name $RunnerUser -Password $password -PasswordNeverExpires -AccountNeverExpires `
        -Description 'LockSys GitHub Actions runners' | Out-Null
    Write-Host "    created $RunnerUser"
}
$userMembers = @(Get-LocalGroupMember -SID $SidUsers | ForEach-Object { $_.Name.Split('\')[-1] })
if ($userMembers -notcontains $RunnerUser) {
    Add-LocalGroupMember -SID $SidUsers -Member $RunnerUser
}
$adminMembers = @(Get-LocalGroupMember -SID $SidAdministrators | ForEach-Object { $_.Name.Split('\')[-1] })
if ($adminMembers -contains $RunnerUser) {
    throw "$RunnerUser is a member of Administrators; remove it before continuing"
}

# 3. Permissions ----------------------------------------------------------------------------
Write-Step 'Directory permissions'
Grant-DirectoryAccess -Path $LabhostRoot -RunnerAccess 'RX'
foreach ($dir in $runnerDirs) {
    Grant-DirectoryAccess -Path $dir -RunnerAccess 'M'
}

# 4. Hooks and trusted actors ---------------------------------------------------------------
Write-Step 'Runner hook scripts and trusted actors'
$hookSource = Join-Path $PSScriptRoot 'hooks'
foreach ($hook in @('Assert-TrustedJob.ps1', 'Complete-Job.ps1')) {
    Copy-Item -LiteralPath (Join-Path $hookSource $hook) -Destination (Join-Path $LabhostRoot "hooks\$hook") -Force
}
$trustedFile = Join-Path $LabhostRoot 'trusted.txt'
if (-not (Test-Path -LiteralPath $trustedFile)) {
    $content = @('# GitHub logins allowed to trigger jobs on the Lab Host runners (one per line).') + $TrustedActors
    Set-Content -LiteralPath $trustedFile -Value $content -Encoding ASCII
    Write-Host "    created $trustedFile"
}

# 5. OpenSSH Server -------------------------------------------------------------------------
Write-Step 'OpenSSH Server (public-key authentication only)'
$capability = Get-WindowsCapability -Online | Where-Object { $_.Name -like 'OpenSSH.Server*' } | Select-Object -First 1
if ($null -eq $capability) { throw 'OpenSSH Server capability not found' }
if ($capability.State -ne 'Installed') {
    Add-WindowsCapability -Online -Name $capability.Name | Out-Null
}
Set-Service -Name sshd -StartupType Automatic
Start-Service -Name sshd
$sshdConfig = Join-Path $env:ProgramData 'ssh\sshd_config'
[string[]] $original = @(Get-Content -LiteralPath $sshdConfig)
$lines = Merge-SshdOption -Lines $original -Name 'PubkeyAuthentication' -Value 'yes'
$lines = Merge-SshdOption -Lines $lines -Name 'PasswordAuthentication' -Value 'no'
if (($lines -join "`n") -ne ($original -join "`n")) {
    Copy-Item -LiteralPath $sshdConfig -Destination "$sshdConfig.bak" -Force
    Set-Content -LiteralPath $sshdConfig -Value $lines -Encoding ASCII
    $sshd = Join-Path $env:WINDIR 'System32\OpenSSH\sshd.exe'
    & $sshd -t
    if ($LASTEXITCODE -ne 0) {
        Copy-Item -LiteralPath "$sshdConfig.bak" -Destination $sshdConfig -Force
        throw 'sshd_config check failed; previous configuration restored'
    }
    Restart-Service -Name sshd
    Write-Host '    sshd_config updated'
}
if ($AdminPublicKeyFile) {
    $keysFile = Join-Path $env:ProgramData 'ssh\administrators_authorized_keys'
    $key = (Get-Content -LiteralPath $AdminPublicKeyFile -TotalCount 1).Trim()
    $existing = @()
    if (Test-Path -LiteralPath $keysFile) { $existing = @(Get-Content -LiteralPath $keysFile) }
    if ($existing -notcontains $key) {
        Add-Content -LiteralPath $keysFile -Value $key -Encoding ASCII
        Write-Host '    administrator key added'
    }
    Invoke-Native -FilePath icacls.exe -Arguments @(
        $keysFile, '/inheritance:r', '/grant:r', "*${SidAdministrators}:F", '/grant:r', "*${SidSystem}:F"
    ) | Out-Null
}

# 6. Remote desktop with NLA ----------------------------------------------------------------
Write-Step 'Remote desktop with Network Level Authentication'
Set-ItemProperty -Path 'HKLM:\System\CurrentControlSet\Control\Terminal Server' -Name fDenyTSConnections -Value 0
Set-ItemProperty -Path 'HKLM:\System\CurrentControlSet\Control\Terminal Server\WinStations\RDP-Tcp' -Name UserAuthentication -Value 1

# 7. Firewall -------------------------------------------------------------------------------
Write-Step 'Windows Firewall'
Set-NetFirewallProfile -Profile Domain, Private, Public -DefaultInboundAction Block
if ($AllowedRemoteAddress.Count -gt 0) {
    Enable-NetFirewallRule -Group $RemoteDesktopGroup
    Get-NetFirewallRule -Group $RemoteDesktopGroup | Set-NetFirewallRule -RemoteAddress $AllowedRemoteAddress
    $sshRule = Get-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -ErrorAction SilentlyContinue
    if ($null -eq $sshRule) {
        New-NetFirewallRule -Name 'OpenSSH-Server-In-TCP' -DisplayName 'OpenSSH Server (sshd)' -Direction Inbound `
            -Protocol TCP -LocalPort 22 -Action Allow -RemoteAddress $AllowedRemoteAddress | Out-Null
    }
    else {
        $sshRule | Set-NetFirewallRule -RemoteAddress $AllowedRemoteAddress -Enabled True
    }
    Write-Host "    SSH and remote desktop limited to $($AllowedRemoteAddress -join ', ')"
}
else {
    Write-Warning 'SSH and remote desktop rules not restricted: re-run with -AllowedRemoteAddress <workstation address>'
}

# 8. Software -------------------------------------------------------------------------------
if (-not $SkipSoftware) {
    Write-Step 'Git for Windows, PowerShell 7, uv'
    Install-WingetPackage -Command git -PackageId 'Git.Git'
    Install-WingetPackage -Command pwsh -PackageId 'Microsoft.PowerShell'
    Install-Uv -Version $UvVersion -Sha256 $UvSha256 -Target (Join-Path $LabhostRoot 'tools\uv')
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
    if (Get-Command git -ErrorAction SilentlyContinue) {
        Invoke-Native -FilePath git -Arguments @('config', '--system', 'core.autocrlf', 'false')
        Invoke-Native -FilePath git -Arguments @('config', '--system', 'core.longpaths', 'true')
    }
    $pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
    if ($pwsh) {
        New-Item -Path 'HKLM:\SOFTWARE\OpenSSH' -Force | Out-Null
        Set-ItemProperty -Path 'HKLM:\SOFTWARE\OpenSSH' -Name DefaultShell -Value $pwsh.Source
    }
}

# 9. Machine settings -----------------------------------------------------------------------
Write-Step 'Execution policy, long paths, power plan'
Set-ExecutionPolicy -Scope LocalMachine -ExecutionPolicy RemoteSigned -Force
Set-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -Value 1
Invoke-Native -FilePath powercfg.exe -Arguments @('/change', 'standby-timeout-ac', '0')
Invoke-Native -FilePath powercfg.exe -Arguments @('/change', 'hibernate-timeout-ac', '0')
Invoke-Native -FilePath powercfg.exe -Arguments @('/hibernate', 'off')
Invoke-Native -FilePath powercfg.exe -Arguments @('/setacvalueindex', 'SCHEME_CURRENT', $UsbSettingsGroup, $UsbSelectiveSuspend, '0')
Invoke-Native -FilePath powercfg.exe -Arguments @('/setactive', 'SCHEME_CURRENT')

Write-Host ''
Write-Host 'Remaining manual steps (docs/08_process/lab_host.md):'
Write-Host "  - Automatic logon of $RunnerUser (Sysinternals Autologon); lock the screen after logon."
Write-Host '  - BitLocker on the system drive; store the recovery key offline.'
Write-Host '  - BIOS: power on after AC loss.'
Write-Host '  - Windows Update active hours covering the nightly HIL window; pause updates over soak weekends.'
Write-Host '  - IAR EWARM with C-STAT and IAR Visual State, licence activation (day-1 checks D1 to D9).'
Write-Host "  - As ${RunnerUser}: tools\labhost\register-runner.ps1 -Role iar (and -Role hil from M5)."
