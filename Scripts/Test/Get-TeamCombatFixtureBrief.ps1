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
    pair = 'BwayTeamCombatTest: Pair '
    enemyDamage = 'BwayTeamCombatTest: EnemyDamage .*activated=1 .*changed=1'
    sameTeam = 'BwayTeamCombatTest: SameTeamDamage .*activated=1 .*rejected=1'
    passActivation = 'BwayTeamCombatTest: PassActivation .*activated=1 .*started=1'
	passReceive = 'BwayTeamCombatTest: PassReceive .*received=1 .*carrierMatches=1 .*sameTeam=1'
	complete = 'BwayTeamCombatTest: COMPLETE '
	peerConvergence = 'BwayTeamCombatTest: PeerConvergence COMPLETE .*worlds=4'
	peerConvergencePeer = 'BwayTeamCombatTest: PeerConvergencePeer '
	remotePair = 'BwayTeamCombatRemote: Setup '
	remoteDamage = 'BwayTeamCombatRemote: RemoteDamage .*changed=1'
	remotePassSetup = 'BwayTeamCombatRemote: PassSetup '
	remotePassReceive = 'BwayTeamCombatRemote: PassReceive .*received=1 .*carrierMatches=1 .*sameTeam=1'
	remoteComplete = 'BwayTeamCombatRemote: COMPLETE '
	remoteScoreSetup = 'BwayTeamCombatRemote: RemoteScoreSetup '
	remoteGoalOverlap = 'BwayTeamCombatRemote: RemoteGoalOverlap .*entered=1'
	remoteScore = 'BwayTeamCombatRemote: RemoteScore .*scoreAfter=.*objectiveAfter=.*credited=1'
	remoteScoreComplete = 'BwayTeamCombatRemote: RemoteScore COMPLETE '
}

$counts = [ordered]@{}
$firstMatches = [ordered]@{}
foreach ($name in $markerPatterns.Keys) {
    $counts[$name] = 0
    $firstMatches[$name] = $null
}
$lineCount = 0
$errorLines = 0
$criticalLines = 0
$firstCausalError = $null
$firstKnownNonFatalError = $null
$knownNonFatalErrorLines = 0
$unknownErrorLines = 0
$knownNonFatalErrorPatterns = @(
    'LogZenServiceInstance: Error: Failed to write security configuration',
    'LogPixelStreaming2RTC: Error: Cannot set target window - GEngine is not valid\.'
)
$criticalExamples = [System.Collections.Generic.List[object]]::new()
$naturalGoals = 0
$roundCompleteReplications = 0
$resultsScreensShown = 0

$item = Get-Item -LiteralPath $path -ErrorAction Stop
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
            if ($known) {
                $knownNonFatalErrorLines++
                if ($null -eq $firstKnownNonFatalError) { $firstKnownNonFatalError = $line }
            } else {
                $unknownErrorLines++
                if ($null -eq $firstCausalError) { $firstCausalError = $line }
            }
        }
        if ($line -match 'Ensure condition failed|Assertion failed|Fatal error|Accessed None') {
            $criticalLines++
            if ($criticalExamples.Count -lt $CriticalExampleCount) {
                $criticalExamples.Add([ordered]@{ line = $lineCount; text = $line })
            }
        }
        foreach ($name in $markerPatterns.Keys) {
            if ($line -match $markerPatterns[$name]) {
                $counts[$name]++
                if ($null -eq $firstMatches[$name]) {
                    $firstMatches[$name] = $line
                }
            }
        }
        if ($line -match 'BwayRoundManagement: Goal!') { $naturalGoals++ }
        if ($line -match 'Round state replicated to: RoundComplete') { $roundCompleteReplications++ }
        if ($line -match 'Results screen shown') { $resultsScreensShown++ }
    }
} finally {
    $reader.Dispose()
}

$remoteMode = $counts.remotePair -gt 0
$remoteScoreMode = $counts.remoteScoreSetup -gt 0
$required = if ($remoteScoreMode) {
	@('remotePair', 'remoteDamage', 'remotePassSetup', 'remotePassReceive', 'remoteScoreSetup', 'remoteGoalOverlap', 'remoteScore', 'remoteScoreComplete')
} elseif ($remoteMode) {
	@('remotePair', 'remoteDamage', 'remotePassSetup', 'remotePassReceive', 'remoteComplete')
} else {
	@('enemyDamage', 'sameTeam', 'passActivation', 'passReceive', 'complete')
}
$missing = @($required | Where-Object { $counts[$_] -lt 1 })
$summary = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    log = $item.FullName
    linesRead = $lineCount
    status = if ($missing.Count -eq 0 -and $criticalLines -eq 0) { 'passed-scope-limited' } else { 'unverified' }
	scope = if ($remoteScoreMode) {
		'Authority-side inventory for the separate-process remote-owner score fixture; proves a connected remote client drove production damage and relic-pass input, the intended remote teammate received the relic, the real opposing goal overlap awarded team and player objective score once, and the fixture restored transforms. It does not prove physical input or packaged LAN.'
	} elseif ($remoteMode) {
		'Authority-side inventory for the separate-process remote-owner fixture; proves a connected remote client drove production damage and relic-pass input, the intended teammate received the relic on the authority, and the fixture restored transforms. It does not prove same-team rejection, natural scoring, physical input, or packaged LAN.'
	} else {
		'Authority-only development fixture in four-peer PIE; proves real primary/melee enemy damage, same-team rejection, native relic pass, teammate receive, and (when present) four-world peer health/team convergence. It does not prove separate-process client transport, physical input, adverse latency, or packaged LAN.'
	}
	markerCounts = $counts
	missingMarkers = $missing
	fixtureMode = if ($remoteScoreMode) { 'remote-owner-score' } elseif ($remoteMode) { 'remote-owner' } else { 'authority' }
    firstMatches = $firstMatches
    naturalGoals = $naturalGoals
    roundCompleteReplications = $roundCompleteReplications
    resultsScreensShown = $resultsScreensShown
    errorLines = $errorLines
    knownNonFatalErrorLines = $knownNonFatalErrorLines
    unknownErrorLines = $unknownErrorLines
    criticalLines = $criticalLines
    firstCausalError = $firstCausalError
    firstKnownNonFatalError = $firstKnownNonFatalError
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
