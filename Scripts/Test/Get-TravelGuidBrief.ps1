[CmdletBinding()]
param(
    # One or more Unreal logs (editor PIE log or packaged peer logs).
    [Parameter(Mandatory)][string[]]$LogFile,
    [string]$OutputJson,
    [int]$ExampleCount = 3
)

# Compact seamless-travel NetGUID + respawn-progression brief. Counts actual log lines (never inferred):
# read-only GUID reassignment warnings, invalid-GUID replication errors, VSM overflow lines, observer
# START/STOP windows, pins acquired outside an active window, sanitized entries, tracked drivers,
# resolved match-flow configuration, and scheduled respawn delays. Full logs stay on disk.

$ErrorActionPreference = 'Stop'
$patterns = [ordered]@{
    readOnlyGuid = 'Attempt to reassign read-only'
    invalidGuid = 'Invalid Net GUID|StartReplicating: Invalid|Invalid NetGUID'
    vsmOverflow = '\[VSM\]'
    seamlessTravel = 'LogWorld: SeamlessTravel to:'
    travelComplete = 'LogWorld: Bringing World .*L_BW_Dorado'
    errorLine = '(?<!Display): Error: |\]Log\w+: Error: '
    ensure = 'Ensure condition failed|LogOutputDevice: Error: === Handled ensure'
}
$logs = @()
foreach ($path in $LogFile) {
    $entry = [ordered]@{ log = $path; exists = (Test-Path -LiteralPath $path) }
    if (-not $entry.exists) { $logs += $entry; continue }
    $lines = [IO.File]::ReadAllLines($path)
    $entry.lineCount = $lines.Count
    foreach ($key in $patterns.Keys) {
        $hits = @($lines | Select-String -Pattern $patterns[$key])
        $entry["${key}Count"] = $hits.Count
        if ($hits.Count -gt 0 -and $key -in @('readOnlyGuid','invalidGuid','vsmOverflow','ensure')) {
            $entry["${key}Examples"] = @($hits | Select-Object -First $ExampleCount | ForEach-Object { $_.Line.Substring(0, [Math]::Min(300, $_.Line.Length)) })
        }
    }
    # Observer windows: a pin or sanitize line is only legal between START and STOP.
    $active = $false; $starts = 0; $stops = 0; $pinsOutside = 0; $sanitizeOutside = 0; $pinsInside = 0; $sanitized = 0
    $tracked = [System.Collections.Generic.List[string]]::new(); $stopLines = @(); $releases = 0; $arrivals = @()
    foreach ($line in $lines) {
        if ($line -match 'BwayTravelGuid: START ') { $active = $true; $starts++ }
        elseif ($line -match 'BwayTravelGuid: STOP ') { $active = $false; $stops++; $stopLines += $line.Substring($line.IndexOf('BwayTravelGuid:')) }
        elseif ($line -match 'BwayTravelGuid: event=pin-static .*pinned=(\d+)') { if ($active) { $pinsInside += [int]$Matches[1] } else { $pinsOutside += [int]$Matches[1] } }
        elseif ($line -match 'BwayTravelGuid: event=\S+ sanitized=1') { if ($active) { $sanitized++ } else { $sanitizeOutside++ } }
        elseif ($line -match 'BwayTravelGuid: event=track-driver reason=(\S+) driver=(\S+) world=(\S+) netMode=(\d+)') { $tracked.Add("$($Matches[1]) $($Matches[2]) netMode=$($Matches[4])") }
        elseif ($line -match 'BwayTravelGuid: event=release-pins') { $releases++ }
        elseif ($line -match 'BwayTravelGuid: event=destination-arrived .*allArrived=(\d)') { $arrivals += [int]$Matches[1] }
    }
    $entry.observer = [ordered]@{
        starts = $starts; stops = $stops; endedInactive = (-not $active); pinsInsideWindow = $pinsInside; pinsOutsideWindow = $pinsOutside
        sanitizedInsideWindow = $sanitized; sanitizeOutsideWindow = $sanitizeOutside; pinReleases = $releases
        trackedDrivers = @($tracked); destinationArrivals = $arrivals; stopLines = @($stopLines | Select-Object -First 4)
        hooksInstalled = @($lines | Select-String -Pattern 'BwayTravelGuid: hooks installed').Count
        hooksRemoved = @($lines | Select-String -Pattern 'BwayTravelGuid: hooks removed').Count
    }
    $entry.resolvedMatchFlow = @($lines | Select-String -Pattern "BwayMatchFlow: Resolved config '([^']+)'.*Respawn=([\d.]+)s\+([\d.]+)s/death\(max=([\d.]+)s,resetEachRound=(\w+)\)" | ForEach-Object {
        "$($_.Matches[0].Groups[1].Value) $($_.Matches[0].Groups[2].Value)/$($_.Matches[0].Groups[3].Value)/$($_.Matches[0].Groups[4].Value) reset=$($_.Matches[0].Groups[5].Value)" } | Select-Object -Unique)
    $entry.scheduledRespawns = @($lines | Select-String -Pattern 'BwayRoundManagement: Scheduled respawn for (.+?) \(round=(\d+) roundDeaths=(\d+) baseDelay=([\d.]+)s reduction=([\d.]+) effectiveDelay=([\d.]+)s remaining=([\d.]+)s\)' | ForEach-Object {
        $g = $_.Matches[0].Groups; "r$($g[2].Value) d$($g[3].Value) base=$($g[4].Value) red=$($g[5].Value) eff=$($g[6].Value) rem=$($g[7].Value) $($g[1].Value)" })
    $entry.quiesceLines = @($lines | Select-String -Pattern 'Quiesced \d+ outgoing PlayerStates|quiesce-outgoing-playerstates' | Select-Object -First 4 | ForEach-Object { $_.Line.Substring([Math]::Max(0, $_.Line.IndexOf('Log'))) })
    $logs += $entry
}
$sum = { param($k) ($logs | Where-Object { $_.exists } | ForEach-Object { $_[$k] } | Measure-Object -Sum).Sum }
$brief = [ordered]@{
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    logCount = $logs.Count
    missingLogs = @($logs | Where-Object { -not $_.exists } | ForEach-Object { $_.log })
    readOnlyGuidWarnings = & $sum 'readOnlyGuidCount'
    invalidNetGuidErrors = & $sum 'invalidGuidCount'
    vsmOverflowLines = & $sum 'vsmOverflowCount'
    seamlessTravelLines = & $sum 'seamlessTravelCount'
    pinsOutsideObserverWindow = ($logs | Where-Object { $_.exists } | ForEach-Object { $_.observer.pinsOutsideWindow } | Measure-Object -Sum).Sum
    sanitizeOutsideObserverWindow = ($logs | Where-Object { $_.exists } | ForEach-Object { $_.observer.sanitizeOutsideWindow } | Measure-Object -Sum).Sum
    logs = $logs
}
if ($OutputJson) { $brief | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputJson -Encoding UTF8 }
[ordered]@{
    logs = $brief.logCount; missing = $brief.missingLogs.Count; readOnly = $brief.readOnlyGuidWarnings; invalidGuid = $brief.invalidNetGuidErrors
    vsm = $brief.vsmOverflowLines; travels = $brief.seamlessTravelLines; pinsOutside = $brief.pinsOutsideObserverWindow; sanitizeOutside = $brief.sanitizeOutsideObserverWindow
    perLog = @($logs | Where-Object { $_.exists } | ForEach-Object { "$(Split-Path $_.log -Leaf): ro=$($_.readOnlyGuidCount) inv=$($_.invalidGuidCount) vsm=$($_.vsmOverflowCount) err=$($_.errorLineCount) start=$($_.observer.starts) stop=$($_.observer.stops) sanitized=$($_.observer.sanitizedInsideWindow) drivers=$($_.observer.trackedDrivers.Count) cfg=$($_.resolvedMatchFlow -join ';') respawns=$($_.scheduledRespawns.Count)" })
} | ConvertTo-Json -Depth 4
