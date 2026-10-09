[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string[]]$LogFiles = @(
        'Saved/Logs/ProjectB.log',
        'Saved/Logs/ProjectB_2.log',
        'Saved/Logs/ProjectB_3.log'
    ),

    [Parameter(Mandatory = $false)]
    [int]$EnemyId = 0,

    [Parameter(Mandatory = $false)]
    [ValidateRange(1, 32)]
    [int]$MinimumSamples = 1,

    [Parameter(Mandatory = $false)]
    [double]$ExpectedHealth = -1.0,

    [Parameter(Mandatory = $false)]
    [int]$ExpectedTeam = [int]::MinValue,

    [Parameter(Mandatory = $false)]
    [string]$AuthorityLog,

    [Parameter(Mandatory = $false)]
    [ValidateRange(0.0, 20.0)]
    [double]$HealthTolerance = 1.0,

    [Parameter(Mandatory = $false)]
    [string]$OutputJson
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$LogFiles = @(
    foreach ($entry in $LogFiles) {
        foreach ($path in ($entry -split ',')) {
            $trimmed = $path.Trim()
            if ($trimmed) { $trimmed }
        }
    }
)

$markerPattern = 'BwayTeamCombatTest: PeerConvergenceReplicated .*?(?:sample=(?<sample>\d+) )?enemy=(?<enemy>\d+) found=(?<found>[01]) health=(?<health>[0-9.]+) team=(?<team>-?\d+)'
$files = @()
$totalSamples = 0
$allFound = $LogFiles.Count -gt 0
$allFilesMeetMinimum = $LogFiles.Count -gt 0
$authoritySamples = @{}
$resolvedAuthorityLog = $null
if ($AuthorityLog) {
    $resolvedAuthorityLog = if ([System.IO.Path]::IsPathRooted($AuthorityLog)) { $AuthorityLog } else { Join-Path $root $AuthorityLog }
    if (Test-Path -LiteralPath $resolvedAuthorityLog -PathType Leaf) {
        $authorityPattern = 'BwayTeamCombatTest: PeerConvergenceAuthority .*?sample=(?<sample>\d+) enemy=(?<enemy>\d+) found=(?<found>[01]) health=(?<health>[0-9.]+) team=(?<team>-?\d+)'
        $authorityReader = [System.IO.StreamReader]::new([System.IO.FileStream]::new(
            $resolvedAuthorityLog, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::ReadWrite))
        try {
            while ($null -ne ($authorityLine = $authorityReader.ReadLine())) {
                $authorityMatch = [regex]::Match($authorityLine, $authorityPattern)
                if ($authorityMatch.Success) {
                    $authoritySamples[[int]$authorityMatch.Groups['sample'].Value] = [ordered]@{
                        enemyId = [int]$authorityMatch.Groups['enemy'].Value
                        found = $authorityMatch.Groups['found'].Value -eq '1'
                        health = [double]$authorityMatch.Groups['health'].Value
                        team = [int]$authorityMatch.Groups['team'].Value
                        text = $authorityLine
                    }
                }
            }
        } finally { $authorityReader.Dispose() }
    }
}

foreach ($name in $LogFiles) {
    $path = if ([System.IO.Path]::IsPathRooted($name)) { $name } else { Join-Path $root $name }
    $sampleList = [System.Collections.Generic.List[object]]::new()
    $lineCount = 0
    $markerCount = 0
    $errorLines = 0
    $knownNonFatalErrorLines = 0
    $unknownErrorLines = 0
    $criticalLines = 0
    $firstUnknownError = $null
    $sampleIds = [System.Collections.Generic.HashSet[int]]::new()
    $sampleValuesMatch = $true
    $authorityComparisonCount = 0
    $authorityMismatchCount = 0
    $exists = Test-Path -LiteralPath $path -PathType Leaf
    if ($exists) {
        $item = Get-Item -LiteralPath $path -ErrorAction Stop
        $reader = [System.IO.StreamReader]::new([System.IO.FileStream]::new(
            $item.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::ReadWrite))
        try {
            while ($null -ne ($line = $reader.ReadLine())) {
                $lineCount++
                if ($line -match ': Error:') {
                    $errorLines++
                    if ($line -match 'LogZenServiceInstance: Error: Failed to write security configuration|LogPixelStreaming2RTC: Error: Cannot set target window - GEngine is not valid\.') {
                        $knownNonFatalErrorLines++
                    } else {
                        $unknownErrorLines++
                        if ($null -eq $firstUnknownError) { $firstUnknownError = $line }
                    }
                }
                if ($line -match 'Ensure condition failed|Assertion failed|Fatal error|Accessed None') { $criticalLines++ }
                $match = [regex]::Match($line, $markerPattern)
                if (-not $match.Success) { continue }
                if ($EnemyId -gt 0 -and [int]$match.Groups['enemy'].Value -ne $EnemyId) { continue }
                $markerCount++
                $totalSamples++
                if ($match.Groups['found'].Value -eq '0') { $allFound = $false }
                if ($match.Groups['sample'].Success) {
                    [void]$sampleIds.Add([int]$match.Groups['sample'].Value)
                }
                $health = [double]$match.Groups['health'].Value
                $team = [int]$match.Groups['team'].Value
                if ($ExpectedHealth -ge 0.0 -and [math]::Abs($health - $ExpectedHealth) -gt 0.5) { $sampleValuesMatch = $false }
                if ($ExpectedTeam -ne [int]::MinValue -and $team -ne $ExpectedTeam) { $sampleValuesMatch = $false }
                if ($AuthorityLog) {
                    if (-not $match.Groups['sample'].Success -or -not $authoritySamples.ContainsKey([int]$match.Groups['sample'].Value)) {
                        $sampleValuesMatch = $false
                        $authorityMismatchCount++
                    } else {
                        $authorityComparisonCount++
                        $authority = $authoritySamples[[int]$match.Groups['sample'].Value]
                        $authorityMatches = (-not $authority.found) -or ($authority.enemyId -ne [int]$match.Groups['enemy'].Value) -or ([math]::Abs($health - $authority.health) -gt $HealthTolerance) -or ($team -ne $authority.team)
                        if ($authorityMatches) {
                            $sampleValuesMatch = $false
                            $authorityMismatchCount++
                        }
                    }
                }
                if ($sampleList.Count -lt 4) {
                    $sampleList.Add([ordered]@{
                        enemyId = [int]$match.Groups['enemy'].Value
                        found = $match.Groups['found'].Value -eq '1'
                        sample = if ($match.Groups['sample'].Success) { [int]$match.Groups['sample'].Value } else { $null }
                        health = $health
                        team = $team
                        text = $line
                    })
                }
            }
        } finally { $reader.Dispose() }
    } else {
        $allFound = $false
    }

    $sampleCoverage = if ($sampleIds.Count -gt 0) { $sampleIds.Count -ge $MinimumSamples } else { $markerCount -ge $MinimumSamples }
    if (-not $sampleCoverage) { $allFilesMeetMinimum = $false }
    if ($markerCount -eq 0) { $allFound = $false }
    if (-not $sampleValuesMatch) { $allFilesMeetMinimum = $false }
    $files += [ordered]@{
        path = [System.IO.Path]::GetFullPath($path)
        exists = $exists
        linesRead = $lineCount
        markerCount = $markerCount
        distinctSampleCount = $sampleIds.Count
        meetsMinimumSamples = $sampleCoverage
        sampleValuesMatch = $sampleValuesMatch
        authorityComparisonCount = $authorityComparisonCount
        authorityMismatchCount = $authorityMismatchCount
        authorityMatch = if ($AuthorityLog) { $authorityComparisonCount -gt 0 -and $authorityMismatchCount -eq 0 } else { $null }
        samples = @($sampleList.ToArray())
        errorLines = $errorLines
        knownNonFatalErrorLines = $knownNonFatalErrorLines
        unknownErrorLines = $unknownErrorLines
        criticalLines = $criticalLines
        firstUnknownError = $firstUnknownError
    }
}

$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    expectedEnemyId = if ($EnemyId -gt 0) { $EnemyId } else { $null }
    minimumSamples = $MinimumSamples
    expectedHealth = if ($ExpectedHealth -ge 0.0) { $ExpectedHealth } else { $null }
    expectedTeam = if ($ExpectedTeam -ne [int]::MinValue) { $ExpectedTeam } else { $null }
    authorityLog = if ($resolvedAuthorityLog) { [System.IO.Path]::GetFullPath($resolvedAuthorityLog) } else { $null }
    authoritySampleCount = $authoritySamples.Count
    healthTolerance = $HealthTolerance
    clientCount = $LogFiles.Count
    peerSampleCount = $totalSamples
    status = if ($allFound -and $allFilesMeetMinimum) { 'passed-scope-limited' } else { 'unverified' }
    files = @($files)
    scope = "Separate-process replicated GameState probe only; each child must provide at least $MinimumSamples matching sample(s). This does not prove full combat, adverse latency, physical input, or packaged LAN acceptance."
    proofGaps = @(
        'This marker samples only the replicated enemy health/team state; it does not prove every gameplay property or long-lived convergence.',
        'Physical input, adverse latency, and packaged LAN remain separate acceptance gates.'
    )
    qualityNote = 'Raw child logs remain the evidence source; this brief bounds only the marker inventory and preserves error/critical counts.'
}

$json = $result | ConvertTo-Json -Depth 8
if ($OutputJson) {
    $parent = Split-Path -Parent $OutputJson
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    Set-Content -LiteralPath $OutputJson -Value $json -Encoding UTF8
}
$json
