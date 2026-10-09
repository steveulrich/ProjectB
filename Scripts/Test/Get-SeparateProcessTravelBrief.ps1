[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string[]]$LogFiles,

    [Parameter(Mandatory = $false)]
    [string]$HostLog,

    [Parameter(Mandatory = $false)]
    [ValidateRange(0, 12)]
    [int]$LastEventCount = 4,

    [Parameter(Mandatory = $false)]
    [string]$OutputJson
)

$ErrorActionPreference = 'Stop'

# Accept either repeated arguments or one comma-separated argument. This keeps
# pwsh -File invocation reliable while preserving the native string[] shape.
$LogFiles = @(
    foreach ($entry in $LogFiles) {
        foreach ($path in ($entry -split ',')) {
            $trimmed = $path.Trim()
            if ($trimmed) { $trimmed }
        }
    }
)

$eventPatterns = [ordered]@{
    loadMap = 'LogLoad: LoadMap:'
    seamlessTravel = 'LogWorld: SeamlessTravel'
    travelCompleted = 'TravelCompleted'
    experienceStarts = 'StartExperienceLoad\(CurrentExperience'
    experienceCompletes = 'OnExperienceLoadComplete\(CurrentExperience'
    experienceStillLoading = 'Experience still loading'
    travelFailures = 'TravelFailure|NetworkFailure|Failed to load map|Browse failed'
}
$knownNonFatalErrorPatterns = @(
    'LogZenServiceInstance: Error: Failed to write security configuration',
    'LogPixelStreaming2RTC: Error: Cannot set target window - GEngine is not valid.'
)

function Read-TravelLog {
    param([Parameter(Mandatory = $true)][string]$Path)

    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    $counts = [ordered]@{}
    foreach ($name in $eventPatterns.Keys) { $counts[$name] = 0 }
    $recentEvents = [System.Collections.Generic.Queue[object]]::new()
    $lineNumber = 0
    $errorLines = 0
    $knownNonFatalErrorLines = 0
    $unknownErrorLines = 0
    $firstUnknownError = $null
    $reader = [System.IO.StreamReader]::new([System.IO.FileStream]::new(
        $item.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite))
    try {
        while ($null -ne ($line = $reader.ReadLine())) {
            $lineNumber++
            if ($line -match ': Error:') {
                $errorLines++
                $known = $false
                foreach ($knownPattern in $knownNonFatalErrorPatterns) {
                    if ($line -match $knownPattern) { $known = $true; break }
                }
                if ($known) { $knownNonFatalErrorLines++ }
                else {
                    $unknownErrorLines++
                    if ($null -eq $firstUnknownError) { $firstUnknownError = $line }
                }
            }
            foreach ($name in $eventPatterns.Keys) {
                if ($line -match $eventPatterns[$name]) {
                    $counts[$name]++
                    if ($LastEventCount -gt 0) {
                        $excerpt = if ($line.Length -gt 280) { $line.Substring(0, 280) + ' [truncated]' } else { $line }
                        $recentEvents.Enqueue([ordered]@{ kind = $name; line = $lineNumber; text = $excerpt })
                        if ($recentEvents.Count -gt $LastEventCount) { [void]$recentEvents.Dequeue() }
                    }
                    break
                }
            }
        }
    }
    finally { $reader.Dispose() }

    [ordered]@{
        path = $item.FullName
        bytes = [int64]$item.Length
        linesRead = $lineNumber
        lastWriteUtc = $item.LastWriteTimeUtc.ToString('o')
        counts = $counts
        errorLines = $errorLines
        knownNonFatalErrorLines = $knownNonFatalErrorLines
        unknownErrorLines = $unknownErrorLines
        firstUnknownError = $firstUnknownError
        recentEvents = @($recentEvents.ToArray())
    }
}

$children = @(
    foreach ($path in $LogFiles) { Read-TravelLog -Path $path }
)
$result = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    scope = 'Separate-process travel inventory only; compact per-log transition counts and recent markers do not establish gameplay, replication settlement, physical input, or packaged LAN acceptance.'
    childCount = $children.Count
    children = $children
}
if ($HostLog) {
    $result.host = Read-TravelLog -Path $HostLog
}

if ($OutputJson) {
    $root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $outputPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $root $OutputJson }
    $outputDirectory = Split-Path -Parent $outputPath
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    $result.outputJson = [System.IO.Path]::GetFullPath($outputPath)
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $outputPath -Encoding UTF8
}
$result | ConvertTo-Json -Depth 10 -Compress
