[CmdletBinding()]
param(
    [Parameter(Mandatory)][string[]]$LogFiles,
    # Counts are the routine output. Request an event tail only when chronology is needed.
    [ValidateRange(0, 30)][int]$LastEventCount = 0,
    [string]$OutputJson
)

$ErrorActionPreference = 'Stop'
$eventPatterns = [ordered]@{
    selectionStarts = 'Starting hero selection phase'
    duplicateControllersIgnored = 'Ignoring duplicate '
    selectionTravels = 'Hero selection complete; travelling to match URL'
    rematchRequests = 'Rematch ServerTravel to'
    postMatchArrivals = 'PostMatchArrival:'
    resultsShown = 'Results screen shown'
    matchPhaseChanges = 'BwayRoundManagement: Match phase ->'
    normalExits = 'LogExit: Exiting.'
}
$knownNonFatalErrorPatterns = @(
    'LogZenServiceInstance: Error: Failed to write security configuration'
)

$reports = foreach ($logFile in $LogFiles) {
    $path = (Get-Item -LiteralPath $logFile -ErrorAction Stop).FullName
    $counts = [ordered]@{}
    foreach ($name in $eventPatterns.Keys) { $counts[$name] = 0 }
    $events = [System.Collections.Generic.Queue[object]]::new()
    $criticalExamples = [System.Collections.Generic.List[object]]::new()
    $lineNumber = 0
    $errorLines = 0
    $knownNonFatalErrorLines = 0
    $unknownErrorLines = 0
    $firstUnknownError = $null
    $criticalLines = 0
    # Unreal keeps its log open for writing. Allow that existing writer while reading.
    $reader = [System.IO.StreamReader]::new([System.IO.FileStream]::new(
        $path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite))
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
        $excerpt = if ($line.Length -gt 260) { $line.Substring(0, 260) + ' [truncated]' } else { $line }
        if ($line -match 'Ensure condition failed|Assertion failed|Fatal error|Accessed None') {
            $criticalLines++
            if ($criticalExamples.Count -lt 3) { $criticalExamples.Add(@{line=$lineNumber; text=$excerpt}) }
        }
        foreach ($name in $eventPatterns.Keys) {
            if ($line.Contains($eventPatterns[$name])) {
                $counts[$name]++
                if ($LastEventCount -gt 0) {
                    $events.Enqueue(@{kind=$name; line=$lineNumber; text=$excerpt})
                    if ($events.Count -gt $LastEventCount) { [void]$events.Dequeue() }
                }
                break
            }
        }
      }
    } finally { $reader.Dispose() }
    [ordered]@{path=$path; linesRead=$lineNumber; counts=$counts; errorLines=$errorLines;
        knownNonFatalErrorLines=$knownNonFatalErrorLines; unknownErrorLines=$unknownErrorLines;
        firstUnknownError=$firstUnknownError; criticalLines=$criticalLines;
        firstCriticalLines=@($criticalExamples.ToArray()); recentEvents=@($events.ToArray())}
}

$result = [ordered]@{
    scope = 'Log inventory only; counts cover each whole file and do not establish UI, input, natural gameplay, or acceptance. Critical counts are matching lines, not distinct defects. Live files may continue growing.'
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    logs = @($reports)
}
if ($OutputJson) {
    $root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $outputPath = if ([System.IO.Path]::IsPathRooted($OutputJson)) { $OutputJson } else { Join-Path $root $OutputJson }
    $outputDirectory = Split-Path -Parent $outputPath
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    }
    $result.outputJson = [System.IO.Path]::GetFullPath($outputPath)
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outputPath -Encoding UTF8
}
$result | ConvertTo-Json -Depth 8 -Compress
