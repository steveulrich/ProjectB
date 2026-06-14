#Requires -Version 5.1
<#
.SYNOPSIS
    Tier 1 compile gate — fast, unattended UBT build for agent self-verification.
.DESCRIPTION
    Runs UBT via Build.bat for the LyraEditor target. Exit code 0 means compile OK.
    Agents should run this after any C++ change before claiming a task is complete.

.EXAMPLE
    .\Tier1-CompileGate.ps1

.EXAMPLE
    .\Tier1-CompileGate.ps1 -ValidateOnly
#>
[CmdletBinding()]
param(
    [string]$Target,
    [string]$Platform,
    [string]$Configuration,
    [string]$ProjectFile,
    [string]$LogFile,
    [switch]$ValidateOnly,
    [switch]$NoMutexWait
)

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ConfigPath = Join-Path $ScriptDir 'BwayTestConfig.psd1'
if (-not (Test-Path -LiteralPath $ConfigPath)) {
    throw "Missing test config: $ConfigPath"
}
$Config = Import-PowerShellDataFile -Path $ConfigPath

$Target        = if ($Target)        { $Target }        else { $Config.EditorTarget }
$Platform      = if ($Platform)      { $Platform }      else { $Config.Platform }
$Configuration = if ($Configuration) { $Configuration } else { $Config.Configuration }
$ProjectFile   = if ($ProjectFile)   { $ProjectFile }   else { $Config.ProjectFile }

if (-not (Test-Path -LiteralPath $ProjectFile)) {
    throw "Project file not found: $ProjectFile"
}

. (Join-Path $ScriptDir 'Resolve-UnrealEngine.ps1')
$Engine = Resolve-UnrealEngine -EngineAssociation $Config.EngineAssociation -DefaultEngineRoot $Config.DefaultEngineRoot

if ($ValidateOnly) {
    Write-Host "[Tier1] ValidateOnly — paths OK"
    Write-Host "  EngineRoot : $($Engine.EngineRoot)"
    Write-Host "  Build.bat  : $($Engine.BuildBat)"
    Write-Host "  Target     : $Target $Platform $Configuration"
    Write-Host "  Project    : $ProjectFile"
    exit 0
}

if (-not $LogFile) {
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $LogFile = Join-Path $env:TEMP "bway_tier1_compile_$stamp.log"
}

$buildArgs = @(
    $Target
    $Platform
    $Configuration
    "-Project=`"$ProjectFile`""
)
if (-not $NoMutexWait) {
    $buildArgs += '-WaitMutex'
}

Write-Host "[Tier1] Compiling $Target $Platform $Configuration ..."
Write-Host "[Tier1] Log: $LogFile"

$buildProcess = Start-Process `
    -FilePath $Engine.BuildBat `
    -ArgumentList $buildArgs `
    -NoNewWindow `
    -Wait `
    -PassThru `
    -RedirectStandardOutput $LogFile `
    -RedirectStandardError "${LogFile}.err"

$stderrFile = "${LogFile}.err"
if (Test-Path -LiteralPath $stderrFile) {
    $stderr = Get-Content -LiteralPath $stderrFile -Raw -ErrorAction SilentlyContinue
    if ($stderr) {
        Add-Content -LiteralPath $LogFile -Value "`n--- STDERR ---`n$stderr"
    }
}

if ($buildProcess.ExitCode -eq 0) {
    Write-Host "[Tier1] PASS — compile succeeded (exit 0)"
    exit 0
}

$logTail = @(Get-Content -LiteralPath $LogFile -Tail 60 -ErrorAction SilentlyContinue)
Write-Host "[Tier1] FAIL — compile failed (exit $($buildProcess.ExitCode))"
if ($logTail -match 'Live Coding is active') {
    Write-Host "[Tier1] HINT — Close Unreal Editor (or press Ctrl+Alt+F11 to disable Live Coding), then re-run."
}
Write-Host "[Tier1] Last 40 log lines:"
$logTail | Select-Object -Last 40 | ForEach-Object { Write-Host "  $_" }
exit $buildProcess.ExitCode
