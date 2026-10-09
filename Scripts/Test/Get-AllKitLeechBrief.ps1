[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunJson,
    [Parameter(Mandatory)][string]$CatalogJson,
    [Parameter(Mandatory)][string]$OutputJson
)
$ErrorActionPreference='Stop'
$run=Get-Content -LiteralPath $RunJson -Raw | ConvertFrom-Json
$catalog=Get-Content -LiteralPath $CatalogJson -Raw | ConvertFrom-Json
$leech=@($catalog.after | Where-Object id -eq LifeLeech)
if ($leech.Count -ne 1 -or !$leech[0].ranks.Count) {throw 'Frozen life-leech catalog missing'}
$bonus=[double]$leech[0].ranks[0].magnitude
$heroes=@('Argus','Rawlins','Alona','Korryn')

$failures=[Collections.Generic.List[string]]::new()
function Require([bool]$Value,[string]$Reason) {if (!$Value) {$failures.Add($Reason)}}
function Near($A,$B,[double]$Tolerance=0.02) {return $null -ne $A -and $null -ne $B -and [math]::Abs([double]$A-[double]$B) -le $Tolerance}
function Rows([string]$Text,[string]$Name) {
    @([regex]::Matches($Text,'LogTemp: Display: BwayAllKitLeech: '+[regex]::Escape($Name)+' ([^\r\n]+)') | ForEach-Object {
        $r=[ordered]@{offset=$_.Index}
        foreach ($token in ($_.Groups[1].Value -split ' ')) {$pair=$token -split '=',2; if ($pair.Count -eq 2) {$r[$pair[0]]=$pair[1]}}
        [pscustomobject]$r
    })
}
Require ($run.status -eq 'passed-transport-scope' -and $run.hostReady -and $run.clientExitZero -and $run.allConnectedAlive -and !$run.runError) 'Transport and four-live checkpoint did not pass.'
Require (@($run.processes).Count -eq 4 -and (@($run.processes.role | Sort-Object) -join ',') -eq 'client1,client2,client3,host') 'Expected one host and three clients.'
Require (@($run.startupReadiness).Count -eq 3 -and @($run.startupReadiness | Where-Object {!$_.ready}).Count -eq 0) 'Independent client staging readiness missing.'
$peers=@(); $errorCount=0; $firstError=$null
foreach ($process in $run.processes) {
    $text=Get-Content -LiteralPath $process.log -Raw
    $errors=@([regex]::Matches($text,'(?m)^.*(?:\bError:|Ensure condition failed|Assertion failed|Fatal error|Accessed None|BwayAllKitLeech:.*FAIL).*$'))
    $errorCount+=$errors.Count
    if (!$firstError -and $errors.Count) {$firstError=$errors[0].Value.Trim()}
    Require ($text -match 'LogRHI: Loading RHI module D3D12RHI' -and !(@($process.args) -match '(?i)^-nullrhi$')) "$($process.role): rendered D3D12 proof missing."
    Require ($text -match 'LogNet: PktLag set to 200') "$($process.role): 200ms lag missing."
    $pass=@(Rows $text $(if ($process.role -eq 'host') {'Host PASS'} else {'Client PASS'}))
    $samples=@(Rows $text 'Peer'); $activations=@(Rows $text 'Activate'); $ended=@(Rows $text 'Ended')
    Require ($ended.Count -eq $(if ($process.role -eq 'host') {4} else {1}) -and @($ended | Where-Object {$_.active -ne '0'}).Count -eq 0) "$($process.role): primary termination proof missing."
    foreach ($activation in $activations) {
        $end=@($ended | Where-Object {$_.index -eq $activation.index -and $_.hero -eq $activation.hero -and $_.owner -eq $activation.owner -and $_.client -eq $activation.client -and $_.offset -gt $activation.offset})
        Require ($end.Count -eq 1) "$($process.role): owning primary did not end after activation."
    }
    Require ($pass.Count -eq 1 -and $pass[0].kits -eq '4') "$($process.role): unique four-kit pass missing."
    Require ($activations.Count -eq 1 -and $activations[0].activated -eq '1' -and $activations[0].client -eq $(if ($process.role -eq 'host') {'0'} else {'1'})) "$($process.role): owning-player activation did not pass."
    Require ($samples.Count -eq 32) "$($process.role): expected 32 peer rows."
    foreach ($index in 0..3) {
        foreach ($stage in @('Baseline','AfterDamage')) {
            $rows=@($samples | Where-Object {$_.index -eq "$index" -and $_.stage -eq $stage})
            Require ($rows.Count -eq 4 -and @($rows.player | Sort-Object -Unique).Count -eq 4 -and @($rows.observer | Sort-Object -Unique).Count -eq 1) "$($process.role): incomplete $index $stage roster."
            foreach ($row in $rows) {
                $owns=$process.role -eq 'host' -or $row.observer -eq $row.player
                Require ($row.hero -eq $heroes[$index] -and $row.observer -in $rows.player -and $row.source -in $rows.player -and $row.victim -in $rows.player -and $row.source -ne $row.victim) "$($process.role): wrong kit or player identity."
                Require ($row.rank -eq $(if ($owns) {'1'} else {'0'}) -and $row.owned -eq $(if ($owns) {'1'} else {'0'}) -and (Near $row.leech $bonus 0.0001)) "$($process.role): rank privacy or life-leech attribute mismatch."
                Require ([double]$row.health -gt 0 -and [double]$row.health -le [double]$row.maxHealth -and [int]$row.team -ge 0) "$($process.role): invalid living public state."
                if ($pass.Count -eq 1) {Require ($row.offset -lt $pass[0].offset) "$($process.role): sample after terminal pass."}
            }
        }
        $baseline=@($samples | Where-Object {$_.index -eq "$index" -and $_.stage -eq 'Baseline'})
        $after=@($samples | Where-Object {$_.index -eq "$index" -and $_.stage -eq 'AfterDamage'})
        if ($baseline.Count -eq 4 -and $after.Count -eq 4) {Require (($baseline.offset | Measure-Object -Maximum).Maximum -lt ($after.offset | Measure-Object -Minimum).Minimum) "$($process.role): checkpoint order wrong."}
    }
    if ($process.role -ne 'host') {Require ($null -ne $process.exitCode -and $process.exitCode -eq 0) "$($process.role): normal exit zero missing."}
    $peers += [pscustomobject]@{role=$process.role;samples=$samples;activations=$activations;ended=$ended;pass=$pass;plans=@(Rows $text 'Plan');results=@(Rows $text 'Result');damage=@(Rows $text 'Damage');heals=@(Rows $text 'Heal');selected=@(Rows $text 'Selected');setup=@(Rows $text 'Setup');cleanup=@(Rows $text 'Cleanup');log=$process.log}
}
$hosts=@($peers | Where-Object role -eq host)
Require (@($peers.activations.owner | Sort-Object -Unique).Count -eq 4 -and (@($peers.activations.hero | Sort-Object) -join ',') -eq 'Alona,Argus,Korryn,Rawlins') 'Four distinct owning activations/kits missing.'
if ($hosts.Count -eq 1) {
    $h=$hosts[0]
    Require ($h.selected.Count -eq 4 -and (@($h.selected.hero | Sort-Object) -join ',') -eq 'Alona,Argus,Korryn,Rawlins' -and @($h.selected.player | Sort-Object -Unique).Count -eq 4) 'Mixed production selection missing.'
    Require ($h.plans.Count -eq 4 -and $h.results.Count -eq 4 -and $h.setup.Count -eq 1 -and $h.cleanup.Count -eq 1) 'Host plans/results/setup/cleanup incomplete.'
    if ($h.setup.Count -eq 1 -and $h.cleanup.Count -eq 1 -and $h.pass.Count -eq 1) {Require ($h.setup[0].players -eq '4' -and [int]$h.setup[0].bases -gt 0 -and $h.cleanup[0].players -eq '4' -and $h.cleanup[0].bases -eq $h.setup[0].bases -and $h.cleanup[0].listeners -eq '0' -and $h.cleanup[0].offset -lt $h.pass[0].offset) 'Owned cleanup did not finish before host pass.'}
    foreach ($index in 0..3) {
        $plans=@($h.plans | Where-Object index -eq "$index"); $results=@($h.results | Where-Object index -eq "$index")
        $acts=@($peers.activations | Where-Object index -eq "$index")
        Require ($plans.Count -eq 1 -and $results.Count -eq 1 -and $acts.Count -eq 1) "Kit ${index}: unique plan/result/activation missing."
        if ($plans.Count -ne 1 -or $results.Count -ne 1 -or $acts.Count -ne 1) {continue}
        $p=$plans[0]; $r=$results[0]; $a=$acts[0]
        Require (@($h.ended | Where-Object {$_.index -eq "$index" -and $_.hero -eq $p.hero -and $_.owner -eq $p.source -and $_.client -eq '0' -and $_.offset -gt $p.offset -and $_.offset -lt $r.offset}).Count -eq 1) "Kit ${index}: authority primary termination missing."
        $damage=@($h.damage | Where-Object index -eq "$index"); $heals=@($h.heals | Where-Object index -eq "$index")
        $raw=[double]$p.base+[double]$p.scaling*[double]$p.attack
        $perHit=[math]::Max(0.0,$raw*(1.0-[double]$p.armor*0.01)*[double]$p.incoming)
        $total=$perHit*[int]$p.hits
        Require ($p.hero -eq $heroes[$index] -and $r.hero -eq $p.hero -and $a.hero -eq $p.hero -and $a.owner -eq $p.source -and $a.ability -eq $p.ability -and $p.config -and $p.config -match $heroes[$index]) "Kit ${index}: source ability/kit identity mismatch."
        Require ($r.source -eq $p.source -and $r.victim -eq $p.victim -and (Near $p.raw $raw) -and (Near $p.expectedDamage $total) -and (Near $p.bonus $bonus 0.0001) -and [int]$p.hits -gt 0) "Kit ${index}: configured damage calculation mismatch."
        Require ($damage.Count -eq [int]$p.hits -and $heals.Count -eq [int]$p.hits -and [int]$r.hits -eq $damage.Count -and [int]$r.heals -eq $heals.Count) "Kit ${index}: projectile/hit/heal count mismatch."
        foreach ($d in $damage) {Require ($d.source -eq $p.source -and $d.victim -eq $p.victim -and $d.hero -eq $p.hero -and (Near $d.amount $perHit) -and $d.offset -gt $p.offset -and $d.offset -lt $r.offset) "Kit ${index}: damage attribution, amount, or order mismatch."}
        foreach ($heal in $heals) {Require ($heal.source -eq $p.source -and $heal.target -eq $p.source -and $heal.hero -eq $p.hero -and (Near $heal.amount ($perHit*$bonus)) -and $heal.offset -gt $p.offset -and $heal.offset -lt $r.offset) "Kit ${index}: healing attribution, amount, or order mismatch."}
        Require ((Near $r.damage $total) -and (Near $r.healing ($total*$bonus)) -and (Near $r.sourceAfter ([double]$p.sourceBefore+$total*$bonus)) -and (Near $r.victimAfter ([double]$p.victimBefore-$total))) "Kit ${index}: actual aggregate damage/healing mismatch."
        foreach ($peer in $peers) {
            $rows=@($peer.samples | Where-Object index -eq "$index")
            foreach ($row in $rows) {
                $authority=@($h.samples | Where-Object {$_.index -eq $row.index -and $_.stage -eq $row.stage -and $_.player -eq $row.player})
                Require ($authority.Count -eq 1 -and $row.source -eq $p.source -and $row.victim -eq $p.victim) "$($peer.role): kit $index roster differs from authority."
                if ($authority.Count -eq 1) {foreach ($field in @('health','maxHealth','leech','armor','incoming')) {Require (Near $row.$field $authority[0].$field) "$($peer.role): kit $index $field differs from authority."}; Require ($row.team -eq $authority[0].team) "$($peer.role): team differs from authority."}
                $expected=[double]$row.maxHealth
                if ($row.player -eq $p.source) {$expected=[double]$row.maxHealth*0.4; if ($row.stage -eq 'AfterDamage') {$expected+=$total*$bonus}}
                if ($row.player -eq $p.victim) {$expected=[double]$row.maxHealth*0.8; if ($row.stage -eq 'AfterDamage') {$expected-=$total}}
                Require (Near $row.health $expected) "$($peer.role): kit $index health differs from independent expected result."
            }
            $s=@($rows | Where-Object {$_.stage -eq 'Baseline' -and $_.player -eq $p.source}); $v=@($rows | Where-Object {$_.stage -eq 'Baseline' -and $_.player -eq $p.victim})
            Require ($s.Count -eq 1 -and $v.Count -eq 1 -and $s[0].team -ne $v[0].team -and (Near $s[0].health $p.sourceBefore) -and (Near $v[0].health $p.victimBefore)) "$($peer.role): wrong opposing-team baseline."
            $own=@($peer.activations | Where-Object index -eq "$index")
            if ($own.Count -eq 1 -and $s.Count -eq 1) {Require ($own[0].owner -eq $s[0].observer -and $own[0].offset -gt $s[0].offset -and $own[0].offset -lt @($rows | Where-Object {$_.stage -eq 'AfterDamage' -and $_.player -eq $p.source})[0].offset) "$($peer.role): activation not between owned baseline and damage result."}
        }
    }
}
Require ($errorCount -eq 0) 'Runtime errors recorded.'
$result=[ordered]@{passed=($failures.Count -eq 0);candidate=$run.candidate;scope='Mixed-roster production selection and four granted primary activations (host melee and three remote predicted requests), real melee/projectile player hits, damage/healing attribution, exact configured hit counts and 128 all-peer health rows at 200ms. No physical input, visual polish, other abilities, final-alpha eligibility or second-PC proof.';processCount=$peers.Count;activationCount=@($peers.activations).Count;peerRows=@($peers.samples).Count;errorCount=$errorCount;firstCausalError=if($firstError){$firstError}else{$failures | Select-Object -First 1};failures=@($failures);peers=$peers;runJson=$RunJson;catalogJson=$CatalogJson;generatedUtc=[datetime]::UtcNow.ToString('o')}
$result | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
[pscustomobject]$result | Select-Object passed,candidate,processCount,activationCount,peerRows,errorCount,firstCausalError | ConvertTo-Json -Compress
