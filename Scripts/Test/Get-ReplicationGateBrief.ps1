[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$HostLog,
    [Parameter(Mandatory)][string[]]$ChildLogs,
    [string[]]$ReadinessJson = @(),
    [int]$EnemyId = 0,
    [ValidateRange(1, 32)][int]$MinimumSamples = 3,
    [double]$ExpectedHealth = -1.0,
    [int]$ExpectedTeam = [int]::MinValue,
    [ValidateRange(0.0, 20.0)][double]$HealthTolerance = 1.0,
    [switch]$IncludeSamples,
    [string]$OutputJson
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Resolve-ProjectPath {
    param([Parameter(Mandatory)][string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
    return [System.IO.Path]::GetFullPath((Join-Path $root $Path))
}

function Read-JsonFile {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

$resolvedHostLog = Resolve-ProjectPath $HostLog
$resolvedChildLogs = @(
    foreach ($entry in $ChildLogs) {
        foreach ($path in ($entry -split ',')) {
            $trimmed = $path.Trim()
            if ($trimmed) { Resolve-ProjectPath $trimmed }
        }
    }
)
$resolvedReadiness = @(
    foreach ($entry in $ReadinessJson) {
        foreach ($path in ($entry -split ',')) {
            $trimmed = $path.Trim()
            if ($trimmed) { Resolve-ProjectPath $trimmed }
        }
    }
)

if (-not (Test-Path -LiteralPath $resolvedHostLog -PathType Leaf)) {
    throw "Host log does not exist: $resolvedHostLog"
}
if ($resolvedChildLogs.Count -eq 0) { throw 'At least one child log is required.' }

$outputPath = $null
if ($OutputJson) {
    $outputPath = Resolve-ProjectPath $OutputJson
} else {
    $outputPath = Join-Path (Split-Path -Parent $resolvedHostLog) 'replication-gate-brief.json'
}
$outputDirectory = Split-Path -Parent $outputPath
if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$hostStem = [System.IO.Path]::GetFileNameWithoutExtension($resolvedHostLog)
$fixturePath = Join-Path $outputDirectory "$hostStem-fixture-brief.json"
$peerPath = Join-Path $outputDirectory "$hostStem-peer-brief.json"

# Reuse the established parsers. Their full JSON records stay beside the composite
# record; only bounded projections enter the returned result.
$fixtureScript = Join-Path $PSScriptRoot 'Get-TeamCombatFixtureBrief.ps1'
$peerScript = Join-Path $PSScriptRoot 'Get-SeparateProcessPeerProbeBrief.ps1'
& $fixtureScript -LogFile $resolvedHostLog -OutputJson $fixturePath | Out-Null
$peerArgs = @{
    LogFiles = $resolvedChildLogs
    EnemyId = $EnemyId
    MinimumSamples = $MinimumSamples
    ExpectedHealth = $ExpectedHealth
    ExpectedTeam = $ExpectedTeam
    AuthorityLog = $resolvedHostLog
    HealthTolerance = $HealthTolerance
    OutputJson = $peerPath
}
& $peerScript @peerArgs | Out-Null

$fixture = Read-JsonFile $fixturePath
$peer = Read-JsonFile $peerPath
if ($null -eq $fixture -or $null -eq $peer) { throw 'A child evidence parser did not produce JSON.' }

$readiness = @(
    foreach ($path in $resolvedReadiness) {
        $record = Read-JsonFile $path
        if ($null -eq $record) {
            [ordered]@{ path = $path; exists = $false; status = 'missing' }
            continue
        }
        [ordered]@{
            path = $path
            exists = $true
            status = $record.status
            elapsedSeconds = $record.elapsedSeconds
            expectedExperience = $record.expectedExperience
            hostExpectedExperience = $record.hostExpectedExperience
            hostExpectedMapMarker = $record.hostExpectedMapMarker
            hostRequiredMarker = $record.hostRequiredMarker
            childCount = @($record.files).Count
            loadingCount = @($record.files | ForEach-Object { [int]$_.loadingCount } | Measure-Object -Sum).Sum
            requiredReady = $record.host.requiredReady
        }
    }
)

$projectedFiles = @(
    foreach ($file in @($peer.files)) {
        $projection = [ordered]@{
            path = $file.path
            exists = $file.exists
            markerCount = $file.markerCount
            distinctSampleCount = $file.distinctSampleCount
            meetsMinimumSamples = $file.meetsMinimumSamples
            sampleValuesMatch = $file.sampleValuesMatch
            authorityComparisonCount = $file.authorityComparisonCount
            authorityMismatchCount = $file.authorityMismatchCount
            authorityMatch = $file.authorityMatch
            errorLines = $file.errorLines
            unknownErrorLines = $file.unknownErrorLines
            criticalLines = $file.criticalLines
            firstUnknownError = $file.firstUnknownError
        }
        if ($IncludeSamples) { $projection.samples = @($file.samples) }
        $projection
    }
)

$fixtureProjection = [ordered]@{
    status = $fixture.status
    fixtureMode = $fixture.fixtureMode
    markerCounts = $fixture.markerCounts
    missingMarkers = @($fixture.missingMarkers)
    naturalGoals = $fixture.naturalGoals
    roundCompleteReplications = $fixture.roundCompleteReplications
    resultsScreensShown = $fixture.resultsScreensShown
    errorLines = $fixture.errorLines
    knownNonFatalErrorLines = $fixture.knownNonFatalErrorLines
    unknownErrorLines = $fixture.unknownErrorLines
    criticalLines = $fixture.criticalLines
    firstCausalError = $fixture.firstCausalError
}
$peerProjection = [ordered]@{
    status = $peer.status
    clientCount = $peer.clientCount
    peerSampleCount = $peer.peerSampleCount
    authoritySampleCount = $peer.authoritySampleCount
    expectedEnemyId = $peer.expectedEnemyId
    minimumSamples = $peer.minimumSamples
    expectedHealth = $peer.expectedHealth
    expectedTeam = $peer.expectedTeam
    healthTolerance = $peer.healthTolerance
    files = $projectedFiles
}

$fixturePassed = $fixture.status -eq 'passed-scope-limited' -and $fixture.criticalLines -eq 0
$peerPassed = $peer.status -eq 'passed-scope-limited'
$readinessFailed = @($readiness | Where-Object { $_.status -ne 'ready' }).Count -gt 0
$status = if ($fixturePassed -and $peerPassed -and -not $readinessFailed) {
    'passed-scope-limited'
} else {
    'unverified'
}
$nextAction = if ($readinessFailed) {
    'Change the travel/readiness hypothesis before another fixture; inspect the persisted readiness and travel briefs, then relaunch only after child and host markers can settle together.'
} elseif (-not $fixturePassed) {
    'Inspect the persisted fixture brief for the first missing or causal marker before changing source or retrying.'
} elseif (-not $peerPassed) {
	'Inspect the persisted peer brief for sample coverage or authority mismatches before retrying; require a changed hypothesis.'
} else {
	if ($fixture.fixtureMode -eq 'remote-owner-score') {
		'Advance only to the next acceptance gate; this record still does not prove physical input, normal human selection, or packaged LAN.'
	} else {
		'Advance only to the next acceptance gate; this record still does not prove natural scoring, physical input, or packaged LAN.'
	}
}

$proofGaps = if ($fixture.fixtureMode -eq 'remote-owner-score') {
	@(
		'The remote score fixture uses an authoritative placement to enter the goal; physical input and normal human selection remain separate gates.',
		'Multi-sample GameState convergence does not prove every replicated property or long-lived convergence.',
		'Packaged LAN, frontend flow, all-kit actions, buildables/upgrades, UI, and performance remain separate gates.'
	)
} else {
	@(
		'The authority fixture does not prove natural goal/score ownership or full remote pass/scoring behavior.',
		'Multi-sample GameState convergence does not prove every replicated property or long-lived convergence.',
		'Physical input, adverse-latency breadth, packaged LAN, and normal human selection remain separate gates.'
	)
}

$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    status = $status
    scope = 'Composite separate-process replication inventory. It joins readiness, authority fixture, and multi-sample replicated GameState evidence without promoting any partial result to full LAN or gameplay acceptance.'
    hostLog = $resolvedHostLog
    childCount = $resolvedChildLogs.Count
    minimumSamples = $MinimumSamples
    readiness = $readiness
    fixture = $fixtureProjection
    peer = $peerProjection
    evidence = [ordered]@{
        readiness = $resolvedReadiness
        fixtureBrief = [System.IO.Path]::GetFullPath($fixturePath)
        peerBrief = [System.IO.Path]::GetFullPath($peerPath)
        hostLog = $resolvedHostLog
        childLogs = $resolvedChildLogs
    }
	proofGaps = $proofGaps
    nextAction = $nextAction
    qualityNote = 'Full raw logs and the two detailed helper JSON records remain on disk. The composite console projection is intentionally bounded.'
    outputJson = [System.IO.Path]::GetFullPath($outputPath)
}

$json = $result | ConvertTo-Json -Depth 12
Set-Content -LiteralPath $outputPath -Value $json -Encoding UTF8
$result | ConvertTo-Json -Depth 12 -Compress
