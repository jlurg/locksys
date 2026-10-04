# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
# Pester 5 tests of hooks/Assert-TrustedJob.ps1. The hook runs in a child pwsh process,
# as on the runner, with simulated GITHUB_* variables.

BeforeAll {
    # Child processes use the running PowerShell executable (falls back to pwsh on PATH).
    $script:Pwsh = [System.Diagnostics.Process]::GetCurrentProcess().MainModule.FileName
    if ([System.IO.Path]::GetFileNameWithoutExtension($script:Pwsh) -ne 'pwsh') {
        $script:Pwsh = (Get-Command pwsh -CommandType Application | Select-Object -First 1).Source
    }
    $script:Hook = Join-Path (Join-Path (Split-Path -Parent $PSScriptRoot) 'hooks') 'Assert-TrustedJob.ps1'
    $script:Variables = @('GITHUB_REPOSITORY', 'GITHUB_EVENT_NAME', 'GITHUB_TRIGGERING_ACTOR', 'RUNNER_NAME', 'LABHOST_ROOT', 'LABHOST_ROLE')

    function Invoke-Hook {
        param(
            [hashtable] $Environment,
            [string[]] $Arguments = @(),
            [switch] $DotSource
        )
        $saved = @{}
        foreach ($name in $script:Variables) {
            $saved[$name] = [Environment]::GetEnvironmentVariable($name)
            [Environment]::SetEnvironmentVariable($name, $Environment[$name])
        }
        try {
            if ($DotSource) {
                $output = & $script:Pwsh -NoProfile -NonInteractive -Command ". '$script:Hook'" 2>&1
            }
            else {
                $output = & $script:Pwsh -NoProfile -NonInteractive -File $script:Hook @Arguments 2>&1
            }
            return [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = ($output -join "`n") }
        }
        finally {
            foreach ($name in $script:Variables) {
                [Environment]::SetEnvironmentVariable($name, $saved[$name])
            }
        }
    }

    function Get-TrustedEnvironment {
        param([string] $EventName = 'push', [string] $Actor = 'jlurg')
        return @{
            GITHUB_REPOSITORY       = 'jlurg/locksys'
            GITHUB_EVENT_NAME       = $EventName
            GITHUB_TRIGGERING_ACTOR = $Actor
        }
    }
}

Describe 'Assert-TrustedJob' {
    BeforeEach {
        Set-Content -LiteralPath (Join-Path $TestDrive 'trusted.txt') -Value @('# trusted actors', 'jlurg', '')
        Remove-Item -LiteralPath (Join-Path $TestDrive 'bench.lock') -ErrorAction SilentlyContinue
        $script:HookArguments = @('-LabhostRoot', $TestDrive, '-Role', 'iar')
    }

    It 'admits <Name> triggered by a trusted actor' -ForEach @(
        @{ Name = 'push' }, @{ Name = 'schedule' }, @{ Name = 'workflow_dispatch' }
    ) {
        $result = Invoke-Hook -Environment (Get-TrustedEnvironment -EventName $Name) -Arguments $script:HookArguments
        $result.ExitCode | Should -Be 0
        $result.Output | Should -Match 'admitted'
    }

    It 'refuses event <Name>' -ForEach @(
        @{ Name = 'pull_request' }, @{ Name = 'pull_request_target' }, @{ Name = 'workflow_run' },
        @{ Name = 'issue_comment' }, @{ Name = 'merge_group' }, @{ Name = '' }
    ) {
        $result = Invoke-Hook -Environment (Get-TrustedEnvironment -EventName $Name) -Arguments $script:HookArguments
        $result.ExitCode | Should -Be 1
        $result.Output | Should -Match '::error'
    }

    It 'refuses an untrusted or missing actor' -ForEach @(@{ Actor = 'dependabot[bot]' }, @{ Actor = '' }) {
        $result = Invoke-Hook -Environment (Get-TrustedEnvironment -Actor $Actor) -Arguments $script:HookArguments
        $result.ExitCode | Should -Be 1
    }

    It 'compares the actor case-insensitively' {
        $result = Invoke-Hook -Environment (Get-TrustedEnvironment -Actor 'JLURG') -Arguments $script:HookArguments
        $result.ExitCode | Should -Be 0
    }

    It 'refuses another repository' {
        $environment = Get-TrustedEnvironment
        $environment.GITHUB_REPOSITORY = 'someone/locksys'
        (Invoke-Hook -Environment $environment -Arguments $script:HookArguments).ExitCode | Should -Be 1
    }

    It 'refuses when trusted.txt is missing or empty' {
        Set-Content -LiteralPath (Join-Path $TestDrive 'trusted.txt') -Value @('# no actor')
        (Invoke-Hook -Environment (Get-TrustedEnvironment) -Arguments $script:HookArguments).ExitCode | Should -Be 1
        Remove-Item -LiteralPath (Join-Path $TestDrive 'trusted.txt')
        (Invoke-Hook -Environment (Get-TrustedEnvironment) -Arguments $script:HookArguments).ExitCode | Should -Be 1
    }

    It 'refuses hil jobs while the bench is reserved and keeps admitting iar jobs' {
        Set-Content -LiteralPath (Join-Path $TestDrive 'bench.lock') -Value 'reserved'
        $hil = Invoke-Hook -Environment (Get-TrustedEnvironment) -Arguments @('-LabhostRoot', $TestDrive, '-Role', 'hil')
        $hil.ExitCode | Should -Be 1
        $hil.Output | Should -Match 'bench'
        (Invoke-Hook -Environment (Get-TrustedEnvironment) -Arguments $script:HookArguments).ExitCode | Should -Be 0
    }

    It 'refuses an unknown runner role' {
        $result = Invoke-Hook -Environment (Get-TrustedEnvironment) -Arguments @('-LabhostRoot', $TestDrive)
        $result.ExitCode | Should -Be 1
        $result.Output | Should -Match 'unknown runner role'
    }

    It 'derives the role from the runner name' {
        $environment = Get-TrustedEnvironment
        $environment.RUNNER_NAME = 'labhost-hil'
        (Invoke-Hook -Environment $environment -Arguments @('-LabhostRoot', $TestDrive)).ExitCode | Should -Be 0
    }

    It 'sets the exit code when dot-sourced as the runner does' {
        $environment = Get-TrustedEnvironment
        $environment.LABHOST_ROOT = $TestDrive
        $environment.LABHOST_ROLE = 'iar'
        (Invoke-Hook -Environment $environment -DotSource).ExitCode | Should -Be 0
        $environment.GITHUB_EVENT_NAME = 'pull_request'
        (Invoke-Hook -Environment $environment -DotSource).ExitCode | Should -Be 1
    }
}
