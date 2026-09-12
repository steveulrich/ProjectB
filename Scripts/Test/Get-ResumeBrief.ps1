[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Candidate
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
$processes = @(Get-Process | Where-Object { $_.ProcessName -in @('UnrealEditor', 'UnrealEditor-Cmd', 'LyraGame', 'UnrealBuildTool', 'AutomationTool', 'dotnet') } | ForEach-Object {
    $executablePath = $null
    try { $executablePath = $_.Path } catch { }
    $mainWindowHandle = 0L
    try { $mainWindowHandle = $_.MainWindowHandle.ToInt64() } catch { }
    [ordered]@{ id = $_.Id; name = $_.ProcessName; executable = $executablePath; mainWindowHandle = $mainWindowHandle; mainWindowTitle = $_.MainWindowTitle }
})
$ledgerLines = @($ledger -split '\r?\n').Count
if ($ledgerLines -gt 100) { $warnings += 'Condense the ledger below about 100 lines; link detailed history.' }
$nextAction = @($ledger -split '\r?\n' | Where-Object { $_ -match '^\*\*Next action:\*\*' } | Select-Object -First 1)
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
    hiddenEditorCount = @($processes | Where-Object { $_.name -eq 'UnrealEditor' -and $_.mainWindowHandle -eq 0 }).Count
    processes = $processes
    processScope = 'Known Unreal process names and possible dotnet build hosts; verify ownership and unsaved assets before acting.'
    nextActionFromLedger = $nextAction -join ''
    warnings = $warnings
    fullSnapshot = $outputPath
    dirtyFiles = $dirty
}
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputPath -Encoding UTF8
# Keep full status on disk; bound process output even on a busy build machine.
$result.Remove('dirtyFiles')
$result.processes = @($processes | Select-Object -First 6)
$result | ConvertTo-Json -Depth 8
