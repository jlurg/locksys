# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
# Pester 5 tests of hooks/Complete-Job.ps1 (platform-independent parts: bench supply over
# SCPI against a loopback stub, workspace cleanup boundaries).

BeforeAll {
    # Child processes use the running PowerShell executable (falls back to pwsh on PATH).
    $script:Pwsh = [System.Diagnostics.Process]::GetCurrentProcess().MainModule.FileName
    if ([System.IO.Path]::GetFileNameWithoutExtension($script:Pwsh) -ne 'pwsh') {
        $script:Pwsh = (Get-Command pwsh -CommandType Application | Select-Object -First 1).Source
    }
    $script:Hook = Join-Path (Join-Path (Split-Path -Parent $PSScriptRoot) 'hooks') 'Complete-Job.ps1'
    $script:Variables = @('GITHUB_WORKSPACE', 'RUNNER_TEMP', 'RUNNER_NAME', 'LABHOST_ROOT', 'LABHOST_ROLE', 'LABHOST_PSU_ADDRESS')

    function Invoke-Hook {
        param([hashtable] $Environment, [string[]] $Arguments = @())
        $saved = @{}
        foreach ($name in $script:Variables) {
            $saved[$name] = [Environment]::GetEnvironmentVariable($name)
            [Environment]::SetEnvironmentVariable($name, $Environment[$name])
        }
        try {
            $output = & $script:Pwsh -NoProfile -NonInteractive -File $script:Hook @Arguments 2>&1
            return [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = ($output -join "`n") }
        }
        finally {
            foreach ($name in $script:Variables) {
                [Environment]::SetEnvironmentVariable($name, $saved[$name])
            }
        }
    }

    function Open-ScpiStub {
        <# Accepts one connection, records two command lines, answers with the given reply. #>
        param([string] $Reply)
        $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
        $listener.Start()
        $replyLine = $Reply
        $job = Start-ThreadJob -ScriptBlock {
            $server = $using:listener
            $answer = $using:replyLine
            $client = $server.AcceptTcpClient()
            try {
                $stream = $client.GetStream()
                $reader = [System.IO.StreamReader]::new($stream)
                $writer = [System.IO.StreamWriter]::new($stream)
                $writer.AutoFlush = $true
                $command = $reader.ReadLine()
                $query = $reader.ReadLine()
                $writer.WriteLine($answer)
                "$command|$query"
            }
            finally {
                Start-Sleep -Milliseconds 200
                $client.Close()
                $server.Stop()
            }
        }
        return [pscustomobject]@{ Port = $listener.LocalEndpoint.Port; Job = $job }
    }
}

Describe 'Complete-Job' {
    BeforeEach {
        $script:WorkRoot = Join-Path $TestDrive 'runners/iar/_work'
        $script:Workspace = Join-Path $script:WorkRoot 'locksys/locksys'
        New-Item -ItemType Directory -Force -Path $script:Workspace | Out-Null
        Set-Content -LiteralPath (Join-Path $script:Workspace 'build.log') -Value 'x'
    }

    It 'cleans a workspace inside the work directory of the runner' {
        $result = Invoke-Hook -Environment @{ GITHUB_WORKSPACE = $script:Workspace } -Arguments @('-LabhostRoot', $TestDrive, '-Role', 'iar')
        $result.ExitCode | Should -Be 0
        @(Get-ChildItem -LiteralPath $script:Workspace -Force).Count | Should -Be 0
    }

    It 'leaves a workspace outside the work directory untouched' {
        $outside = Join-Path $TestDrive 'elsewhere'
        New-Item -ItemType Directory -Force -Path $outside | Out-Null
        Set-Content -LiteralPath (Join-Path $outside 'keep.txt') -Value 'x'
        $result = Invoke-Hook -Environment @{ GITHUB_WORKSPACE = $outside } -Arguments @('-LabhostRoot', $TestDrive, '-Role', 'iar')
        $result.ExitCode | Should -Be 0
        $result.Output | Should -Match 'not cleaned'
        Test-Path -LiteralPath (Join-Path $outside 'keep.txt') | Should -BeTrue
    }

    It 'resets and cleans a git workspace' {
        & git -C $script:Workspace init --quiet
        & git -C $script:Workspace -c user.name=test -c user.email=test@example.invalid add build.log
        & git -C $script:Workspace -c user.name=test -c user.email=test@example.invalid commit --quiet -m 'test: fixture'
        Set-Content -LiteralPath (Join-Path $script:Workspace 'build.log') -Value 'modified'
        Set-Content -LiteralPath (Join-Path $script:Workspace 'untracked.o') -Value 'x'
        $result = Invoke-Hook -Environment @{ GITHUB_WORKSPACE = $script:Workspace } -Arguments @('-LabhostRoot', $TestDrive, '-Role', 'iar')
        $result.ExitCode | Should -Be 0
        Test-Path -LiteralPath (Join-Path $script:Workspace 'untracked.o') | Should -BeFalse
        Get-Content -LiteralPath (Join-Path $script:Workspace 'build.log') | Should -Be 'x'
    }

    It 'switches the bench supply off on the hil runner' {
        $stub = Open-ScpiStub -Reply '0'
        $arguments = @('-LabhostRoot', $TestDrive, '-Role', 'hil', '-PsuAddress', "127.0.0.1:$($stub.Port)")
        $result = Invoke-Hook -Environment @{} -Arguments $arguments
        $received = Receive-Job -Job $stub.Job -Wait -AutoRemoveJob
        $result.ExitCode | Should -Be 0
        $result.Output | Should -Match 'output is off'
        $received | Should -Be 'OUTP OFF|OUTP?'
    }

    It 'fails when the supply still reports its output on' {
        $stub = Open-ScpiStub -Reply '1'
        $arguments = @('-LabhostRoot', $TestDrive, '-Role', 'hil', '-PsuAddress', "127.0.0.1:$($stub.Port)")
        $result = Invoke-Hook -Environment @{} -Arguments $arguments
        Receive-Job -Job $stub.Job -Wait -AutoRemoveJob | Out-Null
        $result.ExitCode | Should -Be 1
    }

    It 'fails within the timeout when the supply is unreachable' {
        $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
        $listener.Start()
        $port = $listener.LocalEndpoint.Port
        $listener.Stop()
        $arguments = @('-LabhostRoot', $TestDrive, '-Role', 'hil', '-PsuAddress', "127.0.0.1:$port", '-TimeoutMilliseconds', '1000')
        $elapsed = Measure-Command { $script:Result = Invoke-Hook -Environment @{} -Arguments $arguments }
        $script:Result.ExitCode | Should -Be 1
        $elapsed.TotalSeconds | Should -BeLessThan 15
    }

    It 'only warns on the hil runner when no supply is configured' {
        $result = Invoke-Hook -Environment @{} -Arguments @('-LabhostRoot', $TestDrive, '-Role', 'hil')
        $result.ExitCode | Should -Be 0
        $result.Output | Should -Match 'not configured'
    }
}
