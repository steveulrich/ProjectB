[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunJson,
    [Parameter(Mandatory)][string]$CatalogJson,
    [Parameter(Mandatory)][string]$OutputJson,
    [switch]$LifeLeechLoadout
)
$ErrorActionPreference='Stop'
$run=Get-Content -LiteralPath $RunJson -Raw | ConvertFrom-Json
$catalog=Get-Content -LiteralPath $CatalogJson -Raw | ConvertFrom-Json
$ids=@('AttackStrength','Armor','MoveSpeed','CooldownReduction')
$attributes=@('attack','armor','move','cooldown')
if ($LifeLeechLoadout) { $ids[3]='LifeLeech'; $attributes[3]='leech' }
$definitions=@{}
foreach($id in $ids){
    $entries=@($catalog.after | Where-Object id -eq $id)
    if($entries.Count -ne 1 -or !$entries[0].ranks.Count){throw "Frozen catalog lacks $id"}
    $definitions[$id]=$entries[0].ranks[0]
}
$failures=[Collections.Generic.List[string]]::new()
function Require([bool]$Condition,[string]$Reason){if(!$Condition){$failures.Add($Reason)}}
function Fields($match){
    $row=[ordered]@{offset=$match.Index}
    foreach($token in ($match.Groups[1].Value.Trim() -split ' ')){
        $pair=$token -split '=',2
        if($pair.Count -eq 2){$row[$pair[0]]=$pair[1]}
    }
    [pscustomobject]$row
}
function Markers([string]$text,[string]$name){
    @([regex]::Matches($text,'LogTemp: Display: BwayMatchUpgradeLifecycleTest: '+[regex]::Escape($name)+' ([^\r\n]+)') | ForEach-Object {Fields $_})
}
Require ($run.status -eq 'passed-transport-scope' -and $run.hostReady -and $run.clientExitZero -and $run.allConnectedAlive -and !$run.runError) 'Transport/live checkpoint did not pass.'
Require (@($run.processes).Count -eq 4 -and (@($run.processes.role | Sort-Object) -join ',') -eq 'client1,client2,client3,host') 'Expected one host and three clients.'
$peers=@(); $purchases=@(); $destination=$null; $firstLogError=$null; $errorCount=0
foreach($process in $run.processes){
    Require (@($process.args).Count -gt 0 -and !(@($process.args) -match '(?i)^-nullrhi$')) "$($process.role): rendered launch arguments missing or NullRHI requested."
    $raw=Get-Content -LiteralPath $process.log -Raw
    Require ($raw -match 'LogRHI: Loading RHI module D3D12RHI') "$($process.role): rendered D3D12 initialization was not observed."
    $errors=@([regex]::Matches($raw,'(?m)^.*(?:\bError:|Ensure condition failed|Assertion failed|Fatal error|Accessed None|BwayMatchUpgradeLifecycleTest:.*(?:TIMEOUT| FAIL)).*$'))
    $errorCount+=$errors.Count
    if(!$firstLogError -and $errors.Count){$firstLogError=$errors[0].Value.Trim()}
    Require ($raw -match 'LogNet: PktLag set to 200') "$($process.role): missing observed 200ms latency."
    $samples=@(Markers $raw 'RematchAttributes')
    Require ($samples.Count -eq 8) "$($process.role): expected eight attribute rows."
    $before=@(Markers $raw 'ClientBefore')
    $reset=@(Markers $raw 'ClientReset PASS')
    $next=@(Markers $raw 'ClientNextMatch PASS')
    $localBefore=$null; $localAfter=$null
    $purchaseBoundary=-1; $nextLowerBoundary=-1; $nextUpperBoundary=-1
    if($process.role -eq 'host'){
        $purchases=@(Markers $raw 'RematchResetSeed purchase')
        $serverReset=@(Markers $raw 'RematchReset PASS')
        $serverNext=@(Markers $raw 'HostNextMatch PASS')
        $travel=[regex]::Match($raw,'LogTemp: BwayGameState: Rematch ServerTravel to ([^?\s]+)')
        $selectionStart=[regex]::Match($raw,'LogTemp: BwayHeroSelectionPhaseComponent: Starting hero selection phase')
        $selectionTimeout=[regex]::Match($raw,'LogTemp: Warning: BwayHeroSelectionManager: Selection time expired')
        $selectionEnd=[regex]::Match($raw,'LogTemp: BwayHeroSelectionPhaseComponent: Hero selection complete; travelling to match URL')
        if($travel.Success){$destination=$travel.Groups[1].Value}
        if($travel.Success){$purchaseBoundary=$travel.Index}
        if($serverReset.Count -eq 1){$nextLowerBoundary=$serverReset[0].offset}
        if($serverNext.Count -eq 1){$nextUpperBoundary=$serverNext[0].offset}
        Require ($serverReset.Count -eq 1 -and $serverNext.Count -eq 1) 'Host: missing unique reset/next-match pass.'
        Require ($raw -match 'LogTemp: Display: BwayMatchUpgradeLifecycleTest: RematchReset verify oldPlayers=4 newPlayers=4 invalid=0 nonzeroRanks=0 nonzeroStats=0 nonzeroWallets=0 ') 'Host: fresh roster ranks/stats/wallets did not clear.'
        $requested=[regex]::Match($raw,'LogTemp: Display: BwayMatchUpgradeLifecycleTest: RematchReset requested production RestartMatchFromResults')
        # RestartMatchFromResults emits ServerTravel synchronously before the fixture logs its return.
        Require ($requested.Success -and $travel.Success -and $travel.Index -lt $requested.Index) 'Host: missing production rematch travel/request-return sequence.'
        Require ($selectionStart.Success -and $selectionTimeout.Success -and $selectionEnd.Success -and $selectionStart.Index -gt $travel.Index -and $selectionTimeout.Index -gt $selectionStart.Index -and $selectionEnd.Index -gt $selectionTimeout.Index) 'Host: normal selection timeout/travel sequence missing.'
        if($serverNext.Count -eq 1){Require ($serverNext[0].phase -eq 'Playing' -and [int]$serverNext[0].players -eq 4 -and $serverNext[0].modifiers -eq '0' -and $serverNext[0].offset -gt $selectionEnd.Index) 'Host: invalid next-match checkpoint.'}
        if($purchases.Count -and $travel.Success){Require (($purchases.offset | Measure-Object -Maximum).Maximum -lt $travel.Index) 'Host: purchases did not precede rematch.'}
    } else {
        Require ($null -ne $process.exitCode -and $process.exitCode -eq 0) "$($process.role): client exit was not observed as zero."
        Require ($before.Count -eq 4 -and $reset.Count -eq 1 -and $next.Count -eq 1) "$($process.role): missing four-upgrade before/reset/next checkpoints."
        if($before.Count -eq 4 -and $reset.Count -eq 1 -and $next.Count -eq 1){
            $b=$before[0]; $a=$reset[0]; $n=$next[0]
            $localBefore=$b.player; $localAfter=$n.player
            $purchaseBoundary=$a.offset; $nextLowerBoundary=$a.offset; $nextUpperBoundary=$n.offset
            Require ((@($before.id | Sort-Object) -join ',') -eq (@($ids | Sort-Object) -join ',')) "$($process.role): wrong purchased upgrade set."
            foreach($item in $before){Require ($item.player -eq $b.player -and $item.world -eq $b.world -and [int]$item.rank -eq 1 -and [double]$item.gold -gt 0 -and $item.gold -eq $b.gold -and [int]$item.statsDamage -gt 0 -and $item.offset -lt $a.offset) "$($process.role): invalid purchased state."}
            Require ($a.oldPlayer -eq $b.player -and [int]$a.players -eq 4 -and $a.owned -eq '0' -and $a.nonzeroStats -eq '0' -and $a.nonzeroWallets -eq '0' -and [double]$a.settled -ge 2 -and $a.world -ne $b.world) "$($process.role): reset state did not clear."
            Require ($n.offset -gt $a.offset -and $n.owned -eq '0' -and $n.damage -eq '0' -and [double]$n.health -gt 0 -and $n.phase -eq 'Playing' -and $n.world -eq $b.world) "$($process.role): next match is not a fresh playable original arena."
        }
    }
    foreach($stage in @('Purchased','NextMatch')){
        $stageRows=@($samples | Where-Object stage -eq $stage)
        Require ($stageRows.Count -eq 4 -and @($stageRows.player | Sort-Object -Unique).Count -eq 4 -and @($stageRows.observer | Sort-Object -Unique).Count -eq 1) "$($process.role): incomplete $stage attribute roster."
        foreach($sample in $stageRows){
            if($stage -eq 'Purchased'){
                Require ($sample.offset -lt $purchaseBoundary) "$($process.role): purchased attribute sample did not precede reset."
            }else{
                Require ($sample.offset -gt $nextLowerBoundary -and $sample.offset -lt $nextUpperBoundary) "$($process.role): next-match attributes are outside the reset/next-match checkpoints."
            }
            Require ($sample.observer -in $stageRows.player) "$($process.role): observer absent from $stage roster."
            if ($LifeLeechLoadout) {
                Require ('cooldown' -in $sample.PSObject.Properties.Name -and 'cooldownBase' -in $sample.PSObject.Properties.Name -and [math]::Abs([double]$sample.cooldown-[double]$sample.cooldownBase) -le 0.001) "$($process.role): excluded cooldown modifier was present."
            }
            if($process.role -ne 'host'){Require ($sample.observer -eq $(if($stage -eq 'Purchased'){$localBefore}else{$localAfter})) "$($process.role): attribute observer differs from local player."}
            for($i=0;$i -lt 4;$i++){
                $field=$attributes[$i]; $baseField="${field}Base"
                $present=$field -in $sample.PSObject.Properties.Name -and $baseField -in $sample.PSObject.Properties.Name
                Require $present "$($process.role): missing $stage $field values."
                if(!$present){continue}
                $expected=[double]$sample.$baseField + $(if($stage -eq 'Purchased'){[double]$definitions[$ids[$i]].magnitude}else{0})
                $tolerance=if($i -ge 2){0.001}else{0.05}
                Require ([math]::Abs([double]$sample.$field-$expected) -le $tolerance) "$($process.role): $stage $field modifier mismatch player $($sample.player)."
            }
        }
    }
    $peers += [pscustomobject]@{role=$process.role;before=$before;reset=$reset;next=$next;samples=$samples;errors=$errors.Count;log=$process.log}
}
$hostPeer=@($peers | Where-Object role -eq host)
$purchasePlayers=@($purchases.player | Sort-Object -Unique)
Require ($purchases.Count -eq 16 -and $purchasePlayers.Count -eq 4) 'Expected sixteen purchases for four players.'
foreach($player in $purchasePlayers){
    $transactions=@($purchases | Where-Object player -eq $player)
    Require ((@($transactions.id | Sort-Object) -join ',') -eq (@($ids | Sort-Object) -join ',')) "Player ${player}: incomplete purchase set."
    foreach($p in $transactions){
        Require ($p.id -in $ids -and $p.rank -eq '1' -and 'cost' -in $p.PSObject.Properties.Name -and 'before' -in $p.PSObject.Properties.Name -and [int]$p.statsDamage -gt 0) "Player ${player}: missing transaction fields."
        if($p.id -notin $ids){continue}
        Require ([int]$p.cost -eq [int]$definitions[$p.id].cost -and [math]::Abs([double]$p.before-[double]$p.gold-[double]$p.cost) -le 0.01) "Player ${player}: wrong catalog price or debit."
    }
}
foreach($peer in $peers){
    foreach($sample in $peer.samples){
        $authority=if($hostPeer.Count -eq 1){@($hostPeer[0].samples | Where-Object {$_.stage -eq $sample.stage -and $_.player -eq $sample.player})}else{@()}
        Require ($authority.Count -eq 1) "$($peer.role): attribute roster differs from authority."
        if($authority.Count -eq 1){foreach($field in @($attributes | ForEach-Object {$_+'Base'})){Require ([math]::Abs([double]$sample.$field-[double]$authority[0].$field) -le 0.001) "$($peer.role): $field differs from authority."}}
        if($sample.stage -eq 'Purchased'){Require ($sample.player -in $purchasePlayers) "$($peer.role): purchase roster differs from attribute roster."}
    }
    if($peer.role -ne 'host' -and $peer.before.Count -eq 4 -and $peer.reset.Count -eq 1){
        $last=@($purchases | Where-Object player -eq $peer.before[0].player | Sort-Object offset | Select-Object -Last 1)
        Require ($last.Count -eq 1 -and [double]$peer.before[0].gold -ge [double]$last[0].gold -and [double]$peer.before[0].gold -le [double]$last[0].gold+20) "$($peer.role): wallet differs from completed authority purchases."
        Require ($destination -and $peer.reset[0].world.StartsWith($destination+'.')) "$($peer.role): reset destination differs from server travel."
    }
}
foreach($stage in @('Purchased','NextMatch')){Require (@($peers | ForEach-Object {$_.samples | Where-Object stage -eq $stage | Select-Object -ExpandProperty observer -Unique} | Sort-Object -Unique).Count -eq 4) "Four distinct $stage local owners were not observed."}
Require ($errorCount -eq 0) 'Runtime errors were recorded.'
$result=[ordered]@{passed=($failures.Count -eq 0);candidate=$run.candidate;scope='Four rendered packaged processes on one PC at 200ms; all-four-upgrade authority purchases, replicated before-state, production rematch, cleared fresh-world ranks/stats/wallets, normal selection timeout and next-match public modifiers. No physical controls, visual polish, client purchase-RPC or second-PC LAN proof.';purchaseCount=$purchases.Count;attributeRows=@($peers.samples).Count;processCount=$peers.Count;errorCount=$errorCount;firstCausalError=if($firstLogError){$firstLogError}else{$failures | Select-Object -First 1};failures=@($failures);purchases=$purchases;peers=$peers;destination=$destination;catalogJson=$CatalogJson;runJson=$RunJson;generatedUtc=[datetime]::UtcNow.ToString('o')}
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
[pscustomobject]$result | Select-Object passed,candidate,purchaseCount,attributeRows,processCount,errorCount,firstCausalError | ConvertTo-Json -Compress
