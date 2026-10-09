[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string[]]$LogFiles = @(
        'Saved/Logs/ProjectB.log',
        'Saved/Logs/ProjectB_2.log',
        'Saved/Logs/ProjectB_3.log'
    ),

    [Parameter(Mandatory = $false)]
    [string]$ExperienceMarker = 'B_BW_Experience_Dev',

    [Parameter(Mandatory = $false)]
    [string]$HostLog,

    [Parameter(Mandatory = $false)]
    [string]$HostExperienceMarker,

    [Parameter(Mandatory = $false)]
    [string]$HostMapMarker,

    [Parameter(Mandatory = $false)]
    [string]$HostRequiredMarker,

    [Parameter(Mandatory = $false)]
    [string]$RequiredMarker,

    [Parameter(Mandatory = $false)]
    [string]$BaselineJson,

    [Parameter(Mandatory = $false)]
    [ValidateRange(1, 900)]
    [int]$TimeoutSeconds = 90,

    [Parameter(Mandatory = $false)]
    [ValidateRange(100, 10000)]
    [int]$PollMilliseconds = 500,

    [Parameter(Mandatory = $false)]
    [string]$OutputJson
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$startedUtc = [DateTime]::UtcNow
$LogFiles = @(
    foreach ($entry in $LogFiles) {
        foreach ($path in ($entry -split ',')) {
            $trimmed = $path.Trim()
            if ($trimmed) { $trimmed }
        }
    }
)
$experiencePattern = "OnExperienceLoadComplete\(CurrentExperience = LyraExperienceDefinition:$([regex]::Escape($ExperienceMarker)), Client\)"
$loadingPattern = 'Experience still loading'
$hostState = $null
if ($HostLog) {
    if (-not $HostExperienceMarker) { $HostExperienceMarker = $ExperienceMarker }
    $hostState = [ordered]@{
        path = $HostLog
        exists = $false
        offset = [int64]0
        bytes = [int64]0
        ready = $false
        matchCount = 0
        mapMatchCount = 0
        mapReady = $false
        requiredMatchCount = 0
        requiredReady = $false
        loadingCount = 0
        readErrors = 0
    }
}
$states = @(
    foreach ($path in $LogFiles) {
        [ordered]@{
            path = $path
            exists = $false
            offset = [int64]0
            bytes = [int64]0
            ready = $false
            matchCount = 0
            requiredMatchCount = 0
            requiredReady = $false
            loadingCount = 0
            readErrors = 0
        }
    }
)

# A second checkpoint in the same PIE attempt must begin where the prior
# checkpoint ended. This prevents stale markers in Unreal's reused ProjectB*.log
# files from satisfying a later travel/experience expectation.
$baseline = $null
$resolvedBaselineJson = $null
if ($BaselineJson) {
    $resolvedBaselineJson = if ([System.IO.Path]::IsPathRooted($BaselineJson)) {
        [System.IO.Path]::GetFullPath($BaselineJson)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $root $BaselineJson))
    }
    if (-not (Test-Path -LiteralPath $resolvedBaselineJson -PathType Leaf)) {
        throw "Baseline readiness JSON does not exist: $resolvedBaselineJson"
    }
    $baseline = Get-Content -LiteralPath $resolvedBaselineJson -Raw | ConvertFrom-Json
    for ($i = 0; $i -lt $states.Count; $i++) {
        $prior = @($baseline.files | Where-Object { $_.path -eq $states[$i].path }) | Select-Object -First 1
        if ($prior -and [int64]$prior.bytes -gt 0) {
            $states[$i].offset = [int64]$prior.bytes
            $states[$i].bytes = [int64]$prior.bytes
        }
    }
    if ($hostState -and $baseline.host -and [int64]$baseline.host.bytes -gt 0) {
        $hostState.offset = [int64]$baseline.host.bytes
        $hostState.bytes = [int64]$baseline.host.bytes
    }
}

function Read-AppendedText {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int64]$Offset
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [ordered]@{ exists = $false; offset = $Offset; bytes = [int64]0; text = ''; error = $false }
    }

    $stream = $null
    $reader = $null
    try {
        $stream = [System.IO.File]::Open(
            (Resolve-Path -LiteralPath $Path).Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            [System.IO.FileShare]::ReadWrite)
        if ($stream.Length -lt $Offset) {
            $Offset = 0
        }
        $stream.Position = $Offset
        $reader = [System.IO.StreamReader]::new($stream, [System.Text.Encoding]::UTF8, $true, 4096, $true)
        $text = $reader.ReadToEnd()
        $newOffset = $stream.Length
        return [ordered]@{ exists = $true; offset = $newOffset; bytes = $newOffset; text = $text; error = $false }
    }
    catch {
        return [ordered]@{ exists = $true; offset = $Offset; bytes = [int64]0; text = ''; error = $true }
    }
    finally {
        if ($reader) { $reader.Dispose() }
        elseif ($stream) { $stream.Dispose() }
        if ($stream -and $reader) { $stream.Dispose() }
    }
}

$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$isReady = $false
do {
    for ($i = 0; $i -lt $states.Count; $i++) {
        $state = $states[$i]
        $read = Read-AppendedText -Path $state.path -Offset $state.offset
        $state.exists = $read.exists
        $state.offset = $read.offset
        if ($read.exists) { $state.bytes = $read.offset }
        if ($read.error) { $state.readErrors++ }
        if ($read.text) {
            $state.matchCount += [regex]::Matches($read.text, $experiencePattern).Count
            $state.loadingCount += [regex]::Matches($read.text, $loadingPattern).Count
            if ($state.matchCount -gt 0) { $state.ready = $true }
            if ($RequiredMarker) {
                $state.requiredMatchCount += [regex]::Matches($read.text, [regex]::Escape($RequiredMarker)).Count
                if ($state.requiredMatchCount -gt 0) { $state.requiredReady = $true }
            }
        }
    }

    if ($hostState) {
        $read = Read-AppendedText -Path $hostState.path -Offset $hostState.offset
        $hostState.exists = $read.exists
        $hostState.offset = $read.offset
        if ($read.exists) { $hostState.bytes = $read.offset }
        if ($read.error) { $hostState.readErrors++ }
        if ($read.text) {
            $hostPattern = "OnExperienceLoadComplete\(CurrentExperience = LyraExperienceDefinition:$([regex]::Escape($HostExperienceMarker)), Server\)"
            $hostState.matchCount += [regex]::Matches($read.text, $hostPattern).Count
            if ($HostMapMarker) {
                $hostState.mapMatchCount += [regex]::Matches($read.text, [regex]::Escape($HostMapMarker)).Count
                if ($hostState.mapMatchCount -gt 0) { $hostState.mapReady = $true }
            }
            if ($HostRequiredMarker) {
                $hostState.requiredMatchCount += [regex]::Matches($read.text, [regex]::Escape($HostRequiredMarker)).Count
                if ($hostState.requiredMatchCount -gt 0) { $hostState.requiredReady = $true }
            }
            $hostState.loadingCount += [regex]::Matches($read.text, $loadingPattern).Count
            if ($hostState.matchCount -gt 0) { $hostState.ready = $true }
        }
    }

    $isReady = $states.Count -gt 0 -and @($states | Where-Object { -not $_.ready }).Count -eq 0
    if ($RequiredMarker -and @($states | Where-Object { -not $_.requiredReady }).Count -gt 0) { $isReady = $false }
    if ($hostState -and -not $hostState.ready) { $isReady = $false }
    if ($hostState -and $HostMapMarker -and -not $hostState.mapReady) { $isReady = $false }
    if ($hostState -and $HostRequiredMarker -and -not $hostState.requiredReady) { $isReady = $false }
    if (-not $isReady -and [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds $PollMilliseconds
    }
} while (-not $isReady -and [DateTime]::UtcNow -lt $deadline)

$finishedUtc = [DateTime]::UtcNow
$result = [ordered]@{
    status = if ($isReady) { 'ready' } else { 'timeout' }
    expectedExperience = $ExperienceMarker
    hostExpectedExperience = if ($hostState) { $HostExperienceMarker } else { $null }
    hostExpectedMapMarker = if ($hostState -and $HostMapMarker) { $HostMapMarker } else { $null }
    hostRequiredMarker = if ($hostState -and $HostRequiredMarker) { $HostRequiredMarker } else { $null }
    requiredMarker = if ($RequiredMarker) { $RequiredMarker } else { $null }
    elapsedSeconds = [Math]::Round(($finishedUtc - $startedUtc).TotalSeconds, 2)
    startedUtc = $startedUtc.ToString('o')
    finishedUtc = $finishedUtc.ToString('o')
    baselineJson = $resolvedBaselineJson
    files = @(
        foreach ($state in $states) {
            [ordered]@{
                path = $state.path
                exists = $state.exists
                bytes = $state.bytes
                ready = $state.ready
                matchCount = $state.matchCount
                requiredMatchCount = $state.requiredMatchCount
                requiredReady = if ($RequiredMarker) { $state.requiredReady } else { $null }
                loadingCount = $state.loadingCount
                readErrors = $state.readErrors
            }
        }
    )
    host = if ($hostState) {
        [ordered]@{
            path = $hostState.path
            exists = $hostState.exists
            bytes = $hostState.bytes
            ready = $hostState.ready
            matchCount = $hostState.matchCount
            mapMatchCount = $hostState.mapMatchCount
            mapReady = if ($HostMapMarker) { $hostState.mapReady } else { $null }
            requiredMatchCount = $hostState.requiredMatchCount
            requiredReady = if ($HostRequiredMarker) { $hostState.requiredReady } else { $null }
            loadingCount = $hostState.loadingCount
            readErrors = $hostState.readErrors
        }
    } else { $null }
    qualityNote = 'Readiness only: this does not prove combat, replication settlement, physical input, or packaged LAN acceptance.'
}

$json = $result | ConvertTo-Json -Depth 6
if ($OutputJson) {
    $parent = Split-Path -Parent $OutputJson
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    Set-Content -LiteralPath $OutputJson -Value $json -Encoding UTF8
}
$json
