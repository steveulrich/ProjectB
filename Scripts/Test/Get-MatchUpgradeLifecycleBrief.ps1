[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogFile,
    [string]$OutputJson,
    [ValidateRange(0, 6)][int]$CriticalExampleCount = 2
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$path = if ([System.IO.Path]::IsPathRooted($LogFile)) {
    (Resolve-Path -LiteralPath $LogFile -ErrorAction Stop).Path
} else {
    (Resolve-Path -LiteralPath (Join-Path $root $LogFile) -ErrorAction Stop).Path
}

$markerPatterns = [ordered]@{
    setup = 'BwayMatchUpgradeLifecycleTest: RematchReset setup '
    requested = 'BwayMatchUpgradeLifecycleTest: RematchReset requested production RestartMatchFromResults'
    verify = 'BwayMatchUpgradeLifecycleTest: RematchReset verify '
    pass = 'BwayMatchUpgradeLifecycleTest: RematchReset PASS '
    fail = 'BwayMatchUpgradeLifecycleTest: RematchReset FAIL '
    timeout = 'BwayMatchUpgradeLifecycleTest: TIMEOUT '
}
$counts = [ordered]@{}
$firstMatches = [ordered]@{}
foreach ($name in $markerPatterns.Keys) {
    $counts[$name] = 0
    $firstMatches[$name] = $null
}
$lineCount = 0
$fixtureErrorLines = 0
$unknownErrorLines = 0
$criticalLines = 0
$firstCausalError = $null
$criticalExamples = [System.Collections.Generic.List[object]]::new()

$item = Get-Item -LiteralPath $path -ErrorAction Stop
$reader = [System.IO.StreamReader]::new([System.IO.FileStream]::new(
    $item.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
    [System.IO.FileShare]::ReadWrite))
try {
    while ($null -ne ($line = $reader.ReadLine())) {
        $lineCount++
        foreach ($name in $markerPatterns.Keys) {
            if ($line -match $markerPatterns[$name]) {
                $counts[$name]++
                if ($null -eq $firstMatches[$name]) { $firstMatches[$name] = $line }
            }
        }
        if (($line -match 'BwayMatchUpgradeLifecycleTest:') -and (($line -match ': Error:') -or ($line -match 'RematchReset FAIL') -or ($line -match 'TIMEOUT'))) {
            $fixtureErrorLines++
            if ($null -eq $firstCausalError) { $firstCausalError = $line }
        } elseif ($line -match ': Error:') {
            $unknownErrorLines++
        }
        if ($line -match 'Ensure condition failed|Assertion failed|Fatal error|Accessed None') {
            $criticalLines++
            if ($criticalExamples.Count -lt $CriticalExampleCount) {
                $criticalExamples.Add([ordered]@{ line = $lineCount; text = $line })
            }
        }
    }
} finally {
    $reader.Dispose()
}

$setupBlocked = $counts.setup -eq 0 -and $fixtureErrorLines -gt 0
$status = if ($counts.pass -gt 0 -and $criticalLines -eq 0 -and $fixtureErrorLines -eq 0) {
    'passed-scope-limited'
} elseif ($setupBlocked) {
    'setup-blocked'
} else {
    'unverified'
}
$summary = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    log = $item.FullName
    linesRead = $lineCount
    status = $status
    scope = 'Authority-side development fixture in same-process PIE; proves a completed PostMatch production rematch creates fresh player state with purchased upgrades, match stats, and wallet cleared. It does not prove packaged LAN, second-PC transport, or physical UI input.'
    markerCounts = $counts
    firstMatches = $firstMatches
    setupBlocked = $setupBlocked
    fixtureErrorLines = $fixtureErrorLines
    unknownErrorLines = $unknownErrorLines
    criticalLines = $criticalLines
    firstCausalError = $firstCausalError
    criticalExamples = @($criticalExamples.ToArray())
}

if ($OutputJson) {
    $outputPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $root $OutputJson }
    $outputDirectory = Split-Path -Parent $outputPath
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    $summary.outputJson = [System.IO.Path]::GetFullPath($outputPath)
    $summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $outputPath -Encoding UTF8
}

$summary | ConvertTo-Json -Depth 12 -Compress
