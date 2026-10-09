[CmdletBinding()]
param(
    [string[]]$JsonPath,
    [string]$GateContractPath = 'Saved/Logs/buildable-placement-20260914/gate-contract.json',
    [string]$ResumePath = 'Saved/Logs/codex-resume-brief.json',
    [string]$SourceSnapshotPath = 'Saved/Logs/buildable-placement-20260914/source-snapshot-current.json',
    [string]$OutputJson = 'Saved/Logs/buildable-placement-20260914/workflow-validation-brief.json'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $root $Path))
}

function Read-JsonRecord([string]$Path) {
    $resolved = Resolve-ProjectPath $Path
    if (!(Test-Path -LiteralPath $resolved -PathType Leaf)) {
        return [ordered]@{ path = $resolved; exists = $false; parseError = $false }
    }
    try {
        Get-Content -LiteralPath $resolved -Raw | ConvertFrom-Json | Out-Null
        return [ordered]@{ path = $resolved; exists = $true; parseError = $false }
    } catch {
        return [ordered]@{ path = $resolved; exists = $true; parseError = $true }
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

if (!$JsonPath) {
    $JsonPath = @(
        $SourceSnapshotPath,
        $GateContractPath,
        $ResumePath
    )
} else {
    # PowerShell callers can pass a comma-separated list from a single command-line argument.
    $JsonPath = @($JsonPath | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
}

$scriptCount = 0
$scriptParseErrors = 0
$scriptInventory = @()
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $root 'Scripts/Test') -Filter *.ps1 -Recurse) {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($file.FullName, [ref]$tokens, [ref]$errors) | Out-Null
    $scriptCount++
    $scriptParseErrors += @($errors).Count
    $relativePath = $file.FullName.Substring($root.Length).TrimStart([char[]]@('\','/')).Replace('\','/')
    $scriptInventory += "$relativePath|$((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToUpperInvariant())"
}
$scriptInventoryHash = Get-Sha256Hex (($scriptInventory | Sort-Object) -join "`n")

$jsonRecords = @($JsonPath | ForEach-Object { Read-JsonRecord $_ })
$previousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& git -C $root diff --check 2>$null | Out-Null
$diffCheckExit = $LASTEXITCODE
$ErrorActionPreference = $previousErrorAction
$resume = $null
$gate = $null
$snapshot = $null
try { $resume = Get-Content -LiteralPath (Resolve-ProjectPath $ResumePath) -Raw | ConvertFrom-Json } catch { }
try { $gate = Get-Content -LiteralPath (Resolve-ProjectPath $GateContractPath) -Raw | ConvertFrom-Json } catch { }
try { $snapshot = Get-Content -LiteralPath (Resolve-ProjectPath $SourceSnapshotPath) -Raw | ConvertFrom-Json } catch { }

$ledgerCandidate = if ($resume -and $resume.candidate) { $resume.candidate.name } else { $null }
$contractCandidate = if ($gate) { $gate.candidate } else { $null }
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    scope = 'Workflow hygiene only; does not compile, launch, inspect UI, or establish gameplay acceptance.'
    scriptCount = $scriptCount
    scriptParseErrors = $scriptParseErrors
    scriptInventoryHash = $scriptInventoryHash
    jsonPathCount = $jsonRecords.Count
    jsonMissingCount = @($jsonRecords | Where-Object { !$_.exists }).Count
    jsonParseErrors = @($jsonRecords | Where-Object { $_.parseError }).Count
    gitDiffCheckExit = $diffCheckExit
    ledgerCandidate = $ledgerCandidate
    contractCandidate = $contractCandidate
    candidateConsistent = [bool]($ledgerCandidate -and $contractCandidate -and $ledgerCandidate -eq $contractCandidate)
    sourceIdentity = if ($snapshot) { $snapshot.sourceIdentity } else { $null }
    projectProcessCount = if ($resume) { $resume.projectProcessCount } else { $null }
    hiddenEditorCount = if ($resume) { $resume.hiddenEditorCount } else { $null }
    gatePhase = if ($gate) { $gate.package.status } else { $null }
    proofGapCount = if ($gate) { @($gate.package.proofGaps).Count } else { $null }
    firstCausalError = if ($gate) { $gate.package.firstCausalError } else { $null }
    warnings = @()
}
if (!$result.candidateConsistent) { $result.warnings += 'Ledger and gate contract candidates are missing or disagree.' }
if ($result.jsonMissingCount -gt 0) { $result.warnings += 'One or more requested JSON records are missing.' }
if ($result.jsonParseErrors -gt 0) { $result.warnings += 'One or more requested JSON records failed to parse.' }
if ($result.scriptParseErrors -gt 0) { $result.warnings += 'One or more Scripts/Test PowerShell files failed to parse.' }
if ($result.gitDiffCheckExit -ne 0) { $result.warnings += 'git diff --check reported whitespace errors.' }

$resolvedOutput = Resolve-ProjectPath $OutputJson
New-Item -ItemType Directory -Path (Split-Path -Parent $resolvedOutput) -Force | Out-Null
$result.outputJson = $resolvedOutput
$temp = "$resolvedOutput.tmp"
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $temp -Encoding UTF8
Move-Item -LiteralPath $temp -Destination $resolvedOutput -Force
$result | ConvertTo-Json -Depth 8
