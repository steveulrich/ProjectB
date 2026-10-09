[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Candidate,
    [string]$OutputJson,
    [switch]$RequireCandidate,
    [string]$CompiledTargetRecord,
    [switch]$RequireAutomationToolLogWrite,
    [ValidateRange(0.01, 1048576)][double]$MinimumFreeSpaceGiB = 10
)
$ErrorActionPreference = 'Stop'
$config = Import-PowerShellDataFile (Join-Path $PSScriptRoot 'BwayTestConfig.psd1')
. (Join-Path $PSScriptRoot 'Resolve-UnrealEngine.ps1')
$engine = Resolve-UnrealEngine -EngineAssociation $config.EngineAssociation -DefaultEngineRoot $config.DefaultEngineRoot
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$candidatePath = if ($Candidate) { Join-Path $root "Saved/StagedBuilds/$Candidate" } else { $null }
$executable = if ($candidatePath) { Join-Path $candidatePath "Windows/ProjectB/Binaries/Win64/$($config.GameBinary)" } else { $null }
$head = & git -C $root rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve source revision.' }
$dirty = @(& git -C $root status --short)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect source changes.' }
$checks = [ordered]@{
    projectFile = Test-Path -LiteralPath (Join-Path $root 'ProjectB.uproject') -PathType Leaf
    engineEditor = Test-Path -LiteralPath (Join-Path $engine.EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -PathType Leaf
    buildTool = Test-Path -LiteralPath $engine.BuildBat -PathType Leaf
}
# Current stages are about 4 GiB. Reserve room for a fresh stage, packaging
# intermediates, and logs; this guard is not a guarantee for larger future cooks.
# Testing a retained candidate does not allocate another package.
$storage = [ordered]@{
    checked = -not [bool]$RequireCandidate
    volume = [System.IO.Path]::GetPathRoot($root)
    availableBytes = $null
    requiredBytes = [long][math]::Ceiling($MinimumFreeSpaceGiB * 1GB)
    minimumFreeSpaceGiB = $MinimumFreeSpaceGiB
    error = $null
}
if ($storage.checked) {
    try {
        $storage.availableBytes = ([System.IO.DriveInfo]::new($storage.volume)).AvailableFreeSpace
        $checks.packageFreeSpace = $storage.availableBytes -ge $storage.requiredBytes
    } catch {
        $storage.error = $_.Exception.Message
        $checks.packageFreeSpace = $false
    }
}
$automationToolLogWrite = $null
if ($RequireAutomationToolLogWrite) {
    $automationToolLogDirectory = Join-Path $engine.EngineRoot 'Engine/Programs/AutomationTool/Saved/Logs'
    $automationToolErrorLog = Join-Path $automationToolLogDirectory 'ErrorLog.txt'
    $automationToolLogWrite = [ordered]@{
        directory = $automationToolLogDirectory
        path = $automationToolErrorLog
        existedBefore = Test-Path -LiteralPath $automationToolErrorLog -PathType Leaf
        writable = $false
        error = $null
    }
    $stream = $null
    try {
        if (-not (Test-Path -LiteralPath $automationToolLogDirectory -PathType Container)) {
            throw "AutomationTool log directory does not exist: $automationToolLogDirectory"
        }
        $stream = [System.IO.File]::Open($automationToolErrorLog, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::ReadWrite)
        $automationToolLogWrite.writable = $true
    } catch {
        $automationToolLogWrite.error = $_.Exception.Message
    } finally {
        if ($null -ne $stream) { $stream.Dispose() }
        if (-not $automationToolLogWrite.existedBefore -and (Test-Path -LiteralPath $automationToolErrorLog -PathType Leaf)) {
            Remove-Item -LiteralPath $automationToolErrorLog -Force -ErrorAction SilentlyContinue
        }
    }
    $checks.automationToolLogWrite = $automationToolLogWrite.writable
}
$artifactExists = [bool]($executable -and (Test-Path -LiteralPath $executable -PathType Leaf))
if ($RequireCandidate) { $checks.candidateExecutable = $artifactExists }
$compiledTarget = $null
if ($CompiledTargetRecord) {
    $compiledRecordPath = if ([System.IO.Path]::IsPathRooted($CompiledTargetRecord)) {
        $CompiledTargetRecord
    } else {
        Join-Path $root $CompiledTargetRecord
    }
    $compiledTarget = [ordered]@{
        record = $compiledRecordPath
        exists = Test-Path -LiteralPath $compiledRecordPath -PathType Leaf
        status = $null
        target = $null
        sourceHead = $null
        generatedUtc = $null
        processExitCode = $null
        failureSignals = @()
        failureSignalsPresent = $false
        binary = Join-Path $root "Binaries/Win64/$($config.GameBinary)"
        binaryExists = $false
        binaryLastWriteUtc = $null
        currentHeadMatch = $false
        currentTargetMatch = $false
        passing = $false
    }
    if ($compiledTarget.exists) {
        try {
            $record = Get-Content -LiteralPath $compiledRecordPath -Raw | ConvertFrom-Json
            $compiledTarget.status = $record.status
            $compiledTarget.target = $record.target
            $compiledTarget.sourceHead = $record.sourceHead
            $compiledTarget.generatedUtc = $record.generatedUtc
            $compiledTarget.processExitCode = $record.processExitCode
            $failureSignalsProperty = $record.PSObject.Properties['failureSignals']
            if ($null -ne $failureSignalsProperty) {
                $compiledTarget.failureSignalsPresent = $true
                $compiledTarget.failureSignals = @($record.failureSignals | ForEach-Object { [string]$_ } | Where-Object { $_ })
            }
            $compiledTarget.currentHeadMatch = ($record.sourceHead -eq $head.Trim())
            $compiledTarget.currentTargetMatch = ($record.target -eq $config.GameTarget)
            if (Test-Path -LiteralPath $compiledTarget.binary -PathType Leaf) {
                $binaryItem = Get-Item -LiteralPath $compiledTarget.binary
                $compiledTarget.binaryExists = $true
                $compiledTarget.binaryLastWriteUtc = $binaryItem.LastWriteTimeUtc.ToString('o')
            }
            $compiledTarget.passing = ($compiledTarget.status -eq 'passed' -and
                $compiledTarget.failureSignalsPresent -and
                $compiledTarget.failureSignals.Count -eq 0 -and
                $compiledTarget.currentHeadMatch -and
                $compiledTarget.currentTargetMatch -and
                $compiledTarget.binaryExists)
        } catch {
            $compiledTarget.parseError = $_.Exception.Message
        }
    }
    $checks.compiledTarget = $compiledTarget.passing
}
$ready = $checks.Values -notcontains $false
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    currentWorktreeHead = $head.Trim()
    engineRoot = $engine.EngineRoot
    candidate = $Candidate
    candidatePath = $candidatePath
    artifactExists = $artifactExists
    compiledTarget = $compiledTarget
    automationToolLogWrite = $automationToolLogWrite
    storage = $storage
    dirtyFileCount = $dirty.Count
    dirtyFiles = @($dirty | Select-Object -First 20)
    checks = $checks
    ready = $ready
    nextAction = if (!$ready) { 'Resolve failed checks.' } elseif ($RequireCandidate) { 'Run candidate tests; match evidence to its build manifest.' } else { 'Inspect active build/editor processes and unsaved assets before starting a build.' }
}
$json = $result | ConvertTo-Json -Depth 6
if ($OutputJson) { $json | Set-Content -LiteralPath $OutputJson -Encoding UTF8 }
$json
if (!$ready) { exit 1 }
exit 0
