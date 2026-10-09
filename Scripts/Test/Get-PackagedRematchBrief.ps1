[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunJson,
    [Parameter(Mandatory)][string]$OutputJson,
    [switch]$RequireNextMatch
)
$ErrorActionPreference = 'Stop'
$run = Get-Content -Raw -LiteralPath $RunJson | ConvertFrom-Json
$rows = @()
$allPurchases = @()
$travelMap = $null
$serverPass = $false
$selectionFlow = $false
$firstError = $null
foreach ($process in $run.processes) {
    $raw = Get-Content -Raw -LiteralPath $process.log
    $errors = @([regex]::Matches($raw, '(?m)^.*(?:\bError:|Ensure condition failed|Assertion failed|Fatal error|Accessed None|BwayMatchUpgradeLifecycleTest:.*(?:TIMEOUT| FAIL)).*$'))
    if (-not $firstError -and $errors.Count) { $firstError = $errors[0].Value.Trim() }
    if ($process.role -eq 'host') {
        $allPurchases = @([regex]::Matches($raw, 'LogTemp: Display: BwayMatchUpgradeLifecycleTest: RematchResetSeed purchase player=(\d+) id=(\S+) rank=(\d+) gold=([\d.]+) statsDamage=(\d+)') | ForEach-Object {
            [pscustomobject]@{player=[int]$_.Groups[1].Value;id=$_.Groups[2].Value;rank=[int]$_.Groups[3].Value;gold=[double]$_.Groups[4].Value;damage=[int]$_.Groups[5].Value}
        })
        $travel = [regex]::Match($raw, 'LogTemp: BwayGameState: Rematch ServerTravel to ([^?\s]+)')
        if ($travel.Success) { $travelMap = $travel.Groups[1].Value }
        $serverPass = $raw -match 'LogTemp: Display: BwayMatchUpgradeLifecycleTest: RematchReset PASS '
        $serverVerify = $raw -match 'LogTemp: Display: BwayMatchUpgradeLifecycleTest: RematchReset verify oldPlayers=4 newPlayers=\d+ invalid=0 nonzeroRanks=0 nonzeroStats=0 nonzeroWallets=0 '
        $serverRequested = $raw -match 'LogTemp: Display: BwayMatchUpgradeLifecycleTest: RematchReset requested production RestartMatchFromResults'
        $serverSetup = $raw -match 'LogTemp: Display: BwayMatchUpgradeLifecycleTest: Seed purchases complete humans=4'
        $selectionStart = [regex]::Match($raw, 'LogTemp: BwayHeroSelectionPhaseComponent: Starting hero selection phase')
        $selectionEnd = [regex]::Match($raw, 'LogTemp: BwayHeroSelectionPhaseComponent: Hero selection complete; travelling to match URL')
        $selectionTimeout = [regex]::Match($raw, 'LogTemp: Warning: BwayHeroSelectionManager: Selection time expired')
        $selectionFlow = $selectionStart.Success -and $selectionEnd.Success -and $selectionTimeout.Success -and $travel.Success -and $selectionStart.Index -gt $travel.Index -and $selectionTimeout.Index -gt $selectionStart.Index -and $selectionEnd.Index -gt $selectionTimeout.Index
        $rows += [pscustomobject]@{role=$process.role;errors=$errors.Count;pass=($serverPass -and $serverVerify -and $serverRequested -and $serverSetup -and $travel.Success -and (-not $RequireNextMatch -or $selectionFlow));selectionFlow=$selectionFlow;log=$process.log;exitCode=$process.exitCode}
        continue
    }
    $before = @([regex]::Matches($raw, 'LogTemp: Display: BwayMatchUpgradeLifecycleTest: ClientBefore player=(\d+) id=(\S+) rank=(\d+) gold=([\d.]+) statsDamage=(\d+) world=(\S+)'))
    $after = @([regex]::Matches($raw, 'LogTemp: Display: BwayMatchUpgradeLifecycleTest: ClientReset PASS oldPlayer=(\d+) newPlayer=(\d+) players=(\d+) owned=0 nonzeroStats=0 nonzeroWallets=0 settled=([\d.]+) world=(\S+)'))
    $exitPattern = if ($RequireNextMatch) { 'LogExit: .*TestExit: BwayMatchUpgradeLifecycleTest: ClientNextMatch PASS' } else { 'LogExit: .*TestExit: BwayMatchUpgradeLifecycleTest: ClientReset PASS' }
    $exit = @([regex]::Matches($raw, $exitPattern)).Count
    $row = [ordered]@{role=$process.role;errors=$errors.Count;pass=$false;beforeCount=$before.Count;afterCount=$after.Count;testExitCount=$exit;exitCode=$process.exitCode;log=$process.log}
    if ($before.Count -eq 1 -and $after.Count -eq 1) {
        $b=$before[0]; $a=$after[0]
        $row.player=[int]$b.Groups[1].Value
        $row.id=$b.Groups[2].Value
        $row.rank=[int]$b.Groups[3].Value
        $row.gold=[double]$b.Groups[4].Value
        $row.damage=[int]$b.Groups[5].Value
        $row.beforeWorld=$b.Groups[6].Value
        $row.afterWorld=$a.Groups[5].Value
        $row.pass=$b.Index -lt $a.Index -and $row.player -eq [int]$a.Groups[1].Value -and $row.rank -eq 1 -and $row.gold -gt 0 -and $row.damage -gt 0 -and [int]$a.Groups[3].Value -ge 4 -and [double]$a.Groups[4].Value -ge 2 -and $row.beforeWorld -ne $row.afterWorld -and $exit -eq 1 -and $process.exitCode -eq 0
        if ($RequireNextMatch) {
            $next = @([regex]::Matches($raw, 'LogTemp: Display: BwayMatchUpgradeLifecycleTest: ClientNextMatch PASS player=(\d+) owned=0 damage=0 health=([\d.]+) phase=Playing world=(\S+)'))
            $row.nextMatchCount = $next.Count
            $nextPassed = $next.Count -eq 1
            if ($nextPassed) {
                $n = $next[0]
                $row.nextMatchWorld = $n.Groups[3].Value
                $row.nextMatchHealth = [double]$n.Groups[2].Value
                $row.nextMatchPlayer = [int]$n.Groups[1].Value
                $nextPassed = $n.Index -gt $a.Index -and $row.nextMatchHealth -gt 0 -and $row.nextMatchWorld -eq $row.beforeWorld
            }
            $row.pass = $row.pass -and $nextPassed
        }
    }
    $rows += [pscustomobject]$row
}
$clients = @($rows | Where-Object {$_.role -like 'client*'})
$purchasePlayers = @($allPurchases | Select-Object -ExpandProperty player -Unique)
$correlated = $clients.Count -eq 3 -and @($clients | Where-Object {$_.pass}).Count -eq 3
if ($correlated) {
    foreach ($client in $clients) {
        $purchase = @($allPurchases | Where-Object {$_.player -eq $client.player -and $_.id -eq $client.id -and $_.rank -eq $client.rank})
        # Passive gold can increase between the authority transaction and client sample.
        if ($purchase.Count -ne 1 -or $client.gold -lt $purchase[0].gold -or $client.gold -gt $purchase[0].gold + 20 -or -not $travelMap -or -not $client.afterWorld.StartsWith($travelMap + '.')) { $correlated = $false }
    }
    if (@($clients | Select-Object -ExpandProperty player -Unique).Count -ne 3) { $correlated = $false }
}
$passed = $run.status -eq 'passed-transport-scope' -and $rows.Count -eq 4 -and $purchasePlayers.Count -eq 4 -and $allPurchases.Count -eq 4 -and $correlated -and @($rows | Where-Object {-not $_.pass -or $_.errors -gt 0}).Count -eq 0
$result = [ordered]@{
    passed=$passed
    scope='Four packaged processes: authority-driven real purchases and production PostMatch/rematch; three owners observe purchased state then settled fresh-world upgrade/stat/wallet reset. Excludes physical shop/rematch input, frontend session discovery, second-PC, and visual acceptance.'
    candidate=$run.candidate
    purchaseCount=$allPurchases.Count
    clientCount=$clients.Count
    serverResetPassed=$serverPass
    clientStateCorrelated=$correlated
    nextMatchRequired=[bool]$RequireNextMatch
    normalSelectionTimeoutFlow=$selectionFlow
    walletSampleTolerance=20
    errorCount=($rows | Measure-Object errors -Sum).Sum
    firstCausalError=$firstError
    destination=$travelMap
    purchases=$allPurchases
    peers=$rows
    hostTeardown=$run.hostTeardown
    runJson=$RunJson
    generatedUtc=[DateTime]::UtcNow.ToString('o')
}
if ($RequireNextMatch) { $result.scope += ' Also requires ordered normal selection start/timeout/travel and all clients reaching a fresh playable original arena; physical hero selection remains unverified.' }
$result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
[pscustomobject]$result | Select-Object passed,candidate,purchaseCount,clientCount,serverResetPassed,clientStateCorrelated,errorCount,firstCausalError,destination | ConvertTo-Json -Compress
