[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunJson,[Parameter(Mandatory)][string]$CatalogJson,[Parameter(Mandatory)][string]$OutputJson)
$ErrorActionPreference='Stop'
$run=Get-Content -LiteralPath $RunJson -Raw | ConvertFrom-Json
$catalog=Get-Content -LiteralPath $CatalogJson -Raw | ConvertFrom-Json
$definition=@($catalog.after | Where-Object id -eq 'MoveSpeed')
if($definition.Count -ne 1 -or !$definition[0].ranks.Count) {throw 'Frozen catalog must contain one MoveSpeed definition'}
$cost=[int]$definition[0].ranks[0].cost
$bonus=[double]$definition[0].ranks[0].magnitude
$problems=[Collections.Generic.List[string]]::new()
$rows=@()
if($run.runError) {$problems.Add([string]$run.runError)}
if(!$run.hostReady -or !$run.allConnectedAlive -or !$run.clientExitZero) {$problems.Add('Missing ready/four-live/normal-client-exit proof')}
function Number($Groups,[string]$Name) { [double]::Parse($Groups[$Name].Value,[Globalization.CultureInfo]::InvariantCulture) }
foreach($process in $run.processes) {
    $log=if(Test-Path -LiteralPath $process.log) {Get-Content -LiteralPath $process.log -Raw} else {''}
    $errors=[regex]::Matches($log,'(?m)^.*(?:\bError:|Fatal error|Assertion failed|Ensure condition failed|Accessed None).*$')
    $sent=[regex]::Matches($log,'LogTemp: Display: BwayMovementUpgradeTest: Submitted player=(?<id>\d+) netmode=(?<mode>\d+) id=MoveSpeed before=(?<before>[\d.]+) cost=(?<cost>\d+) bonus=(?<bonus>[\d.]+)')
    $purchase=[regex]::Matches($log,'LogTemp: Display: BwayMovementUpgradeTest: Purchase PASS player=(?<id>\d+) netmode=(?<mode>\d+) rank=1 before=(?<before>[\d.]+) after=(?<after>[\d.]+) cost=(?<cost>\d+) tolerance=(?<tolerance>[\d.]+)')
    $peers=[regex]::Matches($log,'LogTemp: Display: BwayMovementUpgradeTest: Peer PASS player=(?<id>\d+) netmode=(?<mode>\d+) base=(?<base>[\d.]+) bonus=(?<bonus>[\d.]+) multiplier=(?<multiplier>[\d.]+) cap=(?<cap>[\d.]+) rankVisibility=1')
    $movement=[regex]::Matches($log,'LogTemp: Display: BwayMovementUpgradeTest: Movement player=(?<id>\d+) netmode=(?<mode>\d+) distance=(?<distance>[\d.]+) seconds=(?<seconds>[\d.]+)')
    $pass=[regex]::Matches($log,'LogTemp: Display: BwayMovementUpgradeTest: PASS peers=4 scope=replicated-speed-limit-native-movement-input')
    $fail=[regex]::Matches($log,'LogTemp: Error: BwayMovementUpgradeTest: FAIL[^\r\n]*')
    $mode=if($process.role -eq 'host') {'2'} else {'3'}
    $ok=$sent.Count -eq 1 -and $purchase.Count -eq 1 -and $peers.Count -eq 4 -and $movement.Count -eq 1 -and $pass.Count -eq 1 -and $fail.Count -eq 0
    $peerRows=@()
    foreach($peer in $peers) {
        $g=$peer.Groups
        $base=Number $g 'base'; $multiplier=Number $g 'multiplier'; $cap=Number $g 'cap'
        if($g['mode'].Value -ne $mode -or $base -le 0 -or [Math]::Abs((Number $g 'bonus')-$bonus) -gt 0.0001 -or [Math]::Abs($multiplier-(1+$bonus)) -gt 0.0001 -or [Math]::Abs($cap-$base*(1+$bonus)) -gt 0.02) {$ok=$false}
        $peerRows += [pscustomobject]@{player=[int]$g['id'].Value; base=$base; multiplier=$multiplier; cap=$cap}
    }
    if(@($peerRows.player | Sort-Object -Unique).Count -ne 4) {$ok=$false}
    $owner=$null
    if($sent.Count -eq 1 -and $purchase.Count -eq 1 -and $movement.Count -eq 1) {
        $s=$sent[0].Groups; $p=$purchase[0].Groups; $m=$movement[0].Groups
        $owner=[int]$s['id'].Value
        $before=Number $p 'before'; $after=Number $p 'after'; $tolerance=Number $p 'tolerance'
        if($s['mode'].Value -ne $mode -or $p['mode'].Value -ne $mode -or $m['mode'].Value -ne $mode -or $p['id'].Value -ne $s['id'].Value -or $m['id'].Value -ne $s['id'].Value -or $owner -notin $peerRows.player -or [int]$s['cost'].Value -ne $cost -or [int]$p['cost'].Value -ne $cost -or [Math]::Abs((Number $s 'bonus')-$bonus) -gt 0.0001 -or [Math]::Abs((Number $s 'before')-$before) -gt 0.01 -or $tolerance -lt 0 -or $tolerance -gt $cost -or $after -ge $before -or $after -lt $before-$cost-0.02 -or $after -gt $before-$cost+$tolerance+0.02 -or (Number $m 'distance') -le 10 -or (Number $m 'seconds') -lt 0.75 -or (Number $m 'seconds') -gt 60) {$ok=$false}
    }
    $latency=$log -match 'LogNet:.*PktLag set to 200'
    if(!$ok) {$problems.Add("$($process.role): " + $(if($fail.Count){$fail[0].Value}else{'missing or inconsistent movement checkpoints'}))}
    if(!$latency) {$problems.Add("$($process.role): missing runtime 200ms latency marker")}
    if($errors.Count) {$problems.Add("$($process.role): $($errors.Count) runtime errors")}
    if($process.role -ne 'host' -and $process.exitCode -ne 0) {$problems.Add("$($process.role): client exit not zero")}
    $rows += [pscustomobject]@{role=$process.role;owner=$owner;passed=$ok;peers=$peerRows;errorCount=$errors.Count;firstError=if($errors.Count){$errors[0].Value}else{$null};latencyConfirmed=$latency;exitCode=$process.exitCode;log=$process.log}
}
if(@($rows.role | Sort-Object -Unique).Count -ne 4 -or @($rows | Where-Object role -eq 'host').Count -ne 1 -or @($rows | Where-Object role -match '^client[123]$').Count -ne 3 -or @($rows.owner | Sort-Object -Unique).Count -ne 4) {$problems.Add('Expected one listen host and three distinct client owners')}
$cohort=@($rows.owner | Sort-Object)
foreach($row in $rows) {if((@($row.peers.player | Sort-Object) -join ',') -ne ($cohort -join ',')) {$problems.Add("$($row.role): peer cohort differs from the four owners")}}
$result=[pscustomobject]@{generatedUtc=[DateTime]::UtcNow.ToString('o');candidate=$run.candidate;passed=$problems.Count -eq 0;scope='Four rendered packaged processes, 200ms: production MoveSpeed purchase RPC, bounded wallet sanity, all 16 peer speed-cap/attribute observations and owner-private ranks, injected W movement. Exact debit/rollback/slow/replace/reset are separate automation evidence. No physical controls, slide physics, final tuning, or second-PC proof.';processCount=$rows.Count;peerObservationCount=($rows.peers | Measure-Object).Count;catalogCost=$cost;catalogBonus=$bonus;errorCount=($rows | Measure-Object errorCount -Sum).Sum;firstCausalError=if($problems.Count){$problems[0]}else{$null};problems=@($problems);processes=$rows;runJson=$RunJson;catalogJson=$CatalogJson;hostTeardown=$run.hostTeardown;outputJson=$OutputJson}
$result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
$result | Select-Object passed,candidate,processCount,peerObservationCount,errorCount,firstCausalError,outputJson | ConvertTo-Json -Compress
