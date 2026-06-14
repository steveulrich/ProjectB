#Requires -Version 5.1
<#
.SYNOPSIS
    Dispatcher for ProjectB agentic test tiers.
.EXAMPLE
    .\Run-Tier.ps1 -Tier 1
    .\Run-Tier.ps1 -Tier 1 -ValidateOnly
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('1', '2', '3')]
    [string]$Tier,

    [switch]$ValidateOnly,
    [string[]]$ExtraArgs
)

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

switch ($Tier) {
    '1' {
        $tier1Params = @{}
        if ($ValidateOnly) { $tier1Params['ValidateOnly'] = $true }
        & (Join-Path $ScriptDir 'Tier1-CompileGate.ps1') @tier1Params @ExtraArgs
        exit $LASTEXITCODE
    }
    '2' {
        $tier2 = Join-Path $ScriptDir 'Tier2-StandaloneSmoke.ps1'
        if (-not (Test-Path -LiteralPath $tier2)) {
            Write-Error "Tier 2 script not implemented yet. See Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md"
            exit 2
        }
        & $tier2 @ExtraArgs
        exit $LASTEXITCODE
    }
    '3' {
        $tier3 = Join-Path $ScriptDir 'Tier3-CQTest.ps1'
        if (-not (Test-Path -LiteralPath $tier3)) {
            Write-Error "Tier 3 script not implemented yet. See Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md"
            exit 2
        }
        & $tier3 @ExtraArgs
        exit $LASTEXITCODE
    }
}
