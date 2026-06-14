#Requires -Version 5.1
<#
.SYNOPSIS
    Resolves the Unreal Engine root directory for ProjectB agentic tests.
#>
function Resolve-UnrealEngine {
    [CmdletBinding()]
    param(
        [string]$EngineAssociation = '{A9367F94-430F-554F-8376-B6843DA48377}',
        [string]$DefaultEngineRoot
    )

    function Test-EngineRoot {
        param([string]$Root)
        if ([string]::IsNullOrWhiteSpace($Root)) { return $false }
        $BuildBat = Join-Path $Root 'Engine\Build\BatchFiles\Build.bat'
        return (Test-Path -LiteralPath $BuildBat)
    }

    function Normalize-EngineRoot {
        param([string]$Root)
        if ([string]::IsNullOrWhiteSpace($Root)) { return $null }
        $Root = $Root.Trim().TrimEnd('\', '/')
        if ($Root -match '\\Engine$|/Engine$') {
            $Root = Split-Path -Parent $Root
        }
        return $Root
    }

    $candidates = @()

    foreach ($varName in @('UE_ENGINE_ROOT', 'UNREAL_ENGINE_ROOT')) {
        $value = [Environment]::GetEnvironmentVariable($varName)
        if ($value) { $candidates += Normalize-EngineRoot $value }
    }

    $registryPath = "HKCU:\Software\Epic Games\Unreal Engine\Builds"
    if (Test-Path -LiteralPath $registryPath) {
        try {
            $props = Get-ItemProperty -LiteralPath $registryPath -ErrorAction Stop
            if ($props.PSObject.Properties.Name -contains $EngineAssociation) {
                $candidates += Normalize-EngineRoot $props.$EngineAssociation
            }
        }
        catch {
            Write-Verbose "Registry lookup failed: $($_.Exception.Message)"
        }
    }

    if ($DefaultEngineRoot) {
        $candidates += Normalize-EngineRoot $DefaultEngineRoot
    }

    $epicRoot = Join-Path ${env:ProgramFiles} 'Epic Games'
    if (Test-Path -LiteralPath $epicRoot) {
        Get-ChildItem -LiteralPath $epicRoot -Directory -Filter 'UE_*' -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object { $candidates += $_.FullName }
    }

    foreach ($candidate in $candidates | Select-Object -Unique) {
        if (Test-EngineRoot -Root $candidate) {
            return [PSCustomObject]@{
                EngineRoot = $candidate
                BuildBat   = Join-Path $candidate 'Engine\Build\BatchFiles\Build.bat'
                EditorCmd  = Join-Path $candidate 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
                UATBat     = Join-Path $candidate 'Engine\Build\BatchFiles\RunUAT.bat'
            }
        }
    }

    throw "Could not resolve Unreal Engine root. Set UE_ENGINE_ROOT to your engine folder (parent of Engine/), or verify EngineAssociation '$EngineAssociation' in the Epic Launcher registry."
}
