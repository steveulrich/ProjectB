[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Gate,
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [Alias('JsonFile')][string[]]$EvidenceFile = @(),
    [Alias('ContractPath')][string]$ContractFile,
    [ValidateSet('compile', 'runtime', 'package', 'all')][string]$ContractPhase,
    [string[]]$LogFile = @(),
    [ValidateRange(0, 6)][int]$CriticalExampleCount = 2,
    [string]$OutputJson,
    [switch]$SummaryOnly
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$evidencePath = if ([System.IO.Path]::IsPathRooted($EvidenceDirectory)) {
    (Get-Item -LiteralPath $EvidenceDirectory -ErrorAction Stop).FullName
} else {
    (Get-Item -LiteralPath (Join-Path $root $EvidenceDirectory) -ErrorAction Stop).FullName
}

function Get-CompactJsonSummary {
    param([Parameter(Mandatory)]$Object)
    $keys = @(
        'scope', 'status', 'result', 'pass', 'success', 'phase', 'samples',
        'active_samples', 'request_active_samples', 'visibility_mismatches',
        'request_visibility_mismatch_samples', 'team_mismatch_samples',
        'world_count', 'peer_player_counts', 'errors', 'warnings',
        'errorLines', 'knownNonFatalErrorLines', 'unknownErrorLines', 'firstUnknownError',
        'firstCausalError', 'firstKnownNonFatalError', 'missingMarkers',
        'criticalLines', 'normalExits', 'events', 'nextAction',
        'sourceHeadObserved', 'proofGaps'
    )
    $summary = [ordered]@{}
    foreach ($key in $keys) {
        $property = $Object.PSObject.Properties[$key]
        if ($null -eq $property) { continue }
        $value = $property.Value
        if ($value -is [System.Array]) {
            $summary[$key] = [ordered]@{
                count = @($value).Count
                first = @($value | Select-Object -First 1)
            }
        } else { $summary[$key] = $value }
    }
    foreach ($nested in @('preflight', 'runtime')) {
        $property = $Object.PSObject.Properties[$nested]
        if ($null -eq $property -or $null -eq $property.Value) { continue }
        $nestedKeys = if ($nested -eq 'preflight') {
            @('editorLaunch', 'requestedTopology', 'observedTopology', 'bridge', 'uiChannel')
        } else {
            @('log', 'scope', 'selectionFixture', 'observedPeerWorlds', 'observedPawns',
              'nativeRelicPassGrants', 'naturalGoals', 'roundCompleteReplications',
              'resultsScreensShown', 'criticalLines', 'errorLines', 'knownNonFatalErrorLines',
              'unknownErrorLines', 'firstCausalError', 'firstKnownNonFatalError',
              'passActivations', 'status')
        }
        $nestedSummary = [ordered]@{}
        foreach ($nestedKey in $nestedKeys) {
            $nestedProperty = $property.Value.PSObject.Properties[$nestedKey]
            if ($null -eq $nestedProperty) { continue }
            $nestedValue = $nestedProperty.Value
            if ($nestedValue -is [System.Array]) {
                $nestedSummary[$nestedKey] = [ordered]@{ count = @($nestedValue).Count; first = @($nestedValue | Select-Object -First 1) }
            } else { $nestedSummary[$nestedKey] = $nestedValue }
        }
        if ($nested -eq 'preflight') {
            $capability = $property.Value.PSObject.Properties['capabilityCache']
            if ($null -ne $capability) {
                $capabilityCounts = [ordered]@{ supported = 0; unsupported = 0; unavailable = 0; notAttempted = 0 }
                foreach ($entry in $capability.Value.PSObject.Properties) {
                    $valueText = [string]$entry.Value
                    if ($valueText -match 'unsupported') { $capabilityCounts.unsupported++ }
                    elseif ($valueText -match 'unavailable') { $capabilityCounts.unavailable++ }
                    elseif ($valueText -match 'not attempted') { $capabilityCounts.notAttempted++ }
                    else { $capabilityCounts.supported++ }
                }
                $nestedSummary.capabilityCache = $capabilityCounts
            }
        } else {
            foreach ($optional in @('proofGaps', 'evidence')) {
                $optionalProperty = $property.Value.PSObject.Properties[$optional]
                if ($null -ne $optionalProperty) {
                    $nestedSummary[$optional] = [ordered]@{ count = @($optionalProperty.Value).Count; first = @($optionalProperty.Value | Select-Object -First 1) }
                }
            }
            $peerFixture = $property.Value.PSObject.Properties['peerConvergenceFixture']
            if ($null -ne $peerFixture) {
                $nestedSummary.peerConvergenceFixture = [ordered]@{}
                foreach ($entry in $peerFixture.Value.PSObject.Properties) {
                    if ($entry.Name -in @('status', 'worlds', 'peerSamples', 'enemyPlayerId', 'authorityHealth', 'authorityTeam', 'attempts', 'evidence', 'scope')) {
                        $nestedSummary.peerConvergenceFixture[$entry.Name] = $entry.Value
                    }
                }
            }
        }
        $summary[$nested] = $nestedSummary
    }
    return $summary
}

function Read-CompactEvidence {
    param([Parameter(Mandatory)][string]$Path)
    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    $record = [ordered]@{ path = $item.FullName; bytes = $item.Length; lastWriteUtc = $item.LastWriteTimeUtc.ToString('o') }
    try {
        $parsed = Get-Content -LiteralPath $item.FullName -Raw | ConvertFrom-Json
        $record.summary = Get-CompactJsonSummary -Object $parsed
    } catch { $record.parse = 'not-json-or-unreadable' }
    return $record
}

function Read-CompactLog {
    param([Parameter(Mandatory)][string]$Path)
    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    $lineCount = 0; $errorLines = 0; $knownNonFatalErrorLines = 0; $unknownErrorLines = 0; $firstUnknownError = $null; $criticalLines = 0
    $knownNonFatalErrorPatterns = @(
        'LogZenServiceInstance: Error: Failed to write security configuration'
    )
    $criticalExamples = [System.Collections.Generic.List[object]]::new()
    $reader = [System.IO.StreamReader]::new([System.IO.FileStream]::new(
        $item.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite))
    try {
        while ($null -ne ($line = $reader.ReadLine())) {
            $lineCount++
            if ($line -match ': Error:') {
                $errorLines++
                $known = $false
                foreach ($knownPattern in $knownNonFatalErrorPatterns) {
                    if ($line -match $knownPattern) { $known = $true; break }
                }
                if ($known) { $knownNonFatalErrorLines++ }
                else {
                    $unknownErrorLines++
                    if ($null -eq $firstUnknownError) { $firstUnknownError = $line }
                }
            }
            if ($line -match 'Ensure condition failed|Assertion failed|Fatal error|Accessed None') {
                $criticalLines++
                if ($criticalExamples.Count -lt $CriticalExampleCount) {
                    $excerpt = if ($line.Length -gt 240) { $line.Substring(0, 240) + ' [truncated]' } else { $line }
                    $criticalExamples.Add([ordered]@{ line = $lineCount; text = $excerpt })
                }
            }
        }
    } finally { $reader.Dispose() }
    return [ordered]@{
        path = $item.FullName; linesRead = $lineCount; errorLines = $errorLines
        knownNonFatalErrorLines = $knownNonFatalErrorLines; unknownErrorLines = $unknownErrorLines
        firstUnknownError = $firstUnknownError; criticalLines = $criticalLines
        criticalExamples = @($criticalExamples.ToArray())
    }
}

$head = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve the current revision.' }
$dirty = @(& git -C $root status --short)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the current worktree.' }
$selectedEvidenceFiles = @($EvidenceFile)
$contractEvidenceFiles = @()
$phaseCausalErrorExplicitlyClear = $false
if ($ContractPhase -and -not $ContractFile) {
    throw '-ContractPhase requires -ContractFile (or its -ContractPath alias).'
}
if ($ContractFile) {
    $selectedEvidenceFiles = @($ContractFile) + $selectedEvidenceFiles
    if ($ContractPhase) {
        $contractPath = if ([System.IO.Path]::IsPathRooted($ContractFile)) {
            $ContractFile
        } elseif (Test-Path -LiteralPath (Join-Path $evidencePath $ContractFile)) {
            Join-Path $evidencePath $ContractFile
        } else {
            Join-Path $root $ContractFile
        }
        $contractObject = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
        $phaseNames = if ($ContractPhase -eq 'all') { @('compile', 'runtime', 'package') } else { @($ContractPhase) }
        foreach ($phaseName in $phaseNames) {
            $phaseProperty = $contractObject.PSObject.Properties[$phaseName]
            if ($null -eq $phaseProperty) { continue }
            if ($phaseNames.Count -eq 1) {
                $causalProperty = $phaseProperty.Value.PSObject.Properties['firstCausalError']
                if ($null -ne $causalProperty -and $null -eq $causalProperty.Value) {
                    $phaseCausalErrorExplicitlyClear = $true
                }
            }
            $evidenceProperty = $phaseProperty.Value.PSObject.Properties['evidence']
            if ($null -ne $evidenceProperty) {
                $contractEvidenceFiles += @($evidenceProperty.Value)
            }
        }
    }
}
$selectedEvidenceFiles = @($selectedEvidenceFiles + $contractEvidenceFiles | Select-Object -Unique)
$contractLogs = @()
if ($ContractPhase -and $contractObject) {
    # Phase evidence is authoritative. Route existing .log entries to the log scanner;
    # missing entries remain visible in the contract summary without breaking the brief.
    $contractLogs = @($contractEvidenceFiles | Where-Object {
        [System.IO.Path]::GetExtension([string]$_) -ieq '.log'
    })
    $selectedEvidenceFiles = @($selectedEvidenceFiles | Where-Object {
        [System.IO.Path]::GetExtension([string]$_) -ine '.log'
    })
}
$selectedEvidenceFiles = @($selectedEvidenceFiles | Select-Object -Unique)
$evidence = foreach ($name in $selectedEvidenceFiles) {
    $path = if ([System.IO.Path]::IsPathRooted($name)) { $name } elseif (Test-Path -LiteralPath (Join-Path $evidencePath $name)) { Join-Path $evidencePath $name } else { Join-Path $root $name }
    Read-CompactEvidence -Path $path
}
$selectedLogFiles = @($LogFile + $contractLogs | Select-Object -Unique)
$logs = foreach ($name in $selectedLogFiles) {
    $path = if ([System.IO.Path]::IsPathRooted($name)) { $name } elseif (Test-Path -LiteralPath (Join-Path $evidencePath $name)) { Join-Path $evidencePath $name } else { Join-Path $root $name }
    Read-CompactLog -Path $path
}
$projectFilePattern = [regex]::Escape((Join-Path $root 'ProjectB.uproject'))
$projectDirectoryPrefix = [System.IO.Path]::GetFullPath($root).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
$processesById = @{}
foreach ($process in @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
    $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe', 'LyraGame.exe', 'UnrealBuildTool.exe', 'AutomationTool.exe')
})) {
    $processesById[[int]$process.ProcessId] = $process
}
$knownProcesses = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -in @('UnrealEditor', 'UnrealEditor-Cmd', 'LyraGame', 'UnrealBuildTool', 'AutomationTool')
} | ForEach-Object {
    $path = $null; try { $path = $_.Path } catch { }
    $commandLine = $null
    if ($processesById.ContainsKey($_.Id)) {
        $commandLine = $processesById[$_.Id].CommandLine
        if (!$path) { $path = $processesById[$_.Id].ExecutablePath }
    }
    $projectGame = $_.ProcessName -eq 'LyraGame' -and $path -and
        $path.StartsWith($projectDirectoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)
    $handle = 0L; try { $handle = $_.MainWindowHandle.ToInt64() } catch { }
    [ordered]@{
        id = $_.Id; name = $_.ProcessName; executable = $path; commandLine = $commandLine
        projectB = [bool](($commandLine -and $commandLine -match $projectFilePattern) -or $projectGame)
        mainWindowHandle = $handle; mainWindowTitle = $_.MainWindowTitle
    }
})
$projectProcesses = @($knownProcesses | Where-Object { $_.projectB })
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o'); gate = $Gate; currentHead = $head
   dirtyFileCount = $dirty.Count; evidenceDirectory = $evidencePath
    contractPhase = if ($ContractPhase) { $ContractPhase } else { $null }
    evidence = @($evidence); logs = @($logs); projectProcessCount = $projectProcesses.Count
    hiddenEditorCount = @($projectProcesses | Where-Object { $_.name -eq 'UnrealEditor' -and $_.mainWindowHandle -eq 0 }).Count
    projectProcesses = @($projectProcesses | Select-Object -First 8)
    otherKnownProcessCount = @($knownProcesses | Where-Object { !$_.projectB }).Count
    scope = 'Compact gate inventory. It summarizes selected JSON/log files and does not establish gameplay acceptance.'
}
$outputPath = if ($OutputJson) {
    if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $evidencePath $OutputJson }
} else {
    Join-Path $evidencePath 'gate-brief.json'
}
$outputDirectory = Split-Path -Parent $outputPath
if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
$result.outputJson = [System.IO.Path]::GetFullPath($outputPath)
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputPath -Encoding UTF8
if ($SummaryOnly) {
    $logErrorLines = 0
    $logUnknownErrorLines = 0
    $logCriticalLines = 0
    foreach ($log in @($logs)) {
        $logErrorLines += [int]$log.errorLines
        $logUnknownErrorLines += [int]$log.unknownErrorLines
        $logCriticalLines += [int]$log.criticalLines
    }
    $firstEvidenceCausalError = @(
        foreach ($item in @($evidence)) {
            if ($item.summary -and $item.summary.firstCausalError) { $item.summary.firstCausalError }
            elseif ($item.summary -and $item.summary.runtime -and $item.summary.runtime.firstCausalError) { $item.summary.runtime.firstCausalError }
        }
    ) | Select-Object -First 1
    $firstLogCausalError = @($logs | Where-Object { $_.firstUnknownError } | Select-Object -First 1 | ForEach-Object firstUnknownError)
    $firstCausalError = $null
    if ($phaseCausalErrorExplicitlyClear) {
        # A single selected phase with an explicit null is authoritative; do not
        # resurrect a causal error from an older evidence log in that phase.
        $firstCausalError = $null
    } elseif (@($firstEvidenceCausalError).Count -gt 0) {
        $firstCausalError = [string]@($firstEvidenceCausalError)[0]
    } elseif (-not $phaseCausalErrorExplicitlyClear -and @($firstLogCausalError).Count -gt 0) {
        $firstCausalError = [string]@($firstLogCausalError)[0]
    }
    [ordered]@{
        generatedUtc = $result.generatedUtc
        gate = $result.gate
        contractPhase = $result.contractPhase
        currentHead = $result.currentHead
        dirtyFileCount = $result.dirtyFileCount
        evidenceCount = @($evidence).Count
        logCount = @($logs).Count
        errorLines = $logErrorLines
        unknownErrorLines = $logUnknownErrorLines
        criticalLines = $logCriticalLines
        firstCausalError = $firstCausalError
        projectProcessCount = $result.projectProcessCount
        hiddenEditorCount = $result.hiddenEditorCount
        scope = $result.scope
        outputJson = $result.outputJson
    } | ConvertTo-Json -Depth 5 -Compress
} else {
    $result | ConvertTo-Json -Depth 8 -Compress
}
