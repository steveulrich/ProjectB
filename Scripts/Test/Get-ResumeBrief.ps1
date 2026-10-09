[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Candidate,
    [switch]$IncludeAllProcesses,
    [switch]$SummaryOnly,
    [string]$GateContractPath,
    [string]$SourceSnapshotPath,
    [ValidateSet('compile', 'runtime', 'package')][string]$ContractPhase = 'runtime'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ledgerPath = Join-Path $root 'AI_Planning/SLICE_STATUS.md'
$ledger = Get-Content -LiteralPath $ledgerPath -Raw
if (!$Candidate -and $ledger -match '(?m)^\*\*Current candidate:\*\* `([A-Za-z0-9_-]+)`') {
    $Candidate = $Matches[1]
}
$head = & git -C $root rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve the current revision.' }
$dirty = @(& git -C $root status --short)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the current worktree.' }
$warnings = @()
$candidateInfo = $null
if ($Candidate) {
    $prefix = Join-Path $root "Saved/Logs/codex-packaged-$Candidate"
    $config = Import-PowerShellDataFile (Join-Path $PSScriptRoot 'BwayTestConfig.psd1')
    $exe = Join-Path $root "Saved/StagedBuilds/$Candidate/Windows/ProjectB/Binaries/Win64/$($config.GameBinary)"
    $records = [ordered]@{}
    foreach ($suffix in @('project-head.txt', 'project.patch', 'engine-head.txt', 'engine.patch', 'sha256.csv', 'exit.txt', 'build.log')) {
        $records[$suffix] = Test-Path -LiteralPath "$prefix-$suffix" -PathType Leaf
    }
    $sourceHead = if ($records['project-head.txt']) { (Get-Content -LiteralPath "$prefix-project-head.txt" -Raw).Trim() } else { $null }
    $exitRecord = if ($records['exit.txt']) { (Get-Content -LiteralPath "$prefix-exit.txt" -Raw).Trim() } else { $null }
    $candidateInfo = [ordered]@{
        name = $Candidate
        executableExists = Test-Path -LiteralPath $exe -PathType Leaf
        recordedSourceHead = $sourceHead
        sourceHeadValid = [bool]($sourceHead -match '^[0-9a-fA-F]{40}$')
        currentHeadEqualsRecordedBase = [bool]($sourceHead -and $sourceHead -eq $head.Trim())
        recordedBuildExit = $exitRecord
        evidencePrefix = $prefix
        missingRecords = @($records.Keys | Where-Object { !$records[$_] })
        reuseRequiresSourcePatchAssetAndConfigReview = $true
    }
    if (!$candidateInfo.executableExists -or $candidateInfo.missingRecords.Count -gt 0 -or !$candidateInfo.sourceHeadValid -or $exitRecord -ne '0') {
        $warnings += 'Candidate evidence is incomplete or unsuccessful; inspect it before selecting a build or test action.'
    }
} else {
    $warnings += 'No candidate selected. Pass -Candidate or set the Current candidate field in the ledger.'
}
# Include dotnet hosts because UBT/UAT can run under them. This is a hint, not an exhaustive build lock.
$projectFilePattern = [regex]::Escape((Join-Path $root 'ProjectB.uproject'))
$projectDirectoryPrefix = [System.IO.Path]::GetFullPath($root).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
$processDetails = @{}
foreach ($process in @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue)) {
    $processDetails[[int]$process.ProcessId] = $process
}
$processes = @(Get-Process | Where-Object { $_.ProcessName -in @('UnrealEditor', 'UnrealEditor-Cmd', 'LyraGame', 'UnrealBuildTool', 'AutomationTool', 'dotnet') } | ForEach-Object {
    $executablePath = $null
    try { $executablePath = $_.Path } catch { }
    $mainWindowHandle = 0L
    try { $mainWindowHandle = $_.MainWindowHandle.ToInt64() } catch { }
    $commandLine = $null
    if ($processDetails.ContainsKey($_.Id)) {
        $commandLine = $processDetails[$_.Id].CommandLine
        if (!$executablePath) { $executablePath = $processDetails[$_.Id].ExecutablePath }
    }
    # Packaged games start with a map, not a .uproject argument. Match their actual
    # executable within this project root, retaining a directory boundary.
    $projectGame = $_.ProcessName -eq 'LyraGame' -and $executablePath -and
        $executablePath.StartsWith($projectDirectoryPrefix, [System.StringComparison]::OrdinalIgnoreCase)
    [ordered]@{
        id = $_.Id; name = $_.ProcessName; executable = $executablePath; commandLine = $commandLine
        projectB = [bool](($commandLine -and $commandLine -match $projectFilePattern) -or $projectGame)
        mainWindowHandle = $mainWindowHandle; mainWindowTitle = $_.MainWindowTitle
    }
})
$projectProcesses = @($processes | Where-Object { $_.projectB })
$ledgerLines = @($ledger -split '\r?\n').Count
if ($ledgerLines -gt 100) { $warnings += 'Condense the ledger below about 100 lines; link detailed history.' }
$nextAction = @($ledger -split '\r?\n' | Where-Object { $_ -match '^\*\*Next action:\*\*' } | Select-Object -First 1)
$nextActionFromLedger = $nextAction -join ''
$nextActionFromLedgerBrief = if ($nextActionFromLedger.Length -gt 360) {
    $nextActionCut = $nextActionFromLedger.Substring(0, 360)
    $lastSpace = $nextActionCut.LastIndexOf(' ')
    if ($lastSpace -gt 0) { $nextActionCut = $nextActionCut.Substring(0, $lastSpace) }
    $nextActionCut + '... [fullSnapshot]'
} else {
    $nextActionFromLedger
}
$gateSummary = $null
if ($GateContractPath) {
    $resolvedContractPath = if ([System.IO.Path]::IsPathRooted($GateContractPath)) {
        [System.IO.Path]::GetFullPath($GateContractPath)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $root $GateContractPath))
    }
    if (Test-Path -LiteralPath $resolvedContractPath -PathType Leaf) {
        $contract = Get-Content -LiteralPath $resolvedContractPath -Raw | ConvertFrom-Json
        if ($contract.candidate -and $Candidate -and $contract.candidate -ne $Candidate) {
            $warnings += "Ledger candidate '$Candidate' does not match gate contract candidate '$($contract.candidate)'; do not reuse either artifact until the current candidate is reconciled."
        }
        $phase = $contract.PSObject.Properties[$ContractPhase].Value
        $gateSummary = [ordered]@{
            path = $resolvedContractPath
            gate = $contract.gate
            candidate = $contract.candidate
            candidateSourceIdentity = $contract.candidateSourceIdentity
            candidateArtifactFrozen = $contract.candidateArtifactFrozen
            overallStatus = $contract.status
            phase = $ContractPhase
            phaseStatus = $phase.status
            scope = $phase.scope
            evidenceCount = @($phase.evidence).Count
            proofGapCount = @($phase.proofGaps).Count
            firstCausalError = $phase.firstCausalError
            nextAction = $contract.nextAction
            retryCondition = $contract.retryCondition
        }
    } else {
        $warnings += "Gate contract was requested but not found: $resolvedContractPath"
    }
}
$sourceSnapshotSummary = $null
if ($SourceSnapshotPath) {
    $resolvedSnapshotPath = if ([System.IO.Path]::IsPathRooted($SourceSnapshotPath)) {
        [System.IO.Path]::GetFullPath($SourceSnapshotPath)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $root $SourceSnapshotPath))
    }
    if (Test-Path -LiteralPath $resolvedSnapshotPath -PathType Leaf) {
        try {
            $snapshot = Get-Content -LiteralPath $resolvedSnapshotPath -Raw | ConvertFrom-Json
            $sourceSnapshotSummary = [ordered]@{
                path = $resolvedSnapshotPath
                sourceIdentity = $snapshot.sourceIdentity
                sourceHead = $snapshot.sourceHead
                dirtyFileCount = $snapshot.dirtyFileCount
                trackedChangedFileCount = $snapshot.trackedChangedFileCount
                untrackedFileCount = $snapshot.untrackedFileCount
                patchSha256 = $snapshot.files.patch.sha256
                statusSha256 = $snapshot.files.status.sha256
                untrackedSha256 = $snapshot.files.untracked.sha256
            }
        } catch {
            $warnings += "Source snapshot could not be parsed: $resolvedSnapshotPath"
        }
    } else {
        $warnings += "Source snapshot was requested but not found: $resolvedSnapshotPath"
    }
}
if ($gateSummary -and $gateSummary.candidateSourceIdentity -and $sourceSnapshotSummary -and $sourceSnapshotSummary.sourceIdentity -and $gateSummary.candidateSourceIdentity -ne $sourceSnapshotSummary.sourceIdentity) {
    $warnings += "Candidate source identity differs from the current snapshot; reuse only for the candidate's recorded frozen scope after provenance review."
}
$logDirectory = Join-Path $root 'Saved/Logs'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$outputPath = Join-Path $logDirectory 'codex-resume-brief.json'
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    scope = 'Resume inventory only. Does not resume the goal, launch processes, verify artifact hashes, or pass acceptance gates.'
    ledgerPath = $ledgerPath
    ledgerLines = $ledgerLines
    currentHead = $head.Trim()
    dirtyFileCount = $dirty.Count
    candidate = $candidateInfo
    processCount = $processes.Count
    projectProcessCount = $projectProcesses.Count
    hiddenEditorCount = @($projectProcesses | Where-Object { $_.name -eq 'UnrealEditor' -and $_.mainWindowHandle -eq 0 }).Count
    processes = $processes
    projectProcesses = @($projectProcesses | Select-Object -First 8)
    processScope = 'Known Unreal process names and possible dotnet build hosts; verify ownership and unsaved assets before acting.'
    nextActionFromLedger = $nextActionFromLedger
    gate = $gateSummary
    sourceSnapshot = $sourceSnapshotSummary
    warnings = $warnings
    fullSnapshot = $outputPath
    dirtyFiles = $dirty
}
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputPath -Encoding UTF8
# Keep full status on disk; bound process output even on a busy build machine.
$result.Remove('dirtyFiles')
$result.otherKnownProcessCount = @($processes | Where-Object { !$_.projectB }).Count
$result.processOutputScope = if ($IncludeAllProcesses) {
    'All known Unreal process records are included because -IncludeAllProcesses was supplied.'
} else {
    'Compact output includes all ProjectB-owned processes and up to six unrelated process records. Full records remain in fullSnapshot; use -IncludeAllProcesses only for ownership diagnosis.'
}
$result.processes = if ($IncludeAllProcesses) {
    @($processes)
} else {
    @(@($projectProcesses) + @($processes | Where-Object { !$_.projectB } | Select-Object -First 6))
}
if ($SummaryOnly) {
    $summary = [ordered]@{
        generatedUtc = $result.generatedUtc
        scope = 'Compact resume projection; the full process and dirty-file inventory remains in fullSnapshot.'
        ledgerLines = $result.ledgerLines
        currentHead = $result.currentHead
        dirtyFileCount = $result.dirtyFileCount
        candidate = if ($result.candidate) {
            [ordered]@{
                name = $result.candidate.name
                executableExists = $result.candidate.executableExists
                recordedSourceHead = $result.candidate.recordedSourceHead
                sourceHeadValid = $result.candidate.sourceHeadValid
                currentHeadEqualsRecordedBase = $result.candidate.currentHeadEqualsRecordedBase
                recordedBuildExit = $result.candidate.recordedBuildExit
                missingRecords = @($result.candidate.missingRecords)
            }
        } else { $null }
        processCount = $result.processCount
        projectProcessCount = $result.projectProcessCount
        hiddenEditorCount = $result.hiddenEditorCount
        nextActionFromLedger = $nextActionFromLedgerBrief
        gate = $result.gate
        sourceSnapshot = $result.sourceSnapshot
        warnings = @($result.warnings)
        fullSnapshot = $result.fullSnapshot
    }
    $summary | ConvertTo-Json -Depth 8
} else {
    $result | ConvertTo-Json -Depth 8
}
