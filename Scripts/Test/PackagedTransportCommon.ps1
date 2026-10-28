# Shared, side-effect-free definitions. Dot-sourcing never starts a process or changes a contract.
function Get-BwayPackagedRunBudget($Contract, [int]$AdditionalRuns = 1) {
    if ($AdditionalRuns -lt 0) { throw 'Additional runtime runs cannot be negative.' }
    $values = @()
    foreach ($container in @('counters', 'counts')) {
        $bag = $Contract.$container
        if ($bag -and $bag.PSObject.Properties['packagedRuns']) {
            $value = 0L
            if (![long]::TryParse([string]$bag.packagedRuns, [ref]$value) -or $value -lt 0) { throw "Invalid $container.packagedRuns." }
            $values += $value
        }
    }
    $current = if ($values.Count) { [long]($values | Measure-Object -Maximum).Maximum } else { 0L }
    $cap = $null
    # packageRuns counts packaging invocations; packagedRuns counts runtime topologies.
    # Never consume or reset the build/package caps when reserving runtime evidence.
    if ($Contract.caps -and $Contract.caps.PSObject.Properties['packagedRuns']) {
        $parsedCap = 0L
        if (![long]::TryParse([string]$Contract.caps.packagedRuns, [ref]$parsedCap) -or $parsedCap -lt 0) { throw 'Invalid caps.packagedRuns.' }
        $cap = $parsedCap
        if ($current + $AdditionalRuns -gt $cap) { throw 'Packaged runtime attempt cap exhausted.' }
    }
    [pscustomobject]@{ current = $current; next = $current + $AdditionalRuns; cap = $cap }
}

function Add-BwayPackagedRun($Contract) {
    $budget = Get-BwayPackagedRunBudget $Contract 1
    $found = $false
    foreach ($container in @('counters', 'counts')) {
        $bag = $Contract.$container
        if ($bag -and $bag.PSObject.Properties['packagedRuns']) { $bag.packagedRuns = $budget.next; $found = $true }
    }
    if (!$found) {
        $container = if ($Contract.PSObject.Properties['counters']) { 'counters' } elseif ($Contract.PSObject.Properties['counts']) { 'counts' } else { 'counters' }
        if (!$Contract.$container) { $Contract | Add-Member -NotePropertyName $container -NotePropertyValue ([pscustomobject]@{}) -Force }
        $Contract.$container | Add-Member -NotePropertyName packagedRuns -NotePropertyValue $budget.next
    }
    $budget.next
}

function Get-BwayNativeLines([string]$Text, [string]$Prefix) {
    # An echoed command line containing a completion marker is never evidence.
    $pattern = '(?m)^(?:\[[^\]\r\n]*\]\s*)*LogTemp: Display: (' + [regex]::Escape($Prefix) + '[^\r\n]*)\r?$'
    @([regex]::Matches($Text, $pattern) | ForEach-Object { $_.Groups[1].Value })
}

function Get-BwayOneMarker([string[]]$Lines, [string]$Pattern, [string]$Name) {
    $matchesFound = @($Lines | ForEach-Object { $m = [regex]::Match($_, $Pattern); if ($m.Success) { $m } })
    if ($matchesFound.Count -ne 1) { throw "$Name requires exactly one emitted marker; found $($matchesFound.Count)." }
    $matchesFound[0]
}

function Assert-BwayRelicLaunchPhysics([string[]]$FixtureLines, [string[]]$ThrowLines, [int]$Source) {
    function Number($Value) {
        if ($null -eq $Value -or $Value -is [bool]) { throw 'Missing numeric launch evidence.' }
        $number = 0.0
        if (![double]::TryParse([string]$Value,[globalization.NumberStyles]::Float,[cultureinfo]::InvariantCulture,[ref]$number) -or [double]::IsNaN($number) -or [double]::IsInfinity($number)) { throw 'Nonfinite launch evidence.' }
        return $number
    }
    function Vector($Value) {
        if (@($Value).Count -ne 3) { throw 'Launch vector must have three components.' }
        return ,([double[]]@((Number $Value[0]),(Number $Value[1]),(Number $Value[2])))
    }
    function Length([double[]]$V) { [math]::Sqrt($V[0]*$V[0]+$V[1]*$V[1]+$V[2]*$V[2]) }
    function Difference([double[]]$A,[double[]]$B) { Length ([double[]]@(($A[0]-$B[0]),($A[1]-$B[1]),($A[2]-$B[2]))) }
    $physicsLines = @($FixtureLines | Where-Object { $_.StartsWith('BwayRelicFlight: LaunchPhysics ') })
    if ($physicsLines.Count -ne 3) { throw 'Current RelicFlight requires exactly three LaunchPhysics records.' }
    $physics = @($physicsLines | ForEach-Object { $_.Substring('BwayRelicFlight: LaunchPhysics '.Length) | ConvertFrom-Json })
    $releasePattern = '^BwayRelicThrow: RELEASE role=2 player=' + $Source + ' chargeSeconds=(\d+\.\d{6}) speed=(\d+\.\d{6}) aim=X=([-\d.]+) Y=([-\d.]+) Z=([-\d.]+) authoritative=1 accepted=1$'
    $releases = @($ThrowLines | ForEach-Object { $m=[regex]::Match($_,$releasePattern); if($m.Success){$m} })
    if ($releases.Count -ne 3) { throw 'Three accepted authority release contracts are required.' }
    for ($scenario=0; $scenario -lt 3; ++$scenario) {
        $rows = @($physics | Where-Object { (Number $_.scenario) -eq $scenario -and (Number $_.source) -eq $Source })
        if ($rows.Count -ne 1) { throw 'Missing or duplicate scenario reconstruction.' }
        $p=$rows[0]
        if ((Number $p.schema) -ne 1 -or $p.pass -isnot [bool] -or !$p.pass) { throw 'Invalid reconstruction schema/verdict.' }
        if ($p.solverMode -ne 'synchronous-single-step' -or (Number $p.lastPhysicsFrame) -ne (Number $p.sampleFrame) -or (Number $p.sampleFrame) -le 0 -or (Number $p.sampleSolverFrame)-(Number $p.startSolverFrame) -ne @($p.steps).Count) { throw 'Untracked or stale physics frame.' }
        $observed=Vector $p.observed; $initial=Vector $p.initial; $aim=Vector $p.aim; $claimedGravity=Vector $p.gravityVelocity
        if ([math]::Abs((Length $aim)-1.0) -gt 0.001) { throw 'Invalid launch aim.' }
        if (@($p.steps).Count -lt 1 -or @($p.steps).Count -gt 4) { throw 'Invalid launch step count.' }
        $multiplier=1.0; $seconds=0.0; $gravity=[double[]]@(0,0,0)
        foreach($step in $p.steps) {
            $dt=Number $step.seconds; $damping=Number $step.damping; $g=Vector $step.gravity
            if($dt -le 0 -or $dt -gt 0.05 -or $damping -lt 0) { throw 'Invalid recorded solver step.' }
            $drag=[math]::Max(0.0,1-$damping*$dt); $multiplier*=$drag; $seconds+=$dt
            for($axis=0;$axis -lt 3;++$axis){$gravity[$axis]=($gravity[$axis]+$g[$axis]*$dt)*$drag}
        }
        if($seconds -gt 0.15 -or $multiplier -lt 0.5 -or [math]::Abs($seconds-(Number $p.seconds)) -gt 0.000001 -or [math]::Abs($seconds-(Number $p.solverElapsed)) -gt 0.00001 -or [math]::Abs($multiplier-(Number $p.multiplier)) -gt 0.000001 -or (Difference $gravity $claimedGravity) -gt 0.001) { throw "Step timing or affine transform disagrees: seconds=$seconds recorded=$($p.seconds) solver=$($p.solverElapsed) multiplier=$multiplier recordedMultiplier=$($p.multiplier) gravityError=$(Difference $gravity $claimedGravity)." }
        $recovered=[double[]]@(0,0,0)
        for($axis=0;$axis -lt 3;++$axis){$recovered[$axis]=($observed[$axis]-$gravity[$axis])/$multiplier}
        if((Length $observed) -le 0 -or (Difference $initial $recovered) -gt 0.001) { throw 'Physical launch reconstruction disagrees.' }
        $launch=Get-BwayOneMarker $FixtureLines ("^BwayRelicFlight: Launch scenario=$scenario source=$Source observedSpeed=([\d.]+) expectedSpeed=([\d.]+) physicsTolerance=180 sample=post-physics-next-frame pass=1`$") "Relic launch $scenario"
        $expected=Number $p.expectedSpeed; $minimum=Number $p.minThrowSpeed; $maximum=Number $p.maxThrowSpeed; $maxCharge=Number $p.maxChargeTime; $hold=Number $p.holdSeconds
        if($minimum -le 0 -or $maximum -le $minimum -or $maxCharge -le 0 -or $hold -lt 0) { throw 'Invalid charge configuration.' }
        $requiredHold=@(0.1,($maxCharge*0.5),($maxCharge+0.35))[$scenario]
        if([math]::Abs($hold-$requiredHold) -gt 0.000001) { throw 'Wrong scenario charge duration.' }
        $nominal=$minimum+($maximum-$minimum)*[math]::Min(1.0,[math]::Max(0.0,$hold/$maxCharge))
        if([math]::Abs($nominal-$expected) -gt 0.001 -or [math]::Abs((Number $launch.Groups[1].Value)-(Length $observed)) -gt 0.001 -or [math]::Abs((Number $launch.Groups[2].Value)-$expected) -gt 0.001) { throw 'Launch marker or charge configuration disagrees.' }
        $speed=Length $initial
        $expectedVector=[double[]]@(($aim[0]*$expected),($aim[1]*$expected),($aim[2]*$expected)); $error=Difference $initial $expectedVector
        if([math]::Abs($speed-$expected) -gt 180.001 -or $error -gt 180.001 -or [math]::Abs($error-(Number $p.vectorError)) -gt 0.001) { throw 'Initial launch contract violated.' }
        if(($scenario -eq 0 -and $speed -ge $maximum-200) -or ($scenario -eq 1 -and ($speed -le $minimum+200 -or $speed -ge $maximum-200))) { throw 'Charge scenarios are not distinguished.' }
        $release=$releases[$scenario]; $charge=Number $release.Groups[1].Value; $requested=Number $release.Groups[2].Value
        $releaseAim=[double[]]@((Number $release.Groups[3].Value),(Number $release.Groups[4].Value),(Number $release.Groups[5].Value))
        $contract=$minimum+($maximum-$minimum)*[math]::Min(1.0,[math]::Max(0.0,$charge/$maxCharge))
        $roundingAllowance=0.0000005*(1+($maximum-$minimum)/$maxCharge)+0.001
        if($charge -lt 0 -or $charge -gt $maxCharge+0.000001 -or [math]::Abs($contract-$requested) -gt $roundingAllowance -or (Difference $releaseAim $aim) -gt 0.002 -or [math]::Abs($requested-$speed) -gt 180.001) { throw 'Authoritative release contract disagrees with physical evidence.' }
    }
}


function Assert-BwayFixtureEvidence([ValidateSet('RelicFlight','BuildableCombat','Siege')][string]$Phase, [hashtable]$Logs) {
    $roles = @('host','client1','client2','client3')
    if ($Logs.Count -ne 4 -or @($roles | Where-Object { !$Logs.ContainsKey($_) }).Count) { throw 'Exactly four role logs required.' }
    $prefix = switch ($Phase) { RelicFlight { 'BwayRelicFlight:' } BuildableCombat { 'BwayBuildableCombat:' } Siege { 'BwaySiegeAudit:' } }
    $lines = @{}; foreach ($role in $roles) { $lines[$role] = @(Get-BwayNativeLines $Logs[$role] $prefix) }
    $ids = @(); $source = $null; $held = $null
    if ($Phase -eq 'RelicFlight') {
        $complete = Get-BwayOneMarker $lines.host '^BwayRelicFlight: COMPLETE pass=1 peers=4 scenarios=5 remoteOwner=(\d+) groundedDelta=0 airborneDelta=1 cleanup=1$' 'Relic completion'
        $source = [int]$complete.Groups[1].Value
        Assert-BwayRelicLaunchPhysics $lines.host @(Get-BwayNativeLines $Logs.host 'BwayRelicThrow:') $source
        $stageCases = @(@(3,0),@(3,1),@(3,2),@(4,3),@(4,4),@(6,4),@(7,4),@(8,4),@(10,4),@(11,4))
        $serials = @{}
        foreach ($role in $roles) {
            $peer = Get-BwayOneMarker $lines[$role] '^BwayRelicFlight: Peer PASS witness=(\d+) source=(\d+) serial=(\d+) stage=12 scenario=4 net=\d+ .*\bstats=1(?:\s.*)?$' "$role relic final cleanup"
            $ids += [int]$peer.Groups[1].Value
            if ([int]$peer.Groups[2].Value -ne $source) { throw 'Relic source disagreement.' }
            $id = [int]$peer.Groups[1].Value
            if ($role -eq 'host') { $serials.final = $peer.Groups[3].Value } elseif ($serials.final -ne $peer.Groups[3].Value) { throw 'Relic final serial disagreement.' }
            foreach ($stageCase in $stageCases) {
                $stage=$stageCase[0];$scenario=$stageCase[1];$key="$stage-$scenario"
                $marker = Get-BwayOneMarker $lines[$role] ("^BwayRelicFlight: Peer PASS witness=$id source=$source serial=(\d+) stage=$stage scenario=$scenario net=\d+ .*\bstats=1(?:\s.*)?`$") "$role relic stage $key"
                if ($role -eq 'host') { $serials[$key]=$marker.Groups[1].Value } elseif ($serials[$key] -ne $marker.Groups[1].Value) { throw 'Relic scenario serial disagreement.' }
            }
        }
        if ($ids[1] -ne $source) { throw 'Relic active remote owner was not rendered client1.' }
    } else {
        $pattern = if ($Phase -eq 'BuildableCombat') { '^BwayBuildableCombat: Complete reason=observed sequence=9 created=8 destroyed=8 restored=(\d+) expired=(\d+) held=(\d+)$' } else { '^BwaySiegeAudit: Complete reason=observed stage=6 destroyed=0 restored=(\d+) expired=(\d+) held=(\d+)$' }
        $complete = Get-BwayOneMarker $lines.host $pattern "$Phase completion"
        $held = [int]$complete.Groups[3].Value
        if ($held -lt 4 -or [int]$complete.Groups[1].Value + [int]$complete.Groups[2].Value -ne $held) { throw 'Held pawn cleanup was not reconciled.' }
        if ($Phase -eq 'BuildableCombat') {
            $null = Get-BwayOneMarker $lines.host '^BwayBuildableCombat: Setup humans=4 remotes=3 held=\d+ round=\d+$' 'Combat topology'
            for ($sequence=1; $sequence -le 8; ++$sequence) {
                $v = Get-BwayOneMarker $lines.host ("^BwayBuildableCombat: Verify sequence=$sequence netId=(\d+) before=([\d.]+) after=([\d.]+) enemy=([01]) pass=1`$") "Combat case $sequence"
                $before = [double]::Parse($v.Groups[2].Value, [cultureinfo]::InvariantCulture)
                $after = [double]::Parse($v.Groups[3].Value, [cultureinfo]::InvariantCulture)
                if ([long]$v.Groups[1].Value -le 1 -or $before -le 0 -or [int]$v.Groups[4].Value -ne ($sequence % 2 -eq 0) -or ($sequence % 2 -eq 0 -and ($after -le 0 -or $after -ge $before)) -or ($sequence % 2 -eq 1 -and [math]::Abs($after-$before) -gt .001)) { throw 'Buildable damage evidence disagrees with the verdict.' }
            }
        } else {
            $setup = Get-BwayOneMarker $lines.host '^BwaySiegeAudit: Setup id=(\d+) round=\d+ humans=4 remoteHumans=3 maxActive=1 maximum=[\d.]+ speed=[\d.]+ persists=1$' 'Siege topology'
            $source = [int]$setup.Groups[1].Value
            $first = Get-BwayOneMarker $lines.host '^BwaySiegeAudit: Spawn ordinal=1 netId=(\d+) name=\S+ time=[\d.]+$' 'First Siege'
            $second = Get-BwayOneMarker $lines.host '^BwaySiegeAudit: Spawn ordinal=2 netId=(\d+) name=\S+ time=[\d.]+$' 'Second Siege'
            if ([long]$first.Groups[1].Value -le 1 -or [long]$second.Groups[1].Value -le 1 -or $first.Groups[1].Value -eq $second.Groups[1].Value) { throw 'Siege identities are not distinct replicated actors.' }
            $null = Get-BwayOneMarker $lines.host ('^BwaySiegeAudit: RoundRetained netId='+$first.Groups[1].Value+' round=\d+ quota=0 capAllows=0 time=[\d.]+$') 'Retained Siege'
            $null = Get-BwayOneMarker $lines.host ('^BwaySiegeAudit: Expired ordinal=1 netId='+$first.Groups[1].Value+' quota=0 capAllows=1 time=[\d.]+$') 'First expiry'
            $null = Get-BwayOneMarker $lines.host '^BwaySiegeAudit: Expired ordinal=2 quota=1 time=[\d.]+$' 'Second expiry'
            $null = Get-BwayOneMarker $lines.host '^BwaySiegeAudit: CleanupQuota count=0$' 'Siege quota cleanup'
        }
        $witnessRoles = if ($Phase -eq 'BuildableCombat') { $roles } else { $roles[1..3] }
        foreach ($role in $witnessRoles) {
            $releasePattern = if ($Phase -eq 'BuildableCombat') { '^BwayBuildableCombat: PeerReleased peer=(\d+)$' } else { '^BwaySiegeAudit: PeerReleased local=(\d+)$' }
            $release = Get-BwayOneMarker $lines[$role] $releasePattern "$role release"
            $id = [int]$release.Groups[1].Value; $ids += $id
            if ($Phase -eq 'BuildableCombat') {
                for ($sequence=1; $sequence -le 8; ++$sequence) {
                    $peer = Get-BwayOneMarker $lines[$role] ("^BwayBuildableCombat: PeerComplete sequence=$sequence peer=$id samples=[1-9]\d* health=([\d.]+) fired=([01]) ended=1`$") "$role case $sequence"
                    $verification = Get-BwayOneMarker $lines.host ("^BwayBuildableCombat: Verify sequence=$sequence netId=\d+ before=[\d.]+ after=([\d.]+) enemy=[01] pass=1`$") 'Damage reference'
                    if ([math]::Abs([double]::Parse($peer.Groups[1].Value,[cultureinfo]::InvariantCulture)-[double]::Parse($verification.Groups[1].Value,[cultureinfo]::InvariantCulture)) -gt .01) { throw "$role buildable health did not converge." }
                    $shouldFire = [array]::IndexOf($roles,$role) -eq [math]::Floor(($sequence-1)/2)
                    if ([int]$peer.Groups[2].Value -ne [int]$shouldFire) { throw "$role fired ownership mismatch." }
                    if ($shouldFire) { $null = Get-BwayOneMarker $lines[$role] ("^BwayBuildableCombat: OwnerFire sequence=$sequence peer=$id activated=1 spec=\S+`$") "$role owner activation" }
                }
            } else {
                $null = Get-BwayOneMarker $lines[$role] ("^BwaySiegeAudit: PeerComplete local=$id id=$source identities=2 maxCount=1 survived=1 samples=[1-9]\d*`$") "$role Siege completion"
            }
        }
        if ($Phase -eq 'Siege' -and $ids[0] -ne $source) { throw 'Siege active owner was not rendered client1.' }
    }
    if (@($ids | Sort-Object -Unique).Count -ne $ids.Count) { throw 'Peer identities are duplicated.' }
    [pscustomobject]@{ phase=$Phase; passed=$true; observedPeerIds=$ids; activeRemoteOwner=$source; heldPawns=$held; scope='Native fixture lifecycle and same-PC transport only; no pixel, audio, physical input or second-PC LAN acceptance.' }
}
