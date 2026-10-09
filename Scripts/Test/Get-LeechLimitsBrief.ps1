[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunJson,[Parameter(Mandatory)][string]$CatalogJson,[Parameter(Mandatory)][string]$OutputJson)
$ErrorActionPreference='Stop'
$run=Get-Content -LiteralPath $RunJson -Raw | ConvertFrom-Json
$catalog=Get-Content -LiteralPath $CatalogJson -Raw | ConvertFrom-Json
$entry=@($catalog.after | Where-Object id -eq LifeLeech)
if ($entry.Count -ne 1 -or !$entry[0].ranks.Count) {throw 'Frozen leech catalog missing'}
$bonus=[double]$entry[0].ranks[0].magnitude
$failures=[Collections.Generic.List[string]]::new()
function Require([bool]$Condition,[string]$Reason) {if (!$Condition) {$failures.Add($Reason)}}
function Near($A,$B) {return $null -ne $A -and $null -ne $B -and [math]::Abs([double]$A-[double]$B) -le 0.002}
function Rows([string]$Text,[string]$Marker) {
    @([regex]::Matches($Text,'LogTemp: Display: BwayLeechLimits: '+[regex]::Escape($Marker)+' ([^\r\n]+)') | ForEach-Object {
        $r=[ordered]@{offset=$_.Index}
        foreach($token in ($_.Groups[1].Value -split ' ')) {$pair=$token -split '=',2; if($pair.Count -eq 2) {$r[$pair[0]]=$pair[1]}}
        [pscustomobject]$r
    })
}
Require ($run.status -eq 'passed-transport-scope' -and $run.hostReady -and $run.clientExitZero -and $run.allConnectedAlive -and !$run.runError) 'Transport/four-live checkpoint did not pass.'
Require (@($run.processes).Count -eq 4 -and (@($run.processes.role | Sort-Object) -join ',') -eq 'client1,client2,client3,host') 'Expected listen host and three clients.'
Require (@($run.startupReadiness).Count -eq 3 -and @($run.startupReadiness | Where-Object {!$_.ready}).Count -eq 0) 'Client readiness incomplete.'
$peers=@(); $errorCount=0; $peerRowCount=0; $firstError=$null
foreach($process in $run.processes) {
    $text=Get-Content -LiteralPath $process.log -Raw
    $errors=@([regex]::Matches($text,'(?m)^.*(?:\bError:|Ensure condition failed|Assertion failed|Fatal error|Accessed None|BwayLeechLimits:.*FAIL).*$'))
    $errorCount+=$errors.Count; if(!$firstError -and $errors.Count) {$firstError=$errors[0].Value.Trim()}
    Require ($text -match 'LogRHI: Loading RHI module D3D12RHI' -and !(@($process.args) -match '(?i)^-nullrhi$')) "$($process.role): rendered D3D12 missing."
    Require ($text -match 'LogNet: PktLag set to 200') "$($process.role): latency missing."
    $pass=@(Rows $text $(if($process.role -eq 'host') {'Host PASS'} else {'Client PASS'}))
    $samples=@(Rows $text 'Peer'); $teardown=@(Rows $text 'Teardown')
    $peerRowCount += $samples.Count
    Require ($pass.Count -eq 1 -and $pass[0].cases -eq '7' -and $pass[0].frames -eq '16' -and $samples.Count -eq 64) "$($process.role): complete case sequence missing."
    Require ($teardown.Count -eq 1 -and $teardown[0].finished -eq '1' -and $pass.Count -eq 1 -and $teardown[0].offset -gt $pass[0].offset) "$($process.role): fixture actor teardown missing."
    foreach($serial in 1..16) {
        $rows=@($samples | Where-Object serial -eq "$serial")
        Require ($rows.Count -eq 4 -and @($rows.player | Sort-Object -Unique).Count -eq 4 -and @($rows.observer | Sort-Object -Unique).Count -eq 1) "$($process.role): frame $serial roster incomplete."
        if($serial -gt 1) {
            $previous=@($samples | Where-Object serial -eq "$($serial-1)")
            if($rows.Count -and $previous.Count) {Require (($rows.offset | Measure-Object -Minimum).Minimum -gt ($previous.offset | Measure-Object -Maximum).Maximum) "$($process.role): frame order invalid."}
        }
    }
    if($process.role -ne 'host') {Require ($null -ne $process.exitCode -and $process.exitCode -eq 0 -and $text -match 'LogExit: Exiting.') "$($process.role): normal exit missing."}
    $peers += [pscustomobject]@{role=$process.role;samples=$samples;pass=$pass;plans=@(Rows $text 'Plan');results=@(Rows $text 'Result');damage=@(Rows $text 'Damage');heals=@(Rows $text 'Heal');saved=@(Rows $text 'Saved');restored=@(Rows $text 'Restored');setup=@(Rows $text 'Setup');cleanup=@(Rows $text 'Cleanup');respawn=@(Rows $text 'Respawn');deathSetup=@(Rows $text 'DeathSetup');teardown=$teardown;log=$process.log}
}
$hosts=@($peers | Where-Object role -eq host)
if($hosts.Count -eq 1) {
    $h=$hosts[0]
    Require ($h.setup.Count -eq 1 -and $h.cleanup.Count -eq 1 -and $h.plans.Count -eq 7 -and $h.results.Count -eq 7 -and $h.saved.Count -eq 4 -and $h.restored.Count -eq 4) 'Authority setup/plans/results/cleanup incomplete.'
    Require ($h.respawn.Count -eq 1 -and $h.respawn[0].players -eq '2' -and $h.respawn[0].natural -eq '1' -and $h.deathSetup.Count -eq 1 -and (Near $h.deathSetup[0].health 0)) 'Actual death and natural respawn evidence missing.'
    if($h.setup.Count -eq 1 -and $h.cleanup.Count -eq 1) {
        Require ($h.setup[0].remoteSource -eq '1' -and $h.setup[0].source -ne $h.pass[0].observer -and (Near $h.setup[0].bonus $bonus) -and (Near $h.setup[0].respawnDuring 20) -and (Near $h.setup[0].respawnBefore $h.cleanup[0].respawnRestored) -and $h.cleanup[0].players -eq '4' -and $h.cleanup[0].bases -eq $h.setup[0].bases -and [int]$h.setup[0].bases -gt 0 -and $h.cleanup[0].listeners -eq '0') 'Authority fixture configuration/cleanup mismatch.'
    }
    foreach($saved in $h.saved) {
        $restored=@($h.restored | Where-Object player -eq $saved.player)
        Require ($restored.Count -eq 1 -and (Near $restored[0].health $saved.health) -and (Near $restored[0].gold $saved.gold) -and (Near $restored[0].leech 0) -and $restored[0].owned -eq '0') 'Saved player health/wallet/upgrades not restored.'
    }
    $names=@('Cap','Full','Immune','Friendly','Self','Overkill','DeadSource')
    foreach($case in 0..6) {
        $plans=@($h.plans | Where-Object case -eq "$case"); $results=@($h.results | Where-Object case -eq "$case")
        Require ($plans.Count -eq 1 -and $results.Count -eq 1) "Case ${case}: plan/result missing."
        if($plans.Count -ne 1 -or $results.Count -ne 1) {continue}
        $p=$plans[0]; $r=$results[0]
        $damage=if($case -eq 2) {0.0} else {[math]::Min([double]$p.targetBefore,[math]::Max(0.0,[double]$p.raw*(1.0-[double]$p.armor*0.01)*[double]$p.incoming))}
        $heal=if($case -in @(0,5)) {[math]::Min([double]$p.sourceMax-[double]$p.sourceBefore,$damage*$bonus)} else {0.0}
        $events=@($h.damage | Where-Object case -eq "$case"); $heals=@($h.heals | Where-Object case -eq "$case")
        Require ($p.name -eq $names[$case] -and $r.name -eq $p.name -and $p.source -eq $h.setup[0].source -and $p.source -eq $r.source -and $p.target -eq $r.target -and (Near $p.bonus $bonus) -and (Near $p.raw 121.25) -and $p.serial -eq "$(2*$case+1)") "Case ${case}: identity/inputs mismatch."
        Require ((Near $r.damage $damage) -and (Near $r.healing $heal) -and $events.Count -eq $(if($damage -gt 0){1}else{0}) -and $heals.Count -eq $(if($heal -gt 0){1}else{0}) -and [int]$r.hits -eq $events.Count -and [int]$r.heals -eq $heals.Count -and $r.attempts -eq $(if($case -eq 5){'2'}else{'1'})) "Case ${case}: damage/heal counts or magnitudes wrong."
        foreach($e in $events) {Require ($e.source -eq $p.source -and $e.target -eq $p.target -and (Near $e.amount $damage) -and $e.offset -gt $p.offset -and $e.offset -lt $r.offset) "Case ${case}: damage attribution/order wrong."}
        foreach($e in $heals) {Require ($e.source -eq $p.source -and $e.target -eq $p.source -and (Near $e.amount $heal) -and $e.offset -gt $p.offset -and $e.offset -lt $r.offset) "Case ${case}: healing attribution/order wrong."}
        Require ((Near $p.sourceBefore $(if($case -eq 0){[double]$p.sourceMax-0.125}elseif($case -eq 1){[double]$p.sourceMax}elseif($case -eq 6){0.0}else{[double]$p.sourceMax*0.4})) -and ($case -ne 5 -or (Near $p.targetBefore 3))) "Case ${case}: boundary not established."
        if($case -eq 0) {Require ((Near $heal 0.125) -and $damage*$bonus -gt $heal) 'Cap was not exercised with excess healing.'}
        foreach($peer in $peers) {
            $rows=@($peer.samples | Where-Object {$_.case -eq "$case" -and [int]$_.stage -le 1})
            foreach($row in $rows) {
                $authority=@($h.samples | Where-Object {$_.serial -eq $row.serial -and $_.player -eq $row.player})
                $expected=0.6*[double]$row.max
                if($case -eq 6 -and $row.player -eq $h.setup[0].victim) {$expected=0}
                if($row.player -eq $p.source) {$expected=[double]$p.sourceBefore}
                if($row.player -eq $p.target) {$expected=[double]$p.targetBefore}
                if($row.stage -eq '1') {if($row.player -eq $p.target){$expected-=$damage}; if($row.player -eq $p.source){$expected+=$heal}}
                $expectedRank=if($row.player -eq $p.source -and ($peer.role -eq 'host' -or $row.observer -eq $row.player)) {'1'} else {'0'}
                $deathMatches=if($expected -eq 0) {$row.death -eq '1' -or $row.pawn -eq 'None'} else {$row.death -eq '0'}
                Require ($row.source -eq $p.source -and $row.target -eq $p.target -and [int]$row.serial -eq (2*$case+1+[int]$row.stage) -and (Near $row.health $expected) -and (Near $row.leech $(if($row.player -eq $p.source){$bonus}else{0.0})) -and $row.rank -eq $expectedRank -and $deathMatches) "$($peer.role): case $case actual health/privacy/death mismatch."
                Require ($authority.Count -eq 1 -and (Near $row.health $authority[0].health) -and (Near $row.max $authority[0].max) -and $row.team -eq $authority[0].team) "$($peer.role): case $case peer convergence missing."
            }
            $source=@($rows | Where-Object {$_.stage -eq '0' -and $_.player -eq $p.source}); $target=@($rows | Where-Object {$_.stage -eq '0' -and $_.player -eq $p.target})
            Require ($source.Count -eq 1 -and $target.Count -eq 1 -and (($source[0].team -eq $target[0].team) -eq ($case -in @(3,4))) -and (($p.source -eq $p.target) -eq ($case -eq 4))) "$($peer.role): case $case target relationship wrong."
            if($target.Count -eq 1 -and $case -notin @(4,5)) {Require (Near $p.targetBefore (0.6*[double]$target[0].max)) "$($peer.role): case $case living target baseline invalid."}
        }
    }
    foreach($peer in $peers) {
        foreach($row in @($peer.samples | Where-Object {[int]$_.stage -ge 2})) {
            $saved=@($h.saved | Where-Object player -eq $row.player)
            $expected=if($row.stage -eq '2') {[double]$row.max} elseif($saved.Count -eq 1) {[double]$saved[0].health} else {-1.0}
            Require ((Near $row.health $expected) -and $row.death -eq '0') "$($peer.role): respawn/cleanup health invalid."
            if($row.player -in @($h.setup[0].source,$h.setup[0].victim)) {Require ($row.pawn -ne 'None' -and $row.pawn -ne $row.original) "$($peer.role): original pawn reused after death."}
            if($row.stage -eq '3') {Require ((Near $row.leech 0) -and $row.rank -eq '0' -and $row.serial -eq '16') "$($peer.role): cleanup leech residue."}
            else {Require ($row.serial -eq '15' -and (Near $row.leech $(if($row.player -eq $h.setup[0].source){$bonus}else{0.0}))) "$($peer.role): respawn upgrade lost."}
        }
    }
}
Require ($errorCount -eq 0) 'Runtime errors recorded.'
$result=[ordered]@{passed=($failures.Count -eq 0);candidate=$run.candidate;scope='Seven leech boundary cases through production damage/health/message paths, remote player attribution, four rendered processes at 200ms, 256 peer rows, natural replacement pawns and cleanup. Delayed dead-source damage is a native fixture call, not an in-flight projectile; final-alpha policy, physical/visual and second-PC scopes remain separate.';processCount=$peers.Count;peerRows=$peerRowCount;errorCount=$errorCount;firstCausalError=if($firstError){$firstError}else{$failures | Select-Object -First 1};failures=@($failures);peers=$peers;runJson=$RunJson;catalogJson=$CatalogJson;generatedUtc=[datetime]::UtcNow.ToString('o')}
$result | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
[pscustomobject]$result | Select-Object passed,candidate,processCount,peerRows,errorCount,firstCausalError | ConvertTo-Json -Compress
