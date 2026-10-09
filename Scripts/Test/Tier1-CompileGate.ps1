#Requires -Version 5.1
<#
.SYNOPSIS
    Tier 1 compile gate — fast, unattended UBT build for agent self-verification.
.DESCRIPTION
    Runs the configured UBT runner (bundled direct UBT or Build.bat) for the selected target.
    Relative project paths are resolved against BwayTestConfig.psd1's ProjectRoot before UBT
    starts. Exit code 0 means compile OK only when failureSignals is empty.
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
    [string]$OutputJson,
    [switch]$ValidateOnly,
    [switch]$NoMutexWait,
    [switch]$DirectUbt
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
$projectFileInput = $ProjectFile
$projectFileWasRelative = -not [System.IO.Path]::IsPathRooted($ProjectFile)
if ($projectFileWasRelative) {
    # UBT may run with the engine directory as its working directory. Resolve
    # project-relative inputs against the configured tree before admission so
    # a relative path cannot be interpreted against the wrong working folder.
    $ProjectFile = Join-Path $Config.ProjectRoot $ProjectFile
}
try {
    $ProjectFile = [System.IO.Path]::GetFullPath($ProjectFile)
} catch {
    throw "Project file path is invalid: $ProjectFile ($($_.Exception.Message))"
}
$sourceHead = (& git -C $Config.ProjectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve the current source revision.' }
$dirtyFileCount = @(& git -C $Config.ProjectRoot status --short).Count
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the current worktree.' }

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
$logDirectory = Split-Path -Parent $LogFile
if ($logDirectory -and -not (Test-Path -LiteralPath $logDirectory)) {
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
}
$LogFile = [System.IO.Path]::GetFullPath($LogFile)

$outputPath = $null
if ($OutputJson) {
    $outputPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $Config.ProjectRoot $OutputJson }
    $outputDirectory = Split-Path -Parent $outputPath
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    $outputPath = [System.IO.Path]::GetFullPath($outputPath)
}

function Write-AtomicJson {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)]$Object
    )
    $temporaryPath = "$Path.$([guid]::NewGuid().ToString('N')).partial"
    try {
        $Object | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $temporaryPath -Encoding UTF8
        Move-Item -LiteralPath $temporaryPath -Destination $Path -Force
    } finally {
        if (Test-Path -LiteralPath $temporaryPath -PathType Leaf) {
            Remove-Item -LiteralPath $temporaryPath -Force -ErrorAction SilentlyContinue
        }
    }
}

$useDirectUbt = $DirectUbt -or ($Config.CompileMode -eq 'DirectBundledUbt')
$compileUbaRoot = if ($Config.CompileUbaRoot) { $Config.CompileUbaRoot } else { Join-Path $Config.ProjectRoot 'Saved\Logs\compile\UBARoot' }
$buildFile = $Engine.BuildBat
$buildArgs = @()
if ($useDirectUbt) {
    $dotnet = Join-Path $Engine.EngineRoot 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
    $ubtDll = Join-Path $Engine.EngineRoot 'Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
    if (-not (Test-Path -LiteralPath $dotnet -PathType Leaf)) {
        throw "Bundled .NET runtime not found: $dotnet"
    }
    if (-not (Test-Path -LiteralPath $ubtDll -PathType Leaf)) {
        throw "UnrealBuildTool DLL not found: $ubtDll"
    }
    if (-not (Test-Path -LiteralPath $compileUbaRoot -PathType Container)) {
        New-Item -ItemType Directory -Path $compileUbaRoot -Force | Out-Null
    }
    $compileProbe = Join-Path $compileUbaRoot ('.codex-uba-write-probe-' + [guid]::NewGuid().ToString('N') + '.tmp')
    try {
        [System.IO.File]::WriteAllText($compileProbe, 'ProjectB UBA preflight')
    } catch {
        throw "Compile UBA root is not writable: $compileUbaRoot ($($_.Exception.Message))"
    } finally {
        if (Test-Path -LiteralPath $compileProbe -PathType Leaf) {
            Remove-Item -LiteralPath $compileProbe -Force -ErrorAction SilentlyContinue
        }
    }
    $buildFile = $dotnet
    $buildArgs = @(
        $ubtDll
        $Target
        $Platform
        $Configuration
        "-Project=`"$ProjectFile`""
        "-UBARootDir=`"$compileUbaRoot`""
        '-UBADisableRemote'
        "-Log=`"$LogFile`""
    )
} else {
    $buildArgs = @(
        $Target
        $Platform
        $Configuration
        "-Project=`"$ProjectFile`""
    )
}
if (-not $NoMutexWait) {
    $buildArgs += '-WaitMutex'
}

Write-Host "[Tier1] Compiling $Target $Platform $Configuration ..."
Write-Host "[Tier1] Log: $LogFile"
Write-Host "[Tier1] Runner: $(if ($useDirectUbt) { 'bundled dotnet + direct UBT' } else { 'Build.bat' })"

$stdoutFile = if ($useDirectUbt) { "${LogFile}.stdout" } else { $LogFile }
$stderrFile = "${LogFile}.err"
$startedUtc = [DateTime]::UtcNow
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$oldBuildMachine = $env:IsBuildMachine
if ($useDirectUbt) { $env:IsBuildMachine = '1' }
try {
    $startParams = @{
        FilePath = $buildFile
        ArgumentList = $buildArgs
        NoNewWindow = $true
        PassThru = $true
        RedirectStandardOutput = $stdoutFile
        RedirectStandardError = $stderrFile
    }
    $buildProcess = Start-Process @startParams
} finally {
    if ($useDirectUbt) {
        if ($null -eq $oldBuildMachine) { Remove-Item Env:IsBuildMachine -ErrorAction SilentlyContinue }
        else { $env:IsBuildMachine = $oldBuildMachine }
    }
}

if ($outputPath) {
    $runningRecord = [ordered]@{
        generatedUtc = [DateTime]::UtcNow.ToString('o')
        startedUtc = $startedUtc.ToString('o')
        sourceHead = $sourceHead
        dirtyFileCount = $dirtyFileCount
        target = $Target
        platform = $Platform
        configuration = $Configuration
        projectFile = $ProjectFile
        projectFileInput = $projectFileInput
        projectFileWasRelative = $projectFileWasRelative
        runner = if ($useDirectUbt) { 'DirectBundledUbt' } else { 'BuildBat' }
        log = $LogFile
        stdout = if ($useDirectUbt) { $stdoutFile } else { $null }
        stderr = $stderrFile
        processId = $buildProcess.Id
        status = 'running'
        scope = 'Compile inventory only; build is still running. Wait for the owned process before interpreting the final record.'
        outputJson = $outputPath
    }
    Write-AtomicJson -Path $outputPath -Object $runningRecord
}

# Wait on this build process rather than Start-Process -Wait's process-tree job.
# Under an agent host, the tree wait can remain blocked after UBT has exited.
$buildProcess.WaitForExit()
$buildProcess.Refresh()
$stopwatch.Stop()

if (Test-Path -LiteralPath $stderrFile) {
    $stderr = Get-Content -LiteralPath $stderrFile -Raw -ErrorAction SilentlyContinue
    if ($stderr) {
        Add-Content -LiteralPath $LogFile -Value "`n--- STDERR ---`n$stderr"
    }
}

$processExitCode = [int]$buildProcess.ExitCode
$failureSignals = @()
if ($useDirectUbt) {
    $failureSources = @($LogFile, $stdoutFile, $stderrFile) |
        Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
        Select-Object -Unique
} else {
    $failureSources = @($LogFile) |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
}
if ($failureSources.Count -gt 0) {
    $failurePatterns = @(
        'Result:\s+Failed',
        'CompilationResultException',
        '\berror\s+C\d{4}\b',
        '\bfatal\s+error\b',
        'Unhandled exception:',
        'UnauthorizedAccessException',
        'Access to the path .* denied',
        'Unable to rename .*\.rsp(?:\.old)?'
    )
    foreach ($source in $failureSources) {
        foreach ($pattern in $failurePatterns) {
            $failureSignals += @(Select-String -LiteralPath $source -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue |
                Select-Object -First 4 |
                ForEach-Object { "[$([System.IO.Path]::GetFileName($source))] $($_.Line.Trim())" })
        }
    }
    $failureSignals = @($failureSignals | Where-Object { $_ } | Select-Object -Unique | Select-Object -First 8)
}
$exitCode = if ($processExitCode -ne 0) { $processExitCode } elseif ($failureSignals.Count -gt 0) { 1 } else { 0 }
$compileRecord = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    startedUtc = $startedUtc.ToString('o')
    sourceHead = $sourceHead
    dirtyFileCount = $dirtyFileCount
    target = $Target
    platform = $Platform
    configuration = $Configuration
    projectFile = $ProjectFile
    projectFileInput = $projectFileInput
    projectFileWasRelative = $projectFileWasRelative
    runner = if ($useDirectUbt) { 'DirectBundledUbt' } else { 'BuildBat' }
    log = $LogFile
    stdout = if ($useDirectUbt) { $stdoutFile } else { $null }
    stderr = $stderrFile
    processId = $buildProcess.Id
    processExitCode = $processExitCode
    exitCode = $exitCode
    elapsedSeconds = [math]::Round($stopwatch.Elapsed.TotalSeconds, 2)
    status = if ($exitCode -eq 0) { 'passed' } else { 'failed' }
    failureSignals = @($failureSignals)
    scope = 'Compile inventory only; a successful build does not establish runtime or multiplayer acceptance.'
}
if ($OutputJson) {
    $compileRecord.outputJson = $outputPath
    Write-AtomicJson -Path $outputPath -Object $compileRecord
}

if ($exitCode -eq 0) {
    Write-Host "[Tier1] PASS — compile succeeded (exit 0)"
    exit 0
}

$logTail = @(Get-Content -LiteralPath $LogFile -Tail 60 -ErrorAction SilentlyContinue)
Write-Host "[Tier1] FAIL — compile failed (exit $exitCode)"
if ($logTail -match 'Live Coding is active') {
    Write-Host "[Tier1] HINT — Close Unreal Editor (or press Ctrl+Alt+F11 to disable Live Coding), then re-run."
}
Write-Host "[Tier1] Last 40 log lines:"
$logTail | Select-Object -Last 40 | ForEach-Object { Write-Host "  $_" }
exit $exitCode
