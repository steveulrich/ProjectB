#Requires -Version 5.1
<##
.SYNOPSIS
    Build one bounded resume packet for the next vertical-slice iteration.
.DESCRIPTION
    Runs the contract-aware resume inventory and, when requested, refreshes the
    workflow-hygiene inventory. The packet combines candidate, provenance,
    phase, process, proof-gap, and next-action fields without replacing the
    detailed evidence records on disk. It does not compile, launch, inspect UI,
    or establish gameplay acceptance.

    Use -RefreshSourceSnapshot after deliberate source or ledger edits, and use
    -RefreshWorkflow after contract, snapshot, ledger, or helper edits. A routine
    resume can reuse the last workflow brief; a source or candidate mismatch is
    reported as a warning so stale hygiene is not silent.
#>
[CmdletBinding()]
param(
    [string]$GateContractPath = 'Saved/Logs/buildable-placement-20260914/gate-contract.json',
    [ValidateSet('compile', 'runtime', 'package')][string]$ContractPhase = 'runtime',
    [string]$SourceSnapshotPath,
    [string]$WorkflowValidationPath,
    [string]$OutputJson,
    [switch]$RefreshSourceSnapshot,
    [switch]$RefreshWorkflow
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $root $Path))
}

$gateDirectory = Split-Path -Parent (Resolve-ProjectPath $GateContractPath)
# Keep implicit evidence paths with the selected gate. An explicit override
# still wins, including a frozen candidate snapshot used for provenance review.
if (!$SourceSnapshotPath) { $SourceSnapshotPath = Join-Path $gateDirectory 'source-snapshot-current.json' }
if (!$WorkflowValidationPath) { $WorkflowValidationPath = Join-Path $gateDirectory 'workflow-validation-brief.json' }
if (!$OutputJson) { $OutputJson = Join-Path $gateDirectory 'next-iteration-brief.json' }
$resolvedSourceSnapshotPath = Resolve-ProjectPath $SourceSnapshotPath

function Read-JsonFile([string]$Path) {
    $resolved = Resolve-ProjectPath $Path
    if (!(Test-Path -LiteralPath $resolved -PathType Leaf)) {
        return $null
    }
    try {
        return Get-Content -LiteralPath $resolved -Raw | ConvertFrom-Json
    } catch {
        return [pscustomobject]@{ parseError = $true; path = $resolved }
    }
}

function Get-Sha256Hex([string]$Text) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        return (($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString('X2') }) -join '')
    } finally {
        $sha.Dispose()
    }
}

function Get-CanonicalFileHash([string]$Path) {
    # New-SourceSnapshot writes UTF-8 with a BOM under Windows PowerShell 5.1,
    # while the resume helper may run under PowerShell 7. Compare content rather
    # than shell-specific encoding bytes; the original byte hashes stay in the
    # snapshot for audit and candidate identity.
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $text = [System.IO.File]::ReadAllText($Path)
    if ($text.Length -gt 0 -and $text[0] -eq [char]0xFEFF) {
        $text = $text.Substring(1)
    }
    $text = $text -replace "`r`n", "`n" -replace "`r", "`n"
    return Get-Sha256Hex $text
}

function Get-ScriptInventoryHash {
    $inventory = @()
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $root 'Scripts/Test') -Filter *.ps1 -Recurse) {
        $relativePath = $file.FullName.Substring($root.Length).TrimStart([char[]]@('\','/')).Replace('\','/')
        $inventory += "$relativePath|$((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToUpperInvariant())"
    }
    return Get-Sha256Hex (($inventory | Sort-Object) -join "`n")
}

function Get-SourceSnapshotFreshness {
    param([object]$Snapshot)

    if ($null -eq $Snapshot -or $Snapshot.parseError) {
        return [ordered]@{
            status = if ($Snapshot -and $Snapshot.parseError) { 'parse-error' } else { 'missing' }
            fresh = $false
            reasonCount = 1
            reasons = @('A readable source snapshot is required before an expensive gate.')
        }
    }
    if (!$Snapshot.sourceHead -or !$Snapshot.files.patch.sha256 -or !$Snapshot.files.status.sha256 -or !$Snapshot.files.untracked.sha256) {
        return [ordered]@{
            status = 'incomplete'
            fresh = $false
            reasonCount = 1
            reasons = @('The source snapshot is missing one or more provenance hashes; refresh it before an expensive gate.')
        }
    }

    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("projectb-source-freshness-$([guid]::NewGuid().ToString('N'))")
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
    $patchPath = Join-Path $tempRoot 'current.patch'
    $statusPath = Join-Path $tempRoot 'current.status'
    $untrackedPath = Join-Path $tempRoot 'current.untracked'
    try {
        $previousErrorAction = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            & git -C $root diff HEAD --binary "--output=$patchPath" -- . 2>$null | Out-Null
            $diffExit = $LASTEXITCODE
            $statusLines = @(& git -C $root status --porcelain=v1 --untracked-files=all 2>$null)
            $statusExit = $LASTEXITCODE
            $statusLines | Set-Content -LiteralPath $statusPath -Encoding UTF8
            $untrackedLines = @(& git -C $root ls-files --others --exclude-standard 2>$null)
            $untrackedExit = $LASTEXITCODE
            $untrackedLines | Set-Content -LiteralPath $untrackedPath -Encoding UTF8
            $currentHead = (& git -C $root rev-parse HEAD 2>$null).Trim()
            $headExit = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousErrorAction
        }

        $patchHash = Get-CanonicalFileHash $patchPath
        $statusHash = Get-CanonicalFileHash $statusPath
        $untrackedHash = Get-CanonicalFileHash $untrackedPath
        $snapshotPatchHash = Get-CanonicalFileHash ([string]$Snapshot.files.patch.path)
        $snapshotStatusHash = Get-CanonicalFileHash ([string]$Snapshot.files.status.path)
        $snapshotUntrackedHash = Get-CanonicalFileHash ([string]$Snapshot.files.untracked.path)
        $headMatches = [bool]($headExit -eq 0 -and $currentHead -eq $Snapshot.sourceHead)
        $patchMatches = [bool]($diffExit -eq 0 -and $null -ne $snapshotPatchHash -and $patchHash -eq $snapshotPatchHash)
        $statusMatches = [bool]($statusExit -eq 0 -and $null -ne $snapshotStatusHash -and $statusHash -eq $snapshotStatusHash)
        $untrackedMatches = [bool]($untrackedExit -eq 0 -and $null -ne $snapshotUntrackedHash -and $untrackedHash -eq $snapshotUntrackedHash)
        $reasons = @()
        if (!$headMatches) { $reasons += 'HEAD changed since the recorded source snapshot.' }
        if (!$patchMatches) { $reasons += 'The tracked patch changed since the recorded source snapshot.' }
        if (!$statusMatches) { $reasons += 'The worktree status changed since the recorded source snapshot.' }
        if (!$untrackedMatches) { $reasons += 'The untracked-file manifest changed since the recorded source snapshot.' }
        if ($diffExit -ne 0 -or $statusExit -ne 0 -or $untrackedExit -ne 0 -or $headExit -ne 0) {
            $reasons += 'Git provenance inspection did not complete successfully.'
        }
        return [ordered]@{
            status = if ($reasons.Count -eq 0) { 'fresh' } else { 'stale' }
            fresh = [bool]($reasons.Count -eq 0)
            reasonCount = $reasons.Count
            reasons = $reasons
            sourceHeadMatches = $headMatches
            patchMatches = $patchMatches
            statusMatches = $statusMatches
            untrackedMatches = $untrackedMatches
        }
    } catch {
        return [ordered]@{
            status = 'check-error'
            fresh = $false
            reasonCount = 1
            reasons = @("Source snapshot freshness check failed: $($_.Exception.Message)")
        }
    } finally {
        if (Test-Path -LiteralPath $tempRoot -PathType Container) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

function Invoke-JsonScript {
    param(
        [Parameter(Mandatory)][string]$ScriptPath,
        [Parameter(Mandatory)][string[]]$Arguments
    )
    $tempRoot = [System.IO.Path]::GetTempPath()
    $token = [guid]::NewGuid().ToString('N')
    $stdoutPath = Join-Path $tempRoot "projectb-next-iteration-$token.stdout"
    $stderrPath = Join-Path $tempRoot "projectb-next-iteration-$token.stderr"
    try {
        $previousErrorAction = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            & powershell.exe -NoProfile -File $ScriptPath @Arguments 1> $stdoutPath 2> $stderrPath
        } finally {
            $ErrorActionPreference = $previousErrorAction
        }
        $exitCode = $LASTEXITCODE
        $stdoutRaw = if (Test-Path -LiteralPath $stdoutPath -PathType Leaf) {
            Get-Content -LiteralPath $stdoutPath -Raw
        } else { $null }
        $stderrRaw = if (Test-Path -LiteralPath $stderrPath -PathType Leaf) {
            Get-Content -LiteralPath $stderrPath -Raw
        } else { $null }
        $text = if ($null -eq $stdoutRaw) { '' } else { ([string]$stdoutRaw).Trim() }
        $errorText = if ($null -eq $stderrRaw) { '' } else { ([string]$stderrRaw).Trim() }
        if ($exitCode -ne 0) {
            throw "Helper failed ($exitCode): $ScriptPath`n$text`n$errorText"
        }
        try {
            return $text | ConvertFrom-Json
        } catch {
            throw "Helper returned non-JSON output: $ScriptPath`n$text`n$errorText"
        }
    } finally {
        foreach ($path in @($stdoutPath, $stderrPath)) {
            if (Test-Path -LiteralPath $path -PathType Leaf) {
                Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
            }
        }
    }
}

$sourceSnapshotRefreshed = $false
if ($RefreshSourceSnapshot) {
    $snapshotScript = Join-Path $PSScriptRoot 'New-SourceSnapshot.ps1'
    $snapshotPrefix = [System.IO.Path]::ChangeExtension($resolvedSourceSnapshotPath, $null)
    [void](Invoke-JsonScript -ScriptPath $snapshotScript -Arguments @(
        '-OutputPrefix', $snapshotPrefix
        '-Force'
    ))
    $sourceSnapshotRefreshed = $true
}

$resumeScript = Join-Path $PSScriptRoot 'Get-ResumeBrief.ps1'
$resume = Invoke-JsonScript -ScriptPath $resumeScript -Arguments @(
    '-SummaryOnly'
    '-GateContractPath', (Resolve-ProjectPath $GateContractPath)
    '-ContractPhase', $ContractPhase
    '-SourceSnapshotPath', $resolvedSourceSnapshotPath
)

$sourceSnapshotRecord = Read-JsonFile $resolvedSourceSnapshotPath
$sourceFreshness = Get-SourceSnapshotFreshness -Snapshot $sourceSnapshotRecord

$workflowPath = Resolve-ProjectPath $WorkflowValidationPath
$workflow = $null
if ($RefreshWorkflow) {
    $workflowScript = Join-Path $PSScriptRoot 'Get-WorkflowValidationBrief.ps1'
    $workflow = Invoke-JsonScript -ScriptPath $workflowScript -Arguments @(
        '-GateContractPath', (Resolve-ProjectPath $GateContractPath)
        '-ResumePath', (Join-Path (Resolve-ProjectPath 'Saved/Logs') 'codex-resume-brief.json')
        '-SourceSnapshotPath', $resolvedSourceSnapshotPath
        '-OutputJson', $workflowPath
    )
} else {
    $workflow = Read-JsonFile $workflowPath
}

$warnings = @($resume.warnings)
if ($null -eq $workflow) {
    $warnings += "Workflow hygiene brief was not found: $workflowPath. Run with -RefreshWorkflow before relying on hygiene fields."
} elseif ($workflow.parseError) {
    $warnings += "Workflow hygiene brief could not be parsed: $workflowPath. Run with -RefreshWorkflow before relying on hygiene fields."
} else {
    $resumeSourceIdentity = if ($resume.sourceSnapshot) { [string]$resume.sourceSnapshot.sourceIdentity } else { $null }
    if ($resumeSourceIdentity -and $workflow.sourceIdentity -and $resumeSourceIdentity -ne $workflow.sourceIdentity) {
        $warnings += 'Workflow hygiene brief sourceIdentity does not match the resume snapshot; refresh it before relying on hygiene fields.'
    }
    if ($resume.candidate -and $workflow.ledgerCandidate -and $resume.candidate.name -ne $workflow.ledgerCandidate) {
        $warnings += 'Workflow hygiene brief candidate does not match the resume candidate; refresh it before relying on hygiene fields.'
    }
    if (!$workflow.scriptInventoryHash) {
        $warnings += 'Workflow hygiene brief lacks a script inventory hash; refresh it before relying on hygiene fields.'
    } else {
        $currentScriptInventoryHash = Get-ScriptInventoryHash
        if ($workflow.scriptInventoryHash -ne $currentScriptInventoryHash) {
            $warnings += 'Workflow hygiene brief script inventory is stale; refresh it before relying on hygiene fields.'
        }
    }
    $warnings += @($workflow.warnings)
}
if (!$sourceFreshness.fresh) {
    $warnings += 'Source snapshot is stale or unavailable; run with -RefreshSourceSnapshot before an expensive gate.'
    $warnings += @($sourceFreshness.reasons)
}

$gate = $resume.gate
$candidate = $resume.candidate
$currentSource = $resume.sourceSnapshot
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    scope = 'Bounded next-iteration packet; full resume, workflow, contract, source, and gate evidence remain on disk.'
    sourceSnapshotRefreshed = $sourceSnapshotRefreshed
    sourceSnapshotFreshness = $sourceFreshness
    candidate = if ($candidate) { $candidate.name } else { $null }
    sourceIdentity = if ($currentSource) { $currentSource.sourceIdentity } else { $null }
    candidateRecordedSourceHead = if ($candidate) { $candidate.recordedSourceHead } else { $null }
    currentHeadEqualsCandidateRecordedBase = if ($candidate) { $candidate.currentHeadEqualsRecordedBase } else { $null }
    candidateSourceIdentity = if ($gate) { $gate.candidateSourceIdentity } else { $null }
    candidateArtifactFrozen = if ($gate) { $gate.candidateArtifactFrozen } else { $null }
    candidateSourceIdentityMatches = if ($gate -and $gate.candidateSourceIdentity -and $currentSource) { $gate.candidateSourceIdentity -eq $currentSource.sourceIdentity } else { $null }
    phase = if ($gate) { $gate.phase } else { $ContractPhase }
    phaseStatus = if ($gate) { $gate.phaseStatus } else { $null }
    proofGapCount = if ($gate) { $gate.proofGapCount } else { $null }
    firstCausalError = if ($gate) { $gate.firstCausalError } else { $null }
    nextAction = if ($gate -and $gate.nextAction) { $gate.nextAction } else { $resume.nextActionFromLedger }
    retryCondition = if ($gate) { $gate.retryCondition } else { $null }
    projectProcessCount = $resume.projectProcessCount
    hiddenEditorCount = $resume.hiddenEditorCount
    warnings = $warnings
    evidence = [ordered]@{
        resumeSnapshot = $resume.fullSnapshot
        workflowValidation = if ($workflow) { $workflow.outputJson } else { $workflowPath }
        gateContract = if ($gate) { $gate.path } else { (Resolve-ProjectPath $GateContractPath) }
        sourceSnapshot = if ($currentSource) { $currentSource.path } else { (Resolve-ProjectPath $SourceSnapshotPath) }
    }
}

$resolvedOutput = Resolve-ProjectPath $OutputJson
New-Item -ItemType Directory -Path (Split-Path -Parent $resolvedOutput) -Force | Out-Null
$result.outputJson = $resolvedOutput
$temporaryPath = "$resolvedOutput.$([guid]::NewGuid().ToString('N')).partial"
try {
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $temporaryPath -Encoding UTF8
    Move-Item -LiteralPath $temporaryPath -Destination $resolvedOutput -Force
} finally {
    if (Test-Path -LiteralPath $temporaryPath -PathType Leaf) {
        Remove-Item -LiteralPath $temporaryPath -Force -ErrorAction SilentlyContinue
    }
}
$result | ConvertTo-Json -Depth 8
