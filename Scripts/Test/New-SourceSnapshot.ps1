#Requires -Version 5.1
<##
.SYNOPSIS
    Persist a compact, reproducible checkout snapshot for a gate.
.DESCRIPTION
    Captures the current HEAD, complete tracked diff (including staged changes),
    porcelain status, and untracked-file manifest without printing the patch to
    the agent context. The JSON record contains counts, hashes, and paths; the
    complete source material remains on disk for candidate reuse review.

    This helper is read-only with respect to the checkout. It writes only the
    requested evidence files.

.EXAMPLE
    .\New-SourceSnapshot.ps1 `
      -OutputPrefix Saved\Logs\team-gameplay-20260912\source-snapshot-buildable
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputPrefix,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$prefix = if ([System.IO.Path]::IsPathRooted($OutputPrefix)) { $OutputPrefix } else { Join-Path $root $OutputPrefix }
$prefix = [System.IO.Path]::GetFullPath($prefix)
$directory = Split-Path -Parent $prefix
if ($directory -and -not (Test-Path -LiteralPath $directory -PathType Container)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}
$paths = [ordered]@{
    patch = "$prefix.patch"
    status = "$prefix.status"
    untracked = "$prefix.untracked"
    outputJson = "$prefix.json"
}
if (-not $Force) {
    $existing = @($paths.Values | Where-Object { Test-Path -LiteralPath $_ })
    if ($existing.Count -gt 0) {
        throw "Source snapshot outputs already exist; choose a fresh OutputPrefix or pass -Force: $($existing -join ', ')"
    }
}

$head = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-fA-F]{40}$') { throw 'Cannot resolve the current source revision.' }

# Compare against HEAD so staged and unstaged tracked edits are both included.
& git -C $root diff HEAD --binary "--output=$($paths.patch)" -- .
if ($LASTEXITCODE -ne 0) { throw 'Cannot capture the tracked source patch.' }

$statusLines = @(& git -C $root status --porcelain=v1 --untracked-files=all)
if ($LASTEXITCODE -ne 0) { throw 'Cannot capture the source status.' }
$statusText = if ($statusLines.Count -gt 0) { $statusLines -join [Environment]::NewLine } else { '' }
[System.IO.File]::WriteAllText($paths.status, $statusText, [System.Text.UTF8Encoding]::new($true))

$untrackedLines = @(& git -C $root ls-files --others --exclude-standard)
if ($LASTEXITCODE -ne 0) { throw 'Cannot capture the untracked-file manifest.' }
$untrackedText = if ($untrackedLines.Count -gt 0) { $untrackedLines -join [Environment]::NewLine } else { '' }
[System.IO.File]::WriteAllText($paths.untracked, $untrackedText, [System.Text.UTF8Encoding]::new($true))

$trackedChanged = @(& git -C $root diff HEAD --name-only -- .)
if ($LASTEXITCODE -ne 0) { throw 'Cannot count tracked source changes.' }
$statusLines = @($statusLines | Where-Object { $_ })
$trackedChanged = @($trackedChanged | Where-Object { $_ })
$untrackedLines = @($untrackedLines | Where-Object { $_ })

function Get-FileMetadata {
    param([Parameter(Mandatory)][string]$Path)
    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    return [ordered]@{
        path = $item.FullName
        bytes = $item.Length
        sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    }
}

function Get-Sha256Hex {
    param([Parameter(Mandatory)][string]$Text)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        return (($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString('X2') }) -join '')
    } finally {
        $sha.Dispose()
    }
}

$patchMetadata = Get-FileMetadata -Path $paths.patch
$statusMetadata = Get-FileMetadata -Path $paths.status
$untrackedMetadata = Get-FileMetadata -Path $paths.untracked
# This is an index for quick candidate comparisons. The complete files and their
# individual hashes remain the authoritative provenance record.
$sourceIdentity = Get-Sha256Hex -Text ($head + '|' + $patchMetadata.sha256 + '|' + $statusMetadata.sha256 + '|' + $untrackedMetadata.sha256)

$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    scope = 'Source provenance snapshot only; it does not establish compile, runtime, package, or acceptance status.'
    projectRoot = $root
    sourceHead = $head
    dirtyFileCount = $statusLines.Count
    trackedChangedFileCount = $trackedChanged.Count
    untrackedFileCount = $untrackedLines.Count
    sourceIdentity = $sourceIdentity
    paths = $paths
    files = [ordered]@{
        patch = $patchMetadata
        status = $statusMetadata
        untracked = $untrackedMetadata
    }
    nextAction = 'Use this snapshot to review candidate source identity and untracked inputs before an expensive gate.'
}
$result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $paths.outputJson -Encoding UTF8
$result | ConvertTo-Json -Depth 10 -Compress
