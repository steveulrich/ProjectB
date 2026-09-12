[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Gate,
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [string[]]$EvidenceFile = @(),
    [string[]]$LogFile = @(),
    [ValidateRange(0, 6)][int]$CriticalExampleCount = 2
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
        'errorLines', 'criticalLines', 'normalExits', 'events', 'nextAction'
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
    $lineCount = 0; $errorLines = 0; $criticalLines = 0
    $criticalExamples = [System.Collections.Generic.List[object]]::new()
    $reader = [System.IO.StreamReader]::new([System.IO.FileStream]::new(
        $item.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite))
    try {
        while ($null -ne ($line = $reader.ReadLine())) {
            $lineCount++
            if ($line -match ': Error:') { $errorLines++ }
            if ($line -match 'Ensure condition failed|Assertion failed|Fatal error|Accessed None') {
                $criticalLines++
                if ($criticalExamples.Count -lt $CriticalExampleCount) {
                    $excerpt = if ($line.Length -gt 240) { $line.Substring(0, 240) + ' [truncated]' } else { $line }
                    $criticalExamples.Add([ordered]@{ line = $lineCount; text = $excerpt })
                }
            }
        }
    } finally { $reader.Dispose() }
    return [ordered]@{ path = $item.FullName; linesRead = $lineCount; errorLines = $errorLines; criticalLines = $criticalLines; criticalExamples = @($criticalExamples.ToArray()) }
}

$head = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve the current revision.' }
$dirty = @(& git -C $root status --short)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the current worktree.' }
$evidence = foreach ($name in $EvidenceFile) {
    $path = if ([System.IO.Path]::IsPathRooted($name)) { $name } elseif (Test-Path -LiteralPath (Join-Path $evidencePath $name)) { Join-Path $evidencePath $name } else { Join-Path $root $name }
    Read-CompactEvidence -Path $path
}
$logs = foreach ($name in $LogFile) {
    $path = if ([System.IO.Path]::IsPathRooted($name)) { $name } elseif (Test-Path -LiteralPath (Join-Path $evidencePath $name)) { Join-Path $evidencePath $name } else { Join-Path $root $name }
    Read-CompactLog -Path $path
}
$knownProcesses = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -in @('UnrealEditor', 'UnrealEditor-Cmd', 'LyraGame', 'UnrealBuildTool', 'AutomationTool')
} | ForEach-Object {
    $path = $null; try { $path = $_.Path } catch { }
    [ordered]@{ id = $_.Id; name = $_.ProcessName; executable = $path; mainWindowHandle = $_.MainWindowHandle.ToInt64(); mainWindowTitle = $_.MainWindowTitle }
})
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o'); gate = $Gate; currentHead = $head
   dirtyFileCount = $dirty.Count; evidenceDirectory = $evidencePath
    evidence = @($evidence); logs = @($logs); projectProcessCount = $knownProcesses.Count
    hiddenEditorCount = @($knownProcesses | Where-Object { $_.name -eq 'UnrealEditor' -and $_.mainWindowHandle -eq 0 }).Count
    projectProcesses = @($knownProcesses | Select-Object -First 8)
    scope = 'Compact gate inventory. It summarizes selected JSON/log files and does not establish gameplay acceptance.'
}
$outputPath = Join-Path $evidencePath 'gate-brief.json'
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputPath -Encoding UTF8
$result | ConvertTo-Json -Depth 8 -Compress
