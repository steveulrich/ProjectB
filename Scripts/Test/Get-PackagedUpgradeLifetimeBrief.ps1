[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunJson,
    [Parameter(Mandatory)][string]$OutputJson,
    [switch]$RequireAllFour,
    [switch]$LifeLeechLoadout,
    [string]$CatalogJson
)
$ErrorActionPreference = 'Stop'
if ($LifeLeechLoadout) { $RequireAllFour = $true }
$run = Get-Content -Raw -LiteralPath $RunJson | ConvertFrom-Json
$failures = [Collections.Generic.List[string]]::new()
function Require([bool]$Condition, [string]$Message) { if (!$Condition) { $failures.Add($Message) } }
function Fields([string]$Text) {
    $row = [ordered]@{}
    foreach ($item in ($Text.Trim() -split ' ')) {
        $pair = $item -split '=', 2
        if ($pair.Count -eq 2) { $row[$pair[0]] = $pair[1] }
    }
    return [pscustomobject]$row
}
$roles = @()
$purchases = @()
$victim = $null
$nativeDamage = $false
$errors = 0
$firstLogError = $null
$attributes = if($RequireAllFour){@('attack','armor','move','cooldown')}else{@('attack','armor')}
$upgradeIds = if($RequireAllFour){@('AttackStrength','Armor','MoveSpeed','CooldownReduction')}else{@('AttackStrength','Armor')}
if ($LifeLeechLoadout) { $attributes[3]='leech'; $upgradeIds[3]='LifeLeech' }
$catalogEntries=@{}
if($RequireAllFour){
    if(!$CatalogJson){throw 'Four-upgrade acceptance requires the frozen catalog report'}
    $catalog=Get-Content -LiteralPath $CatalogJson -Raw | ConvertFrom-Json
    foreach($id in $upgradeIds){
        $entry=@($catalog.after | Where-Object id -eq $id)
        if($entry.Count -ne 1 -or !$entry[0].ranks.Count){throw "Frozen catalog lacks $id"}
        $catalogEntries[$id]=$entry[0].ranks[0]
    }
    Require ([bool]$run.allConnectedAlive) 'Missing simultaneous four-live-process checkpoint.'
}
Require ($run.status -eq 'passed-transport-scope' -and $run.hostReady -and $run.clientExitZero -and !$run.runError) 'Transport harness did not pass.'
Require (@($run.processes).Count -eq 4 -and (@($run.processes.role | Sort-Object) -join ',') -eq 'client1,client2,client3,host') 'Expected exactly one host and three clients.'
foreach ($process in $run.processes) {
    $raw = Get-Content -Raw -LiteralPath $process.log
    if ($LifeLeechLoadout) {
        Require ($raw -match 'LogRHI: Loading RHI module D3D12RHI' -and !(@($process.args) -match '(?i)^-nullrhi$')) "$($process.role): rendered D3D12 proof missing."
    }
    Require ($raw -match 'LogNet: PktLag set to 200') "$($process.role): requested packet latency was not observed."
    $logErrors = @([regex]::Matches($raw, '(?m)^.*(?:\bError:|Ensure condition failed|Assertion failed|Fatal error|Accessed None).*$'))
    $errors += $logErrors.Count
    if (!$firstLogError -and $logErrors.Count) { $firstLogError = $logErrors[0].Value.Trim() }
    $samples = @([regex]::Matches($raw, 'LogTemp: Display: BwayUpgradeLifetimeTest: Sample ([^\r\n]+)') | ForEach-Object { Fields $_.Groups[1].Value })
    $passes = @([regex]::Matches($raw, 'LogTemp: Display: BwayUpgradeLifetimeTest: (Host|Client) PASS ([^\r\n]+)'))
    $ready = @([regex]::Matches($raw, 'LogTemp: Display: BwayUpgradeLifetimeTest: Ready role=(host|client) world=\S*L_BW_Dorado players=4 relics=1'))
    Require ($ready.Count -eq 1) "$($process.role): missing arena/roster/relic readiness."
    Require ($passes.Count -eq 1) "$($process.role): missing unique native pass."
    $expectedRole = if ($process.role -eq 'host') { 'host' } else { 'client' }
    Require ($ready.Count -eq 1 -and $ready[0].Groups[1].Value -eq $expectedRole) "$($process.role): readiness role differs from process ownership."
    Require ($passes.Count -eq 1 -and $passes[0].Groups[1].Value -eq $expectedRole) "$($process.role): terminal role differs from process ownership."
    $pass = if ($passes.Count -eq 1) { Fields $passes[0].Groups[2].Value } else { $null }
    $baseline = @($samples | Where-Object stage -eq 'Baseline')
    $ids = @($baseline.player | Sort-Object -Unique)
    Require ($samples.Count -eq 20 -and $ids.Count -eq 4) "$($process.role): incomplete checkpoint roster."
    Require ((@($samples.stage | Select-Object -Unique) -join ',') -eq 'Baseline,Purchased,Dead,Respawn,Final') "$($process.role): checkpoint sequence differs."
    Require ($pass -and [int]$pass.players -eq 4 -and [int]$pass.durationRestored -eq 1 -and [int]$pass.samples -gt 0) "$($process.role): invalid terminal metadata."
    $owner = @($baseline | Where-Object owner -eq '1')
    Require ($owner.Count -eq 1 -and $pass -and $owner[0].player -eq $pass.local) "$($process.role): local ownership mismatch."
    $isHost = $process.role -eq 'host'
    if ($isHost) {
        if ($pass) { $victim = $pass.victim }
        $damage = @([regex]::Matches($raw, 'LogTemp: Display: BwayUpgradeLifetimeTest: NativeDamage victim=(\d+) guard=0 health=0\.0'))
        $nativeDamage = $damage.Count -eq 1 -and $damage[0].Groups[1].Value -eq $victim
        $purchases = @([regex]::Matches($raw, 'LogTemp: Display: BwayUpgradeLifetimeTest: Purchase ([^\r\n]+)') | ForEach-Object { Fields $_.Groups[1].Value })
        Require ($pass -and $pass.victim -ne $pass.local) 'Authority did not select a remote victim.'
    } else {
        Require ($process.exitCode -eq 0 -and ($RequireAllFour -or $raw -match 'LogExit: .*TestExit: BwayUpgradeLifetimeTest: Client PASS')) "$($process.role): missing successful owned exit."
    }
    foreach ($id in $ids) {
        $before = @($baseline | Where-Object player -eq $id)
        if ($before.Count -ne 1) { $failures.Add("$($process.role): duplicate baseline player $id"); continue }
        Require ([int]$before[0].attackRank -eq 0 -and [int]$before[0].armorRank -eq 0) "$($process.role): ownership was not initially empty."
        foreach ($stage in @('Baseline','Purchased','Dead','Respawn','Final')) {
            $rows = @($samples | Where-Object { $_.player -eq $id -and $_.stage -eq $stage })
            if ($rows.Count -ne 1) { $failures.Add("$($process.role): missing/duplicate $stage player $id"); continue }
            $row = $rows[0]
            Require ($row.owner -eq $before[0].owner) "$($process.role): local ownership changed player $id."
            $expectedRank = if ($stage -ne 'Baseline' -and ($isHost -or $row.owner -eq '1')) { 1 } else { 0 }
            if ($LifeLeechLoadout) {
                Require ('owned' -in $row.PSObject.Properties.Name -and [int]$row.owned -eq 4*$expectedRank -and $row.cooldownRank -eq '0' -and [math]::Abs([double]$row.cooldown-[double]$before[0].cooldownBase) -le 0.001) "$($process.role): four-slot ownership or excluded cooldown changed."
            }
            foreach ($attribute in $attributes) {
                $baseName = "${attribute}Base"
                $bonusName = "${attribute}Bonus"
                $rankName = "${attribute}Rank"
                $required=@($attribute,$baseName,$bonusName,$rankName)
                $missing=@($required | Where-Object {$_ -notin $row.PSObject.Properties.Name -or $_ -notin $before[0].PSObject.Properties.Name})
                Require ($missing.Count -eq 0) "$($process.role): missing $attribute fields $stage player $id."
                if($missing.Count){continue}
                Require ([int]$row.$rankName -eq $expectedRank) "$($process.role): private $attribute rank mismatch $stage player $id."
                $expected = [double]$before[0].$baseName
                if ($stage -ne 'Baseline') { $expected += [double]$before[0].$bonusName }
                $tolerance=if($attribute -in @('move','cooldown','leech')){0.001}else{0.05}
                Require ([Math]::Abs([double]$row.$attribute - $expected) -le $tolerance) "$($process.role): $attribute mismatch $stage player $id."
                if($RequireAllFour){
                    $upgradeId=$upgradeIds[[array]::IndexOf($attributes,$attribute)]
                    Require ([Math]::Abs([double]$row.$bonusName-[double]$catalogEntries[$upgradeId].magnitude) -le $tolerance) "$($process.role): $attribute bonus differs from frozen catalog."
                }
            }
            if($RequireAllFour){
                Require ($row.asc -and $row.ps -and $row.asc -eq $before[0].asc -and $row.ps -eq $before[0].ps) "$($process.role): PlayerState/ASC identity changed player $id."
            }
            if ($stage -in @('Purchased','Respawn','Final')) {
                Require ([double]$row.settled -ge 2.0 -and [double]$row.health -gt 0) "$($process.role): unsettled/lifeless $stage player $id."
            }
        }
    }
    $roles += [pscustomobject]@{role=$process.role;pass=$pass;samples=$samples;baseline=$baseline;log=$process.log;errors=$logErrors.Count}
}
Require ($nativeDamage) 'Missing actual native remote damage outside the script guard.'
$purchaseTarget=4*$upgradeIds.Count
Require ($purchases.Count -eq $purchaseTarget -and @($purchases | ForEach-Object { "$($_.player):$($_.id)" } | Sort-Object -Unique).Count -eq $purchaseTarget) "Expected $purchaseTarget distinct purchases."
foreach ($purchase in $purchases) {
    Require ($purchase.id -in $upgradeIds -and [int]$purchase.rank -eq 1 -and [int]$purchase.cost -ge 0 -and [Math]::Abs([double]$purchase.before - [double]$purchase.after - [double]$purchase.cost) -le 0.01) 'Purchase debit differs from catalog cost.'
    if($RequireAllFour){Require ($catalogEntries.ContainsKey($purchase.id) -and [int]$purchase.cost -eq [int]$catalogEntries[$purchase.id].cost) 'Purchase cost differs from frozen catalog.'}
}
$hostRow = @($roles | Where-Object role -eq 'host')
$hostIds = if ($hostRow.Count -eq 1) { @($hostRow[0].baseline.player | Sort-Object) -join ',' } else { '' }
if ($hostRow.Count -eq 1) {
    foreach ($id in $hostRow[0].baseline.player) {
        foreach ($upgradeId in $upgradeIds) {
            Require (@($purchases | Where-Object { $_.player -eq $id -and $_.id -eq $upgradeId }).Count -eq 1) "Missing unique $upgradeId purchase for roster player $id."
        }
    }
}
Require (@($roles | ForEach-Object { $_.pass.local } | Sort-Object -Unique).Count -eq 4) 'Four distinct local human owners were not observed.'
foreach ($role in $roles) {
    Require ($role.pass -and $role.pass.victim -eq $victim -and (@($role.baseline.player | Sort-Object) -join ',') -eq $hostIds) "$($role.role): victim/roster differs across peers."
    foreach ($id in $role.baseline.player) {
        $localBaseline = @($role.baseline | Where-Object player -eq $id)
        $authorityBaseline = if ($hostRow.Count -eq 1) { @($hostRow[0].baseline | Where-Object player -eq $id) } else { @() }
        if ($localBaseline.Count -eq 1 -and $authorityBaseline.Count -eq 1) {
            foreach ($attribute in $attributes) {
                $tolerance=if($attribute -in @('move','cooldown','leech')){0.001}else{0.05}
                foreach($field in @("${attribute}Base","${attribute}Bonus")){
                    Require ([Math]::Abs([double]$localBaseline[0].$field - [double]$authorityBaseline[0].$field) -le $tolerance) "$($role.role): $field differs from authority player $id."
                }
            }
        }
        $p = @($role.samples | Where-Object { $_.stage -eq 'Purchased' -and $_.player -eq $id })
        $d = @($role.samples | Where-Object { $_.stage -eq 'Dead' -and $_.player -eq $id })
        $r = @($role.samples | Where-Object { $_.stage -eq 'Respawn' -and $_.player -eq $id })
        $f = @($role.samples | Where-Object { $_.stage -eq 'Final' -and $_.player -eq $id })
        if ($p.Count -ne 1 -or $d.Count -ne 1 -or $r.Count -ne 1 -or $f.Count -ne 1) { continue }
        $authorityFinal = if ($hostRow.Count -eq 1) { @($hostRow[0].samples | Where-Object { $_.stage -eq 'Final' -and $_.player -eq $id }) } else { @() }
        Require ($role.pass -and [int]$f[0].round -eq [int]$role.pass.round -and $authorityFinal.Count -eq 1 -and [int]$f[0].round -eq [int]$authorityFinal[0].round) "$($role.role): final round differs from terminal or authority checkpoint player $id."
        $deathDelta = if ($id -eq $victim) { 1 } else { 0 }
        Require ([int]$r[0].deaths -eq ([int]$p[0].deaths + $deathDelta) -and [int]$r[0].round -eq [int]$p[0].round -and [int]$f[0].round -gt [int]$r[0].round) "$($role.role): death or round lifecycle mismatch player $id."
        if ($id -eq $victim) { Require ([double]$d[0].health -eq 0 -and [int]$d[0].dying -eq 1 -and $p[0].pawn -eq $d[0].pawn -and $p[0].pawn -ne $r[0].pawn -and $r[0].pawn -ne 'None') "$($role.role): missing actual death/replacement pawn." }
    }
}
Require ($errors -eq 0) 'Runtime log contains errors.'
$result = [ordered]@{passed=($failures.Count -eq 0);candidate=$run.candidate;scope='Four packaged processes on one PC, 200 ms latency; authority purchases, owner-only ranks/public attributes, native remote death/respawn and production round persistence. No physical UI or second-PC LAN proof.';purchaseCount=$purchases.Count;processCount=$roles.Count;checkpointRows=@($roles | ForEach-Object { $_.samples }).Count;victim=$victim;errorCount=$errors;firstCausalError=if ($firstLogError) {$firstLogError} else {$failures | Select-Object -First 1};failures=@($failures);processes=$roles;runJson=[IO.Path]::GetFullPath($RunJson)}
$result.generatedUtc=[DateTime]::UtcNow.ToString('o')
$result.upgradeIds=$upgradeIds
$result.requireAllFour=[bool]$RequireAllFour
$result.lifeLeechLoadout=[bool]$LifeLeechLoadout
$result.catalogJson=$CatalogJson
$result.scope += ' Upgrade IDs: ' + ($upgradeIds -join ', ') + '.'
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $OutputJson -Encoding utf8
[pscustomobject]$result | Select-Object passed,candidate,purchaseCount,processCount,checkpointRows,victim,errorCount,firstCausalError | ConvertTo-Json -Compress
