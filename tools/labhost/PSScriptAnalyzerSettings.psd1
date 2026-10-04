# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
# PSScriptAnalyzer settings for tools/labhost (tests/Invoke-LabhostChecks.ps1).
@{
    Severity     = @('Error', 'Warning')
    ExcludeRules = @(
        # Setup scripts are interactive administration tools that report progress to the
        # console; hook output goes to the job log through Write-Output.
        'PSAvoidUsingWriteHost'
    )
    Rules        = @{
        # setup-labhost.ps1 runs before PowerShell 7 is installed; the hooks may fall back
        # to Windows PowerShell when pwsh is unavailable.
        PSUseCompatibleSyntax = @{
            Enable         = $true
            TargetVersions = @('5.1', '7.4')
        }
    }
}
