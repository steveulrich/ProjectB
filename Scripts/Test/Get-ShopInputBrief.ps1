[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunJson,
    [Parameter(Mandatory)][string]$OutputJson,
    [switch]$RequireNavigation,
    [switch]$RequirePurchase,
    [switch]$RequireRespawn
)
$ErrorActionPreference = 'Stop'
if ($RequireRespawn) { $RequirePurchase = $true }
if ($RequirePurchase) { $RequireNavigation = $true }
$run = Get-Content -LiteralPath $RunJson -Raw | ConvertFrom-Json
$rows = @()
$problems = [System.Collections.Generic.List[string]]::new()
if ($run.runError) { $problems.Add([string]$run.runError) }
if (!$run.allConnectedAlive) { $problems.Add('Missing four-live-process checkpoint before cleanup') }
foreach ($process in $run.processes) {
    $log = if (Test-Path -LiteralPath $process.log) { Get-Content -LiteralPath $process.log -Raw } else { '' }
    $errors = @([regex]::Matches($log, '(?m)^.*(?:\bError:|Fatal error|Assertion failed|Ensure condition failed|Accessed None).*$'))
    $setup = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: Setup peer=0 bindings=1 keyboard=1 gamepad=1 shops=0')
    $openedKeyboard = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: Open peer=0 netmode=(\d+) key=B count=1 menu=1 selectorFocus=1 moveIgnored=1 lookIgnored=1')
    $openedGamepad = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: Open peer=0 netmode=(\d+) key=DPadUp count=1 menu=1 selectorFocus=1 moveIgnored=1 lookIgnored=1')
    $restored = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: Peer PASS index=0 netmode=(\d+) restored=1')
    $restarts = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: Restart peer=0 activeShop=1 resets=2')
    $navigation = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: Navigation PASS peer=0 down=1 up=1 accept=1 selectorFocus=1')
    $passes = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: PASS peers=1 scope=synthetic-key-open-held-toggle-commonui-back')
    $fails = [regex]::Matches($log, 'LogTemp: Error: BwayShopInputTest: FAIL[^\r\n]*')
    $expectedNetMode = if ($process.role -eq 'host') { '2' } else { '3' }
    $modeMatches = $openedKeyboard.Count -eq 2 -and @($openedKeyboard | Where-Object { $_.Groups[1].Value -ne $expectedNetMode }).Count -eq 0 -and $openedGamepad.Count -eq 1 -and $openedGamepad[0].Groups[1].Value -eq $expectedNetMode -and $restored.Count -eq 1 -and $restored[0].Groups[1].Value -eq $expectedNetMode
    $fixturePassed = $setup.Count -eq 1 -and $modeMatches -and $restarts.Count -eq 1 -and $passes.Count -eq 1 -and $fails.Count -eq 0
    if ($RequireNavigation -and $navigation.Count -ne 1) { $fixturePassed = $false }
    $purchaseSummary = $null
    if ($RequirePurchase) {
        $submitted = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: PurchaseSubmitted peer=0 netmode=(?<mode>\d+) id=(?<id>\S+) before=(?<before>[\d.]+) cost=(?<cost>\d+) income=(?<income>[\d.]+) disabled=1 selectorFocus=1 pending=(?<pending>[01]) rank=(?<rank>\d+)')
        $purchased = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: Purchase PASS peer=0 netmode=(?<mode>\d+) id=(?<id>\S+) rank=1 before=(?<before>[\d.]+) after=(?<after>[\d.]+) cost=(?<cost>\d+) tolerance=(?<tolerance>[\d.]+) walletDeltaOk=1 disabled=1 selectorFocus=1')
        $back = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: UIKey key=Gamepad_FaceButton_Right user=0 ')
        $purchaseOk = $submitted.Count -eq 1 -and $purchased.Count -eq 1 -and $back.Count -eq 1
        $wallet = $null
        if ($purchaseOk) {
            $sent = $submitted[0].Groups
            $done = $purchased[0].Groups
            $before = [double]::Parse($done['before'].Value, [Globalization.CultureInfo]::InvariantCulture)
            $after = [double]::Parse($done['after'].Value, [Globalization.CultureInfo]::InvariantCulture)
            $cost = [int]$done['cost'].Value
            $tolerance = [double]::Parse($done['tolerance'].Value, [Globalization.CultureInfo]::InvariantCulture)
            $income = [double]::Parse($sent['income'].Value, [Globalization.CultureInfo]::InvariantCulture)
            $expectedPending = if ($process.role -eq 'host') { '0' } else { '1' }
            $expectedImmediateRank = if ($process.role -eq 'host') { '1' } else { '0' }
            $purchaseOk = $sent['mode'].Value -eq $expectedNetMode -and $done['mode'].Value -eq $expectedNetMode -and $sent['id'].Value -eq $done['id'].Value -and $sent['before'].Value -eq $done['before'].Value -and $sent['cost'].Value -eq $done['cost'].Value -and $sent['pending'].Value -eq $expectedPending -and $sent['rank'].Value -eq $expectedImmediateRank -and $cost -gt 0 -and $tolerance -ge 0 -and $tolerance -le (10 * $income + 0.02) -and $after -lt $before -and $after -ge ($before - $cost - 0.02) -and $after -le ($before - $cost + $tolerance + 0.02)
            $wallet = [pscustomobject]@{id=$done['id'].Value; before=$before; after=$after; cost=$cost; tolerance=$tolerance; pendingObserved=($sent['pending'].Value -eq '1')}
        }
        $debits = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: AuthorityDebit player=(?<player>\d+) id=(?<id>\S+) before=(?<before>[\d.]+) after=(?<after>[\d.]+) cost=(?<cost>\d+) exact=1 count=1')
        $authorityOk = $true
        if ($process.role -eq 'host') {
            $seeds = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: PurchaseSeed player=(?<player>\d+) id=(?<id>\S+) cost=(?<cost>\d+)')
            $authorityPass = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: AuthorityPurchase PASS peers=4 exactDebits=4')
            $authorityOk = $debits.Count -eq 4 -and $seeds.Count -eq 4 -and $authorityPass.Count -eq 1 -and @($debits | ForEach-Object { $_.Groups['player'].Value } | Sort-Object -Unique).Count -eq 4
            foreach ($debit in $debits) {
                $g = $debit.Groups
                $debitBefore = [double]::Parse($g['before'].Value, [Globalization.CultureInfo]::InvariantCulture)
                $debitAfter = [double]::Parse($g['after'].Value, [Globalization.CultureInfo]::InvariantCulture)
                $seed = @($seeds | Where-Object { $_.Groups['player'].Value -eq $g['player'].Value -and $_.Groups['id'].Value -eq $g['id'].Value -and $_.Groups['cost'].Value -eq $g['cost'].Value })
                if ($seed.Count -ne 1 -or [Math]::Abs($debitBefore - $debitAfter - [int]$g['cost'].Value) -gt 0.02) { $authorityOk = $false }
            }
        }
        $purchaseSummary = [pscustomobject]@{passed=$purchaseOk; submittedCount=$submitted.Count; purchasePassCount=$purchased.Count; gamepadBackCount=$back.Count; wallet=$wallet; authorityPassed=$authorityOk; authorityDebitCount=$debits.Count}
        if (!$purchaseOk -or !$authorityOk) { $fixturePassed = $false }
    }
    $respawnSummary = $null
    if ($RequireRespawn) {
        $baseline = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: RespawnBaseline peer=0 netmode=(?<mode>\d+) old=(?<old>\S+) deaths=(?<deaths>\d+) shopOpen=1')
        $dead = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: DeathObserved peer=0 netmode=(?<mode>\d+) old=(?<old>\S+) shopOpen=1 selectorFocus=1 moveIgnored=1 lookIgnored=1')
        $respawn = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: Respawn PASS peer=0 netmode=(?<mode>\d+) old=(?<old>\S+) new=(?<new>\S+) deathsBefore=(?<before>\d+) deathsAfter=(?<after>\d+) sameShop=1 selectorFocus=1 moveIgnored=1 lookIgnored=1 bindings=1 rank=1')
        $postNavigation = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: RespawnNavigation PASS peer=0 netmode=(?<mode>\d+) accept=1 down=1 selection=1 selectorFocus=1')
        $respawnOk = $baseline.Count -eq 1 -and $dead.Count -eq 1 -and $respawn.Count -eq 1 -and $postNavigation.Count -eq 1
        if ($respawnOk) {
            $b=$baseline[0].Groups; $d=$dead[0].Groups; $r=$respawn[0].Groups
            $respawnOk = $b['mode'].Value -eq $expectedNetMode -and $d['mode'].Value -eq $expectedNetMode -and $r['mode'].Value -eq $expectedNetMode -and $postNavigation[0].Groups['mode'].Value -eq $expectedNetMode -and $b['old'].Value -eq $d['old'].Value -and $b['old'].Value -eq $r['old'].Value -and $r['new'].Value -ne $r['old'].Value -and $r['new'].Value -ne 'None' -and $r['old'].Value -ne 'None' -and [int]$r['before'].Value -eq [int]$b['deaths'].Value -and [int]$r['after'].Value -eq ([int]$r['before'].Value + 1)
        }
        $damage = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: RespawnDamage player=(?<player>\d+) old=(?<old>\S+) health=0.00 deathsBefore=(?<before>\d+)')
        $authorityRespawns = [regex]::Matches($log, 'LogTemp: Display: BwayShopInputTest: AuthorityRespawn PASS player=(?<player>\d+) old=(?<old>\S+) new=(?<new>\S+) deathsBefore=(?<before>\d+) deathsAfter=(?<after>\d+)')
        $authorityRespawnOk = $true
        if ($process.role -eq 'host') {
            $authorityRespawnOk = $damage.Count -eq 4 -and $authorityRespawns.Count -eq 4 -and @($authorityRespawns | ForEach-Object {$_.Groups['player'].Value} | Sort-Object -Unique).Count -eq 4
            foreach ($item in $authorityRespawns) {
                $r=$item.Groups
                $matchingDamage = @($damage | Where-Object {$_.Groups['player'].Value -eq $r['player'].Value -and $_.Groups['old'].Value -eq $r['old'].Value -and $_.Groups['before'].Value -eq $r['before'].Value})
                if ($matchingDamage.Count -ne 1 -or $r['new'].Value -eq $r['old'].Value -or $r['new'].Value -eq 'None' -or [int]$r['after'].Value -ne ([int]$r['before'].Value + 1)) { $authorityRespawnOk = $false }
            }
        }
        $respawnSummary = [pscustomobject]@{passed=$respawnOk; deathObservedCount=$dead.Count; respawnCount=$respawn.Count; postRespawnNavigationCount=$postNavigation.Count; authorityPassed=$authorityRespawnOk; authorityRespawnCount=$authorityRespawns.Count}
        if (!$respawnOk -or !$authorityRespawnOk) { $fixturePassed = $false }
    }
    if (!$fixturePassed) {
        $problems.Add("$($process.role): " + $(if ($fails.Count) { $fails[0].Value } else { 'missing native input checkpoints' }))
    }
    $latencyConfirmed = $log -match 'LogNet:.*PktLag set to 200'
    if (!$latencyConfirmed) { $problems.Add("$($process.role): latency was not confirmed by the runtime") }
    if ($errors.Count) { $problems.Add("$($process.role): $($errors.Count) runtime error/critical lines") }
    if ($process.role -ne 'host' -and $process.exitCode -ne 0) { $problems.Add("$($process.role): client exit was not zero") }
    $rows += [pscustomobject]@{role=$process.role; log=$process.log; fixturePassed=$fixturePassed; latencyConfirmed=$latencyConfirmed; setupCount=$setup.Count; keyboardOpenCount=$openedKeyboard.Count; gamepadOpenCount=$openedGamepad.Count; restartCount=$restarts.Count; restoreCount=$restored.Count; passCount=$passes.Count; failureMarkers=@($fails | ForEach-Object Value); errorCount=$errors.Count; firstError=if($errors.Count){$errors[0].Value}else{$null}; exitCode=$process.exitCode}
    $rows[-1] | Add-Member -NotePropertyName navigationCount -NotePropertyValue $navigation.Count
    $rows[-1] | Add-Member -NotePropertyName purchase -NotePropertyValue $purchaseSummary
    $rows[-1] | Add-Member -NotePropertyName respawn -NotePropertyValue $respawnSummary
}
if (@($rows.role | Sort-Object -Unique).Count -ne 4 -or @($rows | Where-Object role -eq 'host').Count -ne 1 -or @($rows | Where-Object role -match '^client[123]$').Count -ne 3) { $problems.Add('Expected one host and three unique clients') }
$result = [pscustomobject]@{
    generatedUtc=[DateTime]::UtcNow.ToString('o'); candidate=$run.candidate
    passed=($problems.Count -eq 0 -and $run.hostReady -and $run.clientExitZero)
    scope=('Four rendered packaged processes on one PC at 200 ms latency; synthetic B/DPadUp engine keys, held-input behavior, selector focus and blocking through same-pawn client restart and repeated flag reset, keyboard toggle/CommonUI Back, input restoration. ' + $(if ($RequireNavigation) { 'Also requires Slate gamepad Accept, Down/Up selection and focus restoration. ' } else { 'No selector-navigation proof. ' }) + $(if ($RequirePurchase) { 'Also requires Buy navigation/activation, remote pending/disabled focus, replicated wallet/rank, four exact authoritative catalog-price debits and gamepad Back. ' } else { 'No purchase-button navigation proof. ' }) + $(if ($RequireRespawn) { 'Also requires four production deaths and replacement pawns with retained modal/focus/locks/rank, one binding, and post-respawn Slate navigation. ' } else { 'No replacement-pawn proof. ' }) + 'No physical device or second-PC proof.')
    processes=$rows; processCount=$rows.Count; errorCount=($rows | Measure-Object errorCount -Sum).Sum
    navigationRequired=[bool]$RequireNavigation
    purchaseRequired=[bool]$RequirePurchase
    respawnRequired=[bool]$RequireRespawn
    firstCausalError=if($problems.Count){$problems[0]}else{$null}; problems=@($problems)
    hostTeardown=$run.hostTeardown; runJson=$RunJson; outputJson=$OutputJson
}
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
$result | Select-Object passed,candidate,processCount,errorCount,firstCausalError,scope,outputJson | ConvertTo-Json -Compress
