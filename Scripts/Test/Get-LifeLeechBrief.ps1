[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunJson,
    [Parameter(Mandatory)][string]$CatalogJson,
    [Parameter(Mandatory)][string]$OutputJson
)
$ErrorActionPreference = 'Stop'
$run = Get-Content -LiteralPath $RunJson -Raw | ConvertFrom-Json
$catalog = Get-Content -LiteralPath $CatalogJson -Raw | ConvertFrom-Json
$failures = [Collections.Generic.List[string]]::new()
function Require([bool]$Condition, [string]$Reason) { if (!$Condition) { $failures.Add($Reason) } }
function Near($A, $B, [double]$Tolerance = 0.02) {
    return $null -ne $A -and $null -ne $B -and [math]::Abs([double]$A - [double]$B) -le $Tolerance
}
function Markers([string]$Text, [string]$Name) {
    @([regex]::Matches($Text, 'LogTemp: Display: BwayLifeLeechTest: ' + [regex]::Escape($Name) + ' ([^\r\n]+)') | ForEach-Object {
        $row = [ordered]@{offset=$_.Index}
        foreach ($token in ($_.Groups[1].Value.Trim() -split ' ')) {
            $pair = $token -split '=',2
            if ($pair.Count -eq 2) { $row[$pair[0]] = $pair[1] }
        }
        [pscustomobject]$row
    })
}
$definitions = @($catalog.after | Where-Object id -eq LifeLeech)
Require ($definitions.Count -eq 1 -and $definitions[0].effectClass -eq '/Script/BreakawayCoreRuntime.BwayUpgradeEffect_LifeLeech' -and $definitions[0].ranks.Count -gt 0) 'Frozen native life-leech catalog definition missing.'
if ($definitions.Count -ne 1 -or !$definitions[0].ranks.Count) { throw 'Cannot evaluate without a frozen life-leech rank.' }
$bonus = [double]$definitions[0].ranks[0].magnitude
$cost = [int]$definitions[0].ranks[0].cost
Require ($bonus -gt 0 -and $bonus -le 1 -and $cost -gt 10) 'Unsupported catalog tuning for this fixture.'
Require ($run.status -eq 'passed-transport-scope' -and $run.hostReady -and $run.clientExitZero -and $run.allConnectedAlive -and !$run.runError) 'Transport/live checkpoint did not pass.'
Require (@($run.processes).Count -eq 4 -and (@($run.processes.role | Sort-Object) -join ',') -eq 'client1,client2,client3,host') 'Expected one host and three clients.'
Require (@($run.startupReadiness).Count -eq 3 -and @($run.startupReadiness | Where-Object {!$_.ready}).Count -eq 0 -and (@($run.startupReadiness.role | Sort-Object) -join ',') -eq 'client1,client2,client3') 'Independent child pawn readiness missing.'
$peers = @(); $errorCount = 0; $firstLogError = $null
foreach ($process in $run.processes) {
    $role = $process.role
    Require (@($process.args).Count -gt 0 -and !(@($process.args) -match '(?i)^-nullrhi$')) "${role}: rendered launch arguments missing or NullRHI requested."
    $raw = Get-Content -LiteralPath $process.log -Raw
    Require ($raw -match 'LogRHI: Loading RHI module D3D12RHI') "${role}: D3D12 initialization missing."
    Require ($raw -match 'LogNet: PktLag set to 200') "${role}: 200 ms latency not observed."
    $errors = @([regex]::Matches($raw, '(?m)^.*(?:\bError:|Ensure condition failed|Assertion failed|Fatal error|Accessed None|BwayLifeLeechTest:.*(?:TIMEOUT| FAIL)).*$'))
    $errorCount += $errors.Count
    if (!$firstLogError -and $errors.Count) { $firstLogError = $errors[0].Value.Trim() }
    $request = @(Markers $raw 'Request'); $purchase = @(Markers $raw 'Purchased')
    $samples = @(Markers $raw 'Peer'); $damage = @(Markers $raw 'Damage')
    $fund = @(Markers $raw 'Fund'); $cleanup = @(Markers $raw 'Cleanup')
    $pass = @(Markers $raw $(if ($role -eq 'host') {'Host PASS'} else {'Client PASS'}))
    Require ($request.Count -eq 1 -and $purchase.Count -eq 1 -and $pass.Count -eq 1) "${role}: unique owner purchase and pass markers missing."
    if ($request.Count -eq 1 -and $purchase.Count -eq 1 -and $pass.Count -eq 1) {
        $r=$request[0]; $p=$purchase[0]; $end=$pass[0]
        Require ($r.observer -eq $p.observer -and $r.observer -eq $end.observer -and $r.id -eq 'LifeLeech' -and $p.id -eq 'LifeLeech' -and $p.rank -eq '1') "${role}: owner/upgrade identity mismatch."
        Require ($r.client -eq $(if ($role -eq 'host') {'0'} else {'1'})) "${role}: incorrect owner RPC role."
        Require ([int]$r.cost -eq $cost -and [int]$p.cost -eq $cost -and (Near $r.bonus $bonus 0.0001) -and (Near $p.bonus $bonus 0.0001)) "${role}: price or bonus differs from frozen catalog."
        # A bounded ten-gold allowance covers passive income while independent replicated fields settle.
        # Exact authority debit is separately covered by economy automation.
        $debit = [double]$p.before - [double]$p.after
        Require ((Near $r.before $p.before) -and [double]$p.before -ge 450 -and $debit -ge $cost-10 -and $debit -le $cost+0.01) "${role}: wallet did not settle to the purchase debit."
        Require ($r.offset -lt $p.offset -and $p.offset -lt $end.offset -and [int]$end.samples -gt 0) "${role}: invalid purchase/pass ordering."
    }
    Require ($samples.Count -eq 8) "${role}: expected eight peer rows."
    foreach ($stage in @('Baseline','AfterDamage')) {
        $rows = @($samples | Where-Object stage -eq $stage)
        Require ($rows.Count -eq 4 -and @($rows.player | Sort-Object -Unique).Count -eq 4 -and @($rows.observer | Sort-Object -Unique).Count -eq 1) "${role}: incomplete $stage roster."
        foreach ($row in $rows) {
            Require (@('observer','player','source','victim','rank','owned','leech','health','maxHealth','armor','incoming','team' | Where-Object {$_ -notin $row.PSObject.Properties.Name}).Count -eq 0) "${role}: missing peer fields."
            $owns = $role -eq 'host' -or $row.player -eq $row.observer
            Require ($row.rank -eq $(if ($owns) {'1'} else {'0'}) -and $row.owned -eq $(if ($owns) {'1'} else {'0'})) "${role}: owner-only rank privacy failed."
            Require ((Near $row.leech $bonus 0.0001) -and [double]$row.health -gt 0 -and [double]$row.health -le [double]$row.maxHealth -and [int]$row.team -ge 0) "${role}: invalid public state."
            Require ($row.observer -in $rows.player -and $row.source -in $rows.player -and $row.victim -in $rows.player -and $row.source -ne $row.victim) "${role}: invalid actor roster."
            if ($pass.Count -eq 1 -and $purchase.Count -eq 1) {
                Require ($row.observer -eq $pass[0].observer -and $row.source -eq $pass[0].source -and $row.victim -eq $pass[0].victim -and $row.offset -gt $purchase[0].offset -and $row.offset -lt $pass[0].offset) "${role}: peer identity/ordering differs from owner checkpoints."
            }
        }
    }
    $baseline=@($samples | Where-Object stage -eq Baseline); $after=@($samples | Where-Object stage -eq AfterDamage)
    if ($baseline.Count -eq 4 -and $after.Count -eq 4) { Require (($baseline.offset | Measure-Object -Maximum).Maximum -lt ($after.offset | Measure-Object -Minimum).Minimum) "${role}: after-damage rows precede baseline." }
    if ($role -eq 'host') {
        Require ($fund.Count -eq 1 -and $damage.Count -eq 1 -and $cleanup.Count -eq 1) 'Host: setup, damage, or cleanup missing.'
        if ($fund.Count -eq 1 -and $cleanup.Count -eq 1 -and $pass.Count -eq 1 -and $request.Count -eq 1) {
            Require ($fund[0].players -eq '4' -and $fund[0].gold -eq '500' -and $fund[0].remoteSource -eq '1' -and [int]$fund[0].bases -gt 0 -and $cleanup[0].players -eq '4' -and $cleanup[0].bases -eq $fund[0].bases -and $cleanup[0].offset -gt $pass[0].offset -and $fund[0].offset -lt $request[0].offset) 'Host: incomplete owned fixture cleanup/setup.'
        }
        if ($damage.Count -eq 1 -and $baseline.Count -eq 4 -and $after.Count -eq 4) { Require ($damage[0].offset -gt ($baseline.offset | Measure-Object -Maximum).Maximum -and $damage[0].offset -lt ($after.offset | Measure-Object -Minimum).Minimum) 'Host: damage event outside baseline/after checkpoints.' }
    } else {
        Require ($null -ne $process.exitCode -and $process.exitCode -eq 0) "${role}: normal zero client exit missing."
        Require ($damage.Count -eq 0 -and $fund.Count -eq 0 -and $cleanup.Count -eq 0) "${role}: authority-only fixture operation on client."
    }
    $peers += [pscustomobject]@{role=$role;request=$request;purchase=$purchase;pass=$pass;samples=$samples;damage=$damage;fund=$fund;cleanup=$cleanup;log=$process.log}
}
$hosts = @($peers | Where-Object role -eq host)
Require (@($peers.pass.observer | Sort-Object -Unique).Count -eq 4) 'Four distinct local owners were not observed.'
if ($hosts.Count -eq 1 -and $hosts[0].damage.Count -eq 1 -and $hosts[0].pass.Count -eq 1) {
    $hostPeer = $hosts[0]; $d = $hostPeer.damage[0]
    $source = @($hostPeer.samples | Where-Object {$_.stage -eq 'Baseline' -and $_.player -eq $d.source})
    $victim = @($hostPeer.samples | Where-Object {$_.stage -eq 'Baseline' -and $_.player -eq $d.victim})
    Require ($source.Count -eq 1 -and $victim.Count -eq 1 -and $d.source -ne $hostPeer.pass[0].observer -and $d.source -in @($peers | Where-Object role -ne host).pass.observer) 'Source must be a remote owner in the baseline roster.'
    if ($source.Count -eq 1 -and $victim.Count -eq 1) {
        $s=$source[0]; $v=$victim[0]
        # A double zero prevents PowerShell selecting Math.Max(int,int) and
        # rounding fractional damage before the independent healing check.
        $expectedDamage=[math]::Min([double]$v.health, [math]::Max(0.0,121.25*(1-[double]$v.armor*0.01)*[double]$v.incoming))
        $actualDamage=[double]$d.victimBefore-[double]$d.victimAfter
        $actualHeal=[double]$d.sourceAfter-[double]$d.sourceBefore
        Require ($s.team -ne $v.team -and (Near $d.raw 121.25) -and (Near $d.bonus $bonus 0.0001) -and (Near $s.health ([double]$s.maxHealth*0.4)) -and (Near $v.health ([double]$v.maxHealth*0.6))) 'Wrong opposing-team fractional damage setup.'
        Require ((Near $d.sourceBefore $s.health) -and (Near $d.victimBefore $v.health) -and (Near $d.expectedDamage $expectedDamage) -and (Near $actualDamage $expectedDamage) -and (Near $actualHeal ($actualDamage*$bonus)) -and $actualDamage -gt 1) 'Actual damage/healing delta does not match the independent formula.'
        foreach ($peer in $peers) {
            foreach ($row in $peer.samples) {
                $authority=@($hostPeer.samples | Where-Object {$_.stage -eq $row.stage -and $_.player -eq $row.player})
                $base=@($hostPeer.samples | Where-Object {$_.stage -eq 'Baseline' -and $_.player -eq $row.player})
                Require ($authority.Count -eq 1 -and $base.Count -eq 1 -and $row.source -eq $d.source -and $row.victim -eq $d.victim) "$($peer.role): roster/actors differ from authority."
                if ($authority.Count -ne 1 -or $base.Count -ne 1) { continue }
                foreach ($field in @('health','maxHealth','leech','armor','incoming')) { Require (Near $row.$field $authority[0].$field) "$($peer.role): $field differs from authority." }
                Require ($row.team -eq $authority[0].team) "$($peer.role): team differs from authority."
                $expectedHealth=[double]$base[0].health
                if ($row.stage -eq 'AfterDamage') {
                    if ($row.player -eq $d.source) { $expectedHealth += $expectedDamage*$bonus }
                    if ($row.player -eq $d.victim) { $expectedHealth -= $expectedDamage }
                }
                Require (Near $row.health $expectedHealth) "$($peer.role): peer health differs from independent expected delta."
            }
        }
    }
}
Require ($errorCount -eq 0) 'Runtime errors were recorded.'
$result=[ordered]@{passed=($failures.Count -eq 0);candidate=$run.candidate;scope='Four rendered processes on one PC at 200ms: four owner purchases, private ranks/public attributes, one remote attacker healed from actual fractional enemy damage, and settled all-peer health. No physical input, all-kit eligibility, life-cycle reset, visual polish, or second-PC LAN proof.';purchaseCount=@($peers.purchase).Count;peerRows=@($peers.samples).Count;processCount=$peers.Count;errorCount=$errorCount;firstCausalError=if($firstLogError){$firstLogError}else{$failures | Select-Object -First 1};failures=@($failures);peers=$peers;catalogJson=$CatalogJson;runJson=$RunJson;generatedUtc=[datetime]::UtcNow.ToString('o')}
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
[pscustomobject]$result | Select-Object passed,candidate,purchaseCount,peerRows,processCount,errorCount,firstCausalError | ConvertTo-Json -Compress
