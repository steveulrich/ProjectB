[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputJson,
    [Parameter(Mandatory)][string]$OutputJson,
    [string]$ExpectedMap = 'L_BW_Dorado'
)
$ErrorActionPreference = 'Stop'
$run = Get-Content -Raw -LiteralPath $InputJson | ConvertFrom-Json
$failures = [Collections.Generic.List[string]]::new()
function Require([bool]$Condition, [string]$Message) {
    if (!$Condition) { $failures.Add($Message) }
}
function Get-StateKey($Sample) {
    return (@($Sample.peers | ForEach-Object { "$($_.world)|$($_.pawn)|$($_.phase)|$($_.round)|$($_.deaths)" }) -join ';')
}
$initial = @($run.initial.peers)
Require ($run.status -eq 'passed' -and @($run.errors).Count -eq 0) 'Observer did not pass cleanly.'
Require ($run.durationRestored -eq $true) 'Round duration cleanup is missing.'
Require ($initial.Count -eq 4 -and @($initial.world | Select-Object -Unique).Count -eq 4) 'Four distinct initial worlds are required.'
Require (@($initial | Where-Object { $_.world -notlike "*$ExpectedMap*" -or $_.relicCount -ne 1 }).Count -eq 0) 'Expected gameplay arena and one relic per peer.'
Require (@($initial | Where-Object authority).Count -eq 1 -and @($initial | Where-Object owner).Count -eq 1) 'Expected one authority and one remote owner.'
$requiredEvents = @('purchased','roundAfterPurchase','respawnPassed','postRespawnPlanning','finalRoundPassed')
$events = @($run.events)
Require (($events.event -join ',') -eq ($requiredEvents -join ',')) 'Required lifecycle event sequence is missing.'
$purchaseSettled = @($events | Where-Object event -eq 'roundAfterPurchase')
$checked = @($run.samples | Where-Object { $purchaseSettled.Count -eq 1 -and $_.t -ge $purchaseSettled[0].t })
Require ($checked.Count -gt 0) 'No samples after purchase convergence.'
$violations = 0
foreach ($sample in $checked) {
    if (@($sample.peers).Count -ne 4) { $violations++; continue }
    foreach ($peer in $sample.peers) {
        $baseline = @($initial | Where-Object world -eq $peer.world)
        if ($baseline.Count -ne 1 -or $peer.player -ne $baseline[0].player -or $peer.asc -ne $baseline[0].asc) { $violations++; continue }
        if (@($peer.attributes).Count -ne 2 -or ($peer.attributes.id -notcontains 'AttackStrength') -or ($peer.attributes.id -notcontains 'Armor')) { $violations++; continue }
        foreach ($attribute in $peer.attributes) {
            $before = @($baseline[0].attributes | Where-Object id -eq $attribute.id)
            $expectedRank = if ($peer.authority -or $peer.owner) { 1 } else { 0 }
            if ($before.Count -ne 1 -or $attribute.rank -ne $expectedRank -or
                [Math]::Abs($attribute.base - $before[0].base) -gt 0.05 -or
                [Math]::Abs($attribute.current - $before[0].base - $before[0].bonus) -gt 0.05) { $violations++ }
        }
    }
}
Require ($violations -eq 0) 'Rank visibility, owner lifetime, or attribute persistence mismatch.'
$deathWorlds = @($run.samples | ForEach-Object { $_.peers } | Where-Object { $null -ne $_.health -and $_.health -le 0 -and $_.dying } | Select-Object -ExpandProperty world -Unique)
Require ($deathWorlds.Count -eq 4) 'Actual death was not observed in all four worlds.'
foreach ($peer in @($run.afterRespawn.peers)) {
    $old = @($run.beforeDeath.peers | Where-Object world -eq $peer.world)
    Require ($old.Count -eq 1 -and $peer.pawn -and $peer.pawn -ne $old[0].pawn -and $peer.health -gt 0 -and !$peer.dying -and $peer.deaths -eq ($old[0].deaths + 1)) 'Replacement pawn/death-count proof failed.'
}
Require (@($run.afterRespawn.peers).Count -eq 4 -and @($run.final.peers).Count -eq 4) 'Missing respawn/final peer snapshot.'
$settled = @()
foreach ($name in @('roundAfterPurchase','respawnPassed','finalRoundPassed')) {
    $event = @($events | Where-Object event -eq $name)
    if ($event.Count -ne 1) { continue }
    $preceding = @($run.samples | Where-Object { $_.t -le $event[0].t })
    if (!$preceding.Count) { $failures.Add("No samples for $name"); continue }
    $last = $preceding[-1]
    $key = Get-StateKey $last
    $first = $last
    for ($index = $preceding.Count - 2; $index -ge 0; $index--) {
        if ((Get-StateKey $preceding[$index]) -ne $key) { break }
        $first = $preceding[$index]
    }
    $seconds = $last.t - $first.t
    Require ($seconds -ge 2.0) "Insufficient stable pawn/phase interval for $name"
    $settled += [pscustomobject]@{event=$name;seconds=[Math]::Round($seconds,3)}
}
foreach ($peer in @($run.final.peers)) {
    $old = @($run.afterRespawn.peers | Where-Object world -eq $peer.world)
    Require ($old.Count -eq 1 -and $peer.round -gt $old[0].round -and $peer.phase -match 'PLAYING' -and $peer.health -gt 0) 'Post-respawn production round advancement failed.'
}
$result = [ordered]@{
    passed=($failures.Count -eq 0); scope=$run.scope; expectedMap=$ExpectedMap; sampleCount=@($run.samples).Count
    checkedPostPurchaseSamples=$checked.Count; attributeViolations=$violations; deathWorldCount=$deathWorlds.Count
    settledIntervals=$settled; durationRestored=$run.durationRestored; firstCausalError=($failures | Select-Object -First 1)
    failures=@($failures); inputJson=[IO.Path]::GetFullPath($InputJson); outputJson=[IO.Path]::GetFullPath($OutputJson)
}
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputJson -Encoding utf8
$result | ConvertTo-Json -Depth 6 -Compress
