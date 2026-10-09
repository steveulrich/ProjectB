[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunJson,[Parameter(Mandatory)][string]$CatalogJson,[Parameter(Mandatory)][string]$KitJson,[Parameter(Mandatory)][string]$OutputJson)
$ErrorActionPreference='Stop'
$run=Get-Content -LiteralPath $RunJson -Raw | ConvertFrom-Json
$catalog=Get-Content -LiteralPath $CatalogJson -Raw | ConvertFrom-Json
$kit=Get-Content -LiteralPath $KitJson -Raw | ConvertFrom-Json
$entry=@($catalog.after | Where-Object id -eq 'CooldownReduction')
if($entry.Count -ne 1 -or !$entry[0].ranks.Count -or $kit.baseDuration -le 0){throw 'Missing frozen catalog or kit reference'}
$cost=[int]$entry[0].ranks[0].cost
$bonus=[double]$entry[0].ranks[0].magnitude
$base=[double]$kit.baseDuration
$expected=$base*(1-$bonus)
$problems=[Collections.Generic.List[string]]::new()
$rows=@()
function N($Groups,[string]$Name){[double]::Parse($Groups[$Name].Value,[Globalization.CultureInfo]::InvariantCulture)}
function Near([double]$A,[double]$B,[double]$Tolerance=0.01){[Math]::Abs($A-$B) -le $Tolerance}
if($run.runError){$problems.Add([string]$run.runError)}
if(!$run.hostReady -or !$run.allConnectedAlive -or !$run.clientExitZero){$problems.Add('Missing host readiness, four-live checkpoint, or normal client exits')}
$hostLog=''
foreach($process in $run.processes){
    $log=if(Test-Path -LiteralPath $process.log){Get-Content -LiteralPath $process.log -Raw}else{''}
    if($process.role -eq 'host'){$hostLog=$log}
    $mode=if($process.role -eq 'host'){'2'}else{'3'}
    $prefix='LogTemp: Display: BwayCooldownUpgradeTest: '
    $purchase=[regex]::Matches($log,$prefix+'Purchase PASS player=(?<id>\d+) netmode=(?<mode>\d+) before=(?<before>[\d.]+) after=(?<after>[\d.]+) cost=(?<cost>\d+) tolerance=(?<tol>[\d.]+) bonus=(?<bonus>[\d.]+) base=(?<base>[\d.]+)')
    $activate=[regex]::Matches($log,$prefix+'Activate player=(?<id>\d+) netmode=(?<mode>\d+) expected=(?<expected>[\d.]+)')
    $reject=[regex]::Matches($log,$prefix+'RejectedDuringCooldown player=(?<id>\d+) netmode=(?<mode>\d+) inactive=1')
    $hud=[regex]::Matches($log,$prefix+'HUD player=(?<id>\d+) netmode=(?<mode>\d+) sample=(?<sample>\d+) duration=(?<duration>[\d.]+) remaining=(?<remaining>[\d.]+) countdown=(?<countdown>[\d.]+) percent=(?<percent>[\d.]+) elapsed=(?<elapsed>[\d.]+) controls=1')
    $expiry=[regex]::Matches($log,$prefix+'Expiry PASS player=(?<id>\d+) netmode=(?<mode>\d+) elapsed=(?<elapsed>[\d.]+) expected=(?<expected>[\d.]+) controls=1')
    $again=[regex]::Matches($log,$prefix+'Reactivation PASS player=(?<id>\d+) netmode=(?<mode>\d+) duration=(?<duration>[\d.]+)')
    $pass=[regex]::Matches($log,$prefix+'PASS scope=purchase-activation-cooldown-hud-expiry-reactivation')
    $fail=[regex]::Matches($log,'LogTemp: Error: BwayCooldownUpgradeTest: FAIL[^\r\n]*')
    $errors=[regex]::Matches($log,'(?m)^.*(?:\bError:|Fatal error|Assertion failed|Ensure condition failed|Accessed None).*$')
    $ok=$purchase.Count -eq 1 -and $activate.Count -eq 1 -and $reject.Count -eq 1 -and $hud.Count -eq 3 -and $expiry.Count -eq 1 -and $again.Count -eq 1 -and $pass.Count -eq 1 -and !$fail.Count
    $owner=if($purchase.Count){[int]$purchase[0].Groups['id'].Value}else{-1}
    foreach($match in @($purchase)+@($activate)+@($reject)+@($hud)+@($expiry)+@($again)){
        if($match.Groups['id'].Value -ne [string]$owner -or $match.Groups['mode'].Value -ne $mode){$ok=$false}
    }
    if($purchase.Count -eq 1){
        $g=$purchase[0].Groups; $before=N $g 'before'; $after=N $g 'after'; $tol=N $g 'tol'
        if([int]$g['cost'].Value -ne $cost -or !(Near (N $g 'bonus') $bonus 0.0001) -or !(Near (N $g 'base') $base) -or $tol -lt 0 -or $tol -gt $cost -or $after -ge $before -or $after -lt $before-$cost-0.02 -or $after -gt $before-$cost+$tol+0.02){$ok=$false}
    }
    foreach($match in $activate){if(!(Near (N $match.Groups 'expected') $expected)){$ok=$false}}
    $samples=@()
    foreach($match in $hud){
        $g=$match.Groups; $duration=N $g 'duration'; $remaining=N $g 'remaining'; $elapsed=N $g 'elapsed'
        if(!(Near $duration $expected) -or $remaining -le 0 -or $remaining -gt $expected+0.02 -or !(Near (N $g 'countdown') $remaining 0.35) -or !(Near (N $g 'percent') ($remaining/$expected) 0.03) -or $elapsed -lt 1.49){$ok=$false}
        $samples += [pscustomobject]@{sample=[int]$g['sample'].Value;duration=$duration;remaining=$remaining;elapsed=$elapsed;countdown=N $g 'countdown';percent=N $g 'percent'}
    }
    if(($samples.sample -join ',') -ne '1,2,3'){$ok=$false}
    if($samples.Count -eq 3){
        if($samples[1].elapsed-$samples[0].elapsed -lt 1.99 -or $samples[1].remaining -ge $samples[0].remaining -or !(Near ($samples[0].remaining-$samples[1].remaining) ($samples[1].elapsed-$samples[0].elapsed) 0.35)){$ok=$false}
    }
    foreach($match in $expiry){if(!(Near (N $match.Groups 'expected') $expected) -or !(Near (N $match.Groups 'elapsed') $expected 1.5)){$ok=$false}}
    foreach($match in $again){if(!(Near (N $match.Groups 'duration') $expected)){$ok=$false}}
    $latency=$log -match 'LogNet:.*PktLag set to 200'
    if(!$ok){$problems.Add("$($process.role): " + $(if($fail.Count){$fail[0].Value}else{'missing or inconsistent purchase/cooldown/HUD checkpoints'}))}
    if(!$latency){$problems.Add("$($process.role): missing runtime 200ms latency")}
    if($errors.Count){$problems.Add("$($process.role): $($errors.Count) runtime errors")}
    if($process.role -ne 'host' -and $process.exitCode -ne 0){$problems.Add("$($process.role): client exit was not zero")}
    $rows += [pscustomobject]@{role=$process.role;owner=$owner;passed=$ok;samples=$samples;errorCount=$errors.Count;firstError=if($errors.Count){$errors[0].Value}else{$null};latency=$latency;exitCode=$process.exitCode;log=$process.log}
}
if(@($rows.role | Sort-Object -Unique).Count -ne 4 -or @($rows | Where-Object role -eq 'host').Count -ne 1 -or @($rows | Where-Object role -match '^client[123]$').Count -ne 3 -or @($rows.owner | Sort-Object -Unique).Count -ne 4){$problems.Add('Expected one host and three distinct client owners')}
$begins=[regex]::Matches($hostLog,'LogTemp: Display: BwayCooldownUpgradeTest: Authority BEGIN player=(?<id>\d+) cycle=(?<cycle>\d+) base=(?<base>[\d.]+) bonus=(?<bonus>[\d.]+) duration=(?<duration>[\d.]+) rank=1')
$ends=[regex]::Matches($hostLog,'LogTemp: Display: BwayCooldownUpgradeTest: Authority EXPIRE player=(?<id>\d+) elapsed=(?<elapsed>[\d.]+) expected=(?<expected>[\d.]+)')
$authority=@()
if($begins.Count -ne 8 -or $ends.Count -ne 4 -or [regex]::Matches($hostLog,'LogTemp: Display: BwayCooldownUpgradeTest: Authority PASS owners=4 cycles=8 expiries=4').Count -ne 1){$problems.Add('Missing independent authority cycles or expiry evidence')}
foreach($match in $begins){
    $g=$match.Groups; $id=[int]$g['id'].Value
    if($id -notin $rows.owner -or !(Near (N $g 'base') $base) -or !(Near (N $g 'bonus') $bonus 0.0001) -or !(Near (N $g 'duration') $expected)){$problems.Add('Authority duration, catalog, or owner mismatch')}
    $authority += [pscustomobject]@{owner=$id;cycle=[int]$g['cycle'].Value;duration=N $g 'duration'}
}
foreach($owner in $rows.owner){
    if((@($authority | Where-Object owner -eq $owner).cycle -join ',') -ne '1,2'){$problems.Add("Owner $owner lacks exactly two authority cycles")}
    $ownerEnds=@($ends | Where-Object {$_.Groups['id'].Value -eq [string]$owner})
    if($ownerEnds.Count -ne 1){$problems.Add("Owner $owner lacks one authority expiry")}
    foreach($match in $ownerEnds){if(!(Near (N $match.Groups 'expected') $expected) -or !(Near (N $match.Groups 'elapsed') $expected 1.0)){$problems.Add("Owner $owner authority expiry mismatch")}}
}
$causalError=$null
if($problems.Count){
    $nativeError=@($rows | Where-Object firstError | Select-Object -First 1)
    $causalError=if($nativeError.Count){$nativeError[0].firstError}else{$problems[0]}
}
$result=[pscustomobject]@{generatedUtc=[DateTime]::UtcNow.ToString('o');candidate=$run.candidate;passed=$problems.Count -eq 0;scope='Four rendered Argus processes at 200ms: purchase RPC, real ForGlory activation, cooldown rejection, settled owner duration, native HUD controls/countdown/expiry and reactivation; eight independent authority cycles. No physical input, visual inspection, other-kit network, avatar/round lifecycle, final tuning or second-PC proof.';processCount=$rows.Count;hudSamples=($rows.samples | Measure-Object).Count;authorityCycles=$begins.Count;authorityExpiries=$ends.Count;expectedDuration=$expected;catalogCost=$cost;catalogBonus=$bonus;kitBaseDuration=$base;errorCount=($rows | Measure-Object errorCount -Sum).Sum;firstCausalError=$causalError;problems=@($problems);processes=$rows;authority=$authority;runJson=$RunJson;catalogJson=$CatalogJson;kitJson=$KitJson;hostTeardown=$run.hostTeardown;outputJson=$OutputJson}
$result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
$result | Select-Object passed,candidate,processCount,hudSamples,authorityCycles,authorityExpiries,errorCount,firstCausalError,outputJson | ConvertTo-Json -Compress
