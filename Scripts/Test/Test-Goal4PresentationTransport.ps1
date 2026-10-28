<# Offline only: parser, evidence predicates, counter compatibility and ValidateOnly ordering.
   Never loads a candidate, starts Unreal or modifies the real gate contract. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputJson)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputJson){throw 'Preserve previous offline results; choose a fresh output.'}
. (Join-Path $PSScriptRoot 'PackagedTransportCommon.ps1')
$checks=[Collections.Generic.List[object]]::new()
function Check([string]$Name,[scriptblock]$Body){try{& $Body;$checks.Add([pscustomobject]@{name=$Name;passed=$true;error=$null})}catch{$checks.Add([pscustomobject]@{name=$Name;passed=$false;error=$_.Exception.Message})}}
function Require([bool]$Value,[string]$Message){if(!$Value){throw $Message}}
function Reject([scriptblock]$Body){$rejected=$false;try{& $Body|Out-Null}catch{$rejected=$true};Require $rejected 'Invalid evidence was accepted.'}
function Native([string]$Body){'[2026.10.22-19.00.00:001][  1]LogTemp: Display: '+$Body+"`n"}
function Copy-Logs([hashtable]$Logs){$copy=@{};foreach($key in $Logs.Keys){$copy[$key]=$Logs[$key]};$copy}
$roles=@('host','client1','client2','client3')
$relic=@{}
for($i=0;$i -lt 4;++$i){
    $relic[$roles[$i]]=Native "BwayRelicFlight: Peer PASS witness=$($i+256) source=257 serial=42 stage=12 scenario=4 net=1 restored=1 stats=1"
    $serial=1
    foreach($stageCase in @(@(3,0),@(3,1),@(3,2),@(4,3),@(4,4),@(6,4),@(7,4),@(8,4),@(10,4),@(11,4))){$relic[$roles[$i]]+=Native "BwayRelicFlight: Peer PASS witness=$($i+256) source=257 serial=$serial stage=$($stageCase[0]) scenario=$($stageCase[1]) net=1 stats=1 grantCount=1";++$serial}
}
function LaunchFixture([int]$Scenario) {
    $hold=@(0.1,0.75,1.85)[$Scenario];$expected=800+1200*[math]::Min(1.0,[math]::Max(0.0,$hold/1.5))
    $aim=[double[]]@(0.5,0,([math]::Sqrt(3)/2));$initial=[double[]]@(($aim[0]*$expected),0,($aim[2]*$expected));$observed=$initial.Clone()
    $a=1.0;$b=[double[]]@(0,0,0);$steps=@()
    foreach($dt in @(0.025,0.026)) {
        $drag=1-1.5*$dt;$a*=$drag
        for($axis=0;$axis -lt 3;++$axis){$gravity=if($axis -eq 2){-980}else{0};$observed[$axis]=($observed[$axis]+$gravity*$dt)*$drag;$b[$axis]=($b[$axis]+$gravity*$dt)*$drag}
        $steps+=@{seconds=$dt;damping=1.5;gravity=@(0,0,-980)}
    }
    $speed=[math]::Sqrt($observed[0]*$observed[0]+$observed[2]*$observed[2])
    $p=[ordered]@{schema=1;scenario=$Scenario;source=257;multiplier=$a;seconds=0.051;expectedSpeed=$expected;minThrowSpeed=800;maxThrowSpeed=2000;maxChargeTime=1.5;holdSeconds=$hold;vectorError=0;pass=$true;observed=$observed;initial=$initial;aim=$aim;gravityVelocity=$b;steps=$steps;lastPhysicsFrame=100;sampleFrame=100;startSolverFrame=10;sampleSolverFrame=12;solverElapsed=0.051;solverMode='synchronous-single-step'}
    (Native ('BwayRelicFlight: LaunchPhysics '+($p|ConvertTo-Json -Depth 8 -Compress)))+
    (Native ('BwayRelicFlight: Launch scenario={0} source=257 observedSpeed={1:F3} expectedSpeed={2:F3} physicsTolerance=180 sample=post-physics-next-frame pass=1' -f $Scenario,$speed,$expected))+
    (Native ('BwayRelicThrow: RELEASE role=2 player=257 chargeSeconds={0:F6} speed={1:F6} aim=X=0.500 Y=0.000 Z=0.866 authoritative=1 accepted=1' -f [math]::Min($hold,1.5),$expected))
}
function Change-LaunchPhysics([hashtable]$Logs,[scriptblock]$Change) {
    $copy=Copy-Logs $Logs
    $copy.host=[regex]::Replace($copy.host,'(?m)(BwayRelicFlight: LaunchPhysics )(\{[^\r\n]+\})',{
        param($m)
        $p=$m.Groups[2].Value|ConvertFrom-Json
        if($p.scenario -eq 2){& $Change $p}
        $m.Groups[1].Value+($p|ConvertTo-Json -Depth 8 -Compress)
    })
    return $copy
}
function Consistent-WrongLaunch([hashtable]$Logs,[double]$Scale) {
    $copy=Change-LaunchPhysics $Logs {
        $p=$args[0]
        for($axis=0;$axis -lt 3;++$axis){$p.initial[$axis]*=$Scale;$p.observed[$axis]=$p.initial[$axis]*$p.multiplier+$p.gravityVelocity[$axis]}
        $p.vectorError=[math]::Abs($Scale-1)*$p.expectedSpeed
    }
    $line=@(Get-BwayNativeLines $copy.host 'BwayRelicFlight: LaunchPhysics ' | ForEach-Object {$_.Substring('BwayRelicFlight: LaunchPhysics '.Length)|ConvertFrom-Json} | Where-Object {$_.scenario -eq 2})[0]
    $raw=[math]::Sqrt($line.observed[0]*$line.observed[0]+$line.observed[1]*$line.observed[1]+$line.observed[2]*$line.observed[2])
    $copy.host=[regex]::Replace($copy.host,'(Launch scenario=2 source=257 observedSpeed=)[\d.]+',{param($m) $m.Groups[1].Value+$raw.ToString('F3',[cultureinfo]::InvariantCulture)})
    return $copy
}
for($scenario=0;$scenario -lt 3;++$scenario){$relic.host+=LaunchFixture $scenario}
$relic.host+=Native 'BwayRelicFlight: COMPLETE pass=1 peers=4 scenarios=5 remoteOwner=257 groundedDelta=0 airborneDelta=1 cleanup=1'
$combat=@{}
for($i=0;$i -lt 4;++$i){
    $role=$roles[$i];$id=256+$i;$combat[$role]=''
    for($sequence=1;$sequence -le 8;++$sequence){
        $health=if($sequence%2 -eq 0){90}else{100};$fired=[int]($i -eq [math]::Floor(($sequence-1)/2))
        $combat[$role]+=Native "BwayBuildableCombat: PeerComplete sequence=$sequence peer=$id samples=10 health=$health.000 fired=$fired ended=1"
        if($fired){$combat[$role]+=Native "BwayBuildableCombat: OwnerFire sequence=$sequence peer=$id activated=1 spec=GA_Test"}
    }
    $combat[$role]+=Native "BwayBuildableCombat: PeerReleased peer=$id"
}
$combat.host+=Native 'BwayBuildableCombat: Setup humans=4 remotes=3 held=4 round=1'
for($sequence=1;$sequence -le 8;++$sequence){$health=if($sequence%2 -eq 0){90}else{100};$enemy=[int]($sequence%2 -eq 0);$combat.host+=Native "BwayBuildableCombat: Verify sequence=$sequence netId=$($sequence+100) before=100.000 after=$health.000 enemy=$enemy pass=1"}
$combat.host+=Native 'BwayBuildableCombat: Complete reason=observed sequence=9 created=8 destroyed=8 restored=4 expired=0 held=4'
$siege=@{host=''}
foreach($body in @('Setup id=257 round=1 humans=4 remoteHumans=3 maxActive=1 maximum=10.000 speed=900.000 persists=1','Spawn ordinal=1 netId=100 name=Siege1 time=100.000','RoundRetained netId=100 round=2 quota=0 capAllows=0 time=108.000','Expired ordinal=1 netId=100 quota=0 capAllows=1 time=110.000','Spawn ordinal=2 netId=200 name=Siege2 time=111.000','Expired ordinal=2 quota=1 time=121.000','CleanupQuota count=0','Complete reason=observed stage=6 destroyed=0 restored=4 expired=4 held=8')){$siege.host+=Native "BwaySiegeAudit: $body"}
for($i=1;$i -le 3;++$i){$id=256+$i;$siege[$roles[$i]]=(Native "BwaySiegeAudit: PeerComplete local=$id id=257 identities=2 maxCount=1 survived=1 samples=50")+(Native "BwaySiegeAudit: PeerReleased local=$id")}

Check 'current counters schema preserves build/package caps and counts' {
    $c='{"caps":{"editorBuilds":7,"gameBuilds":2,"packageRuns":1},"counters":{"editorBuilds":7,"gameBuilds":0,"packageRuns":0,"automationRuns":5}}'|ConvertFrom-Json
    $before=$c.caps|ConvertTo-Json -Compress;$null=Get-BwayPackagedRunBudget $c 5
    Require (!$c.counters.PSObject.Properties['packagedRuns']) 'Read-only budget check mutated contract.'
    Require ((Add-BwayPackagedRun $c) -eq 1) 'Wrong new runtime count.'
    Require (($c.caps|ConvertTo-Json -Compress) -ceq $before) 'Caps changed.'
    Require ($c.counters.packageRuns -eq 0 -and $c.counters.editorBuilds -eq 7 -and $c.counters.automationRuns -eq 5) 'Unrelated counters changed.'
}
Check 'legacy counts schema increments without resetting history' {$c='{"counts":{"packagedRuns":8,"packageRuns":1},"caps":{"packageRuns":1}}'|ConvertFrom-Json;Require ((Add-BwayPackagedRun $c) -eq 9 -and $c.counts.packageRuns -eq 1) 'Legacy count lost.'}
Check 'dual schema never decreases either history' {$c='{"counts":{"packagedRuns":8},"counters":{"packagedRuns":10}}'|ConvertFrom-Json;$null=Add-BwayPackagedRun $c;Require ($c.counts.packagedRuns -eq 11 -and $c.counters.packagedRuns -eq 11) 'Dual history lost.'}
Check 'runtime cap rejects reservation but permits readback' {$c='{"counters":{"packagedRuns":2},"caps":{"packagedRuns":2}}'|ConvertFrom-Json;Reject {Add-BwayPackagedRun $c};Require ((Get-BwayPackagedRunBudget $c 0).current -eq 2) 'Readback at cap failed.'}
Check 'runtime cap checks all planned phases' {$c='{"counters":{"packagedRuns":1},"caps":{"packagedRuns":5}}'|ConvertFrom-Json;Reject {Get-BwayPackagedRunBudget $c 5}}
Check 'malformed counter fails closed' {Reject {Get-BwayPackagedRunBudget ('{"counts":{"packagedRuns":-1}}'|ConvertFrom-Json)}}
Check 'relic positive complete four-peer evidence' {Require (Assert-BwayFixtureEvidence RelicFlight $relic).passed 'Positive relic rejected.'}
Check 'relic missing final peer rejected' {$l=Copy-Logs $relic;$l.client3='';Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'relic duplicate completion rejected' {$l=Copy-Logs $relic;$l.host+=$l.host;Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'relic wrong rendered owner rejected' {$l=Copy-Logs $relic;$l.client1=$l.client1.Replace('witness=257','witness=259');Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'relic missing cancellation convergence rejected' {$l=Copy-Logs $relic;$l.client3=$l.client3.Replace('stage=4 scenario=3','stage=99 scenario=3');Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'relic mismatched scenario serial rejected' {$l=Copy-Logs $relic;$l.client2=$l.client2.Replace('serial=1 stage=3','serial=99 stage=3');Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'relic launch speed contradiction rejected' {$l=Copy-Logs $relic;$l.host=[regex]::Replace($l.host,'observedSpeed=[\d.]+','observedSpeed=1.000');Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'reconstruction marker omission rejected' {$l=Copy-Logs $relic;$l.host=$l.host.Replace('BwayRelicFlight: LaunchPhysics','Ignored: LaunchPhysics');Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'duplicate reconstruction rejected' {$l=Copy-Logs $relic;$l.host+=LaunchFixture 2;Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'wrong reconstructed initial velocity rejected' {$l=Change-LaunchPhysics $relic {$args[0].initial[0]+=400};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'missing solver step rejected' {$l=Change-LaunchPhysics $relic {$args[0].steps=@()};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'negative solver step rejected' {$l=Change-LaunchPhysics $relic {$args[0].steps[0].seconds=-0.01};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'unbounded solver step rejected' {$l=Change-LaunchPhysics $relic {$args[0].steps[0].seconds=0.2};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'wrong damping evidence rejected' {$l=Change-LaunchPhysics $relic {$args[0].steps[0].damping=0};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'wrong gravity evidence rejected' {$l=Change-LaunchPhysics $relic {$args[0].steps[0].gravity[2]=0};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'stale observed frame rejected' {$l=Change-LaunchPhysics $relic {$args[0].sampleFrame=101};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'untracked solver frame rejected' {$l=Change-LaunchPhysics $relic {$args[0].sampleSolverFrame=13};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'untracked solver time rejected' {$l=Change-LaunchPhysics $relic {$args[0].solverElapsed=0.1};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'unsupported solver mode rejected' {$l=Change-LaunchPhysics $relic {$args[0].solverMode='async'};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'nonfinite numeric evidence rejected' {$l=Change-LaunchPhysics $relic {$args[0].seconds='NaN'};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'incorrect launch aim rejected' {$l=Change-LaunchPhysics $relic {$args[0].aim=@(0,1,0)};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'missing authority release rejected' {$l=Copy-Logs $relic;$l.host=$l.host.Replace('BwayRelicThrow: RELEASE','Ignored: RELEASE');Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'wrong authority release speed rejected' {$l=Copy-Logs $relic;$l.host=$l.host.Replace('speed=2000.000000','speed=1500.000000');Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'contradictory charge mapping rejected' {$l=Change-LaunchPhysics $relic {$args[0].maxChargeTime=3};Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'consistent half-speed launch fails initial contract' {$l=Consistent-WrongLaunch $relic 0.5;$reason='';try{Assert-BwayFixtureEvidence RelicFlight $l|Out-Null}catch{$reason=$_.Exception.Message};Require ($reason -eq 'Initial launch contract violated.') $reason}
Check 'consistent reversed launch fails direction contract' {$l=Consistent-WrongLaunch $relic -1;$reason='';try{Assert-BwayFixtureEvidence RelicFlight $l|Out-Null}catch{$reason=$_.Exception.Message};Require ($reason -eq 'Initial launch contract violated.') $reason}
Check 'command-line echoed completion is not evidence' {$l=Copy-Logs $relic;$l.host=$l.host.Replace('LogTemp: Display: BwayRelicFlight: COMPLETE','LogInit: Command Line: -testexit="BwayRelicFlight: COMPLETE');Reject {Assert-BwayFixtureEvidence RelicFlight $l}}
Check 'buildable eight cases with all four peers and held4 accepted' {Require (Assert-BwayFixtureEvidence BuildableCombat $combat).passed 'Positive combat rejected.'}
Check 'buildable missing host cleanup rejected' {$l=Copy-Logs $combat;$l.host=$l.host.Replace('BwayBuildableCombat: PeerReleased','Ignored: PeerReleased');Reject {Assert-BwayFixtureEvidence BuildableCombat $l}}
Check 'buildable stale peer health rejected' {$l=Copy-Logs $combat;$l.client2=$l.client2.Replace('health=90.000','health=100.000');Reject {Assert-BwayFixtureEvidence BuildableCombat $l}}
Check 'buildable wrong firing ownership rejected' {$l=Copy-Logs $combat;$l.client2=$l.client2.Replace('fired=0','fired=1');Reject {Assert-BwayFixtureEvidence BuildableCombat $l}}
Check 'buildable unrestored pawn rejected' {$l=Copy-Logs $combat;$l.host=$l.host.Replace('restored=4 expired=0 held=4','restored=3 expired=0 held=4');Reject {Assert-BwayFixtureEvidence BuildableCombat $l}}
Check 'siege two distinct lifetimes and held8 accepted' {Require (Assert-BwayFixtureEvidence Siege $siege).passed 'Positive siege rejected.'}
Check 'siege duplicate replicated identity rejected' {$l=Copy-Logs $siege;$l.host=$l.host.Replace('ordinal=2 netId=200','ordinal=2 netId=100');Reject {Assert-BwayFixtureEvidence Siege $l}}
Check 'siege missing witness release rejected' {$l=Copy-Logs $siege;$l.client3=$l.client3.Replace('BwaySiegeAudit: PeerReleased','Ignored: PeerReleased');Reject {Assert-BwayFixtureEvidence Siege $l}}
Check 'siege missing quota reset rejected' {$l=Copy-Logs $siege;$l.host=$l.host.Replace('CleanupQuota count=0','CleanupQuota count=1');Reject {Assert-BwayFixtureEvidence Siege $l}}
Check 'PowerShell files parse cleanly' {
    foreach($name in @('Run-Goal4PresentationTransport.ps1','PackagedTransportCommon.ps1','Run-AllKitOwnerTransport.ps1','Test-Goal4PresentationTransport.ps1')){$tokens=$null;$errors=$null;$null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name),[ref]$tokens,[ref]$errors);Require (!$errors.Count) ($errors.Message -join '; ')}
}
Check 'ValidateOnly returns before any launch/reservation/output-directory mutation' {
    $path=Join-Path $PSScriptRoot 'Run-Goal4PresentationTransport.ps1';$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
    $gate=@($ast.EndBlock.Statements|Where-Object {$_.Extent.Text -match '^if\(\$ValidateOnly\)'});Require ($gate.Count -eq 1 -and $gate[0].Extent.Text -match ';return\}') 'Read-only return boundary missing.'
    $mutations=@($ast.EndBlock.Statements|Where-Object {$_ -isnot [Management.Automation.Language.FunctionDefinitionAst] -and $_.Extent.StartOffset -lt $gate[0].Extent.StartOffset -and $_.Extent.Text -match '\b(Start-Process|Set-Content|Reserve-RuntimeRun|Add-BwayPackagedRun|New-Item)\b'})
    Require (!$mutations.Count) 'A mutation moved ahead of ValidateOnly.'
    $text=[IO.File]::ReadAllText($path);Require ($text -notmatch '(?i)LAN-20\d{6}-\d{2}') 'A historical candidate was pinned.'
}
Check 'ValidateOnly executes read-only plan with isolated preflight stubs' {
    # Exercise the actual top-level path in a disposable synthetic project. Only the
    # established candidate/slide preflights are stubbed; this is not package proof.
    $fixtureRoot=[IO.Path]::GetFullPath($OutputJson+'.inputs')
    Require (!(Test-Path -LiteralPath $fixtureRoot)) 'Synthetic fixture directory already exists.'
    $fixtureScripts=Join-Path $fixtureRoot 'Scripts/Test';$fixtureSaved=Join-Path $fixtureRoot 'Saved'
    $null=New-Item -ItemType Directory -Path $fixtureScripts,$fixtureSaved
    foreach($name in @('Run-Goal4PresentationTransport.ps1','PackagedTransportCommon.ps1')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $fixtureScripts $name)}
    @'
param($Candidate,$Attempt,$ExpectedLagMs,$ExpectedExeSHA256,$PackageResultPath,$GateContractPath,$EvidenceDirectory,[switch]$ValidateOnly)
if(!$ValidateOnly){throw 'Offline stub rejects all runtime launches.'}
if($ExpectedLagMs -ne 100){throw 'Wrong lag plan.'}
[pscustomobject]@{status='preflight-only';effectiveSourceIdentity='offline-only'}|ConvertTo-Json -Compress
'@|Set-Content -LiteralPath (Join-Path $fixtureScripts 'Run-AllKitOwnerTransport.ps1')
    @'
param($Candidate,$Attempt,$OwnerTargetHz,$PackageResultPath,$PythonExecutable,[switch]$ReviewRegression,[switch]$ValidateOnly)
if(!$ValidateOnly -or !$ReviewRegression -or $OwnerTargetHz -ne 30){throw 'Offline stub rejects non-review or runtime execution.'}
[pscustomobject]@{status='validated-artifact-and-paths'}|ConvertTo-Json -Compress
'@|Set-Content -LiteralPath (Join-Path $fixtureSaved 'run-slide-trajectory.ps1')
    foreach($name in @('Verify-AllKitOwnerTransport.py','Verify-SlideReview.py')){'# Offline existence stub; never invoked.'|Set-Content -LiteralPath (Join-Path $fixtureScripts $name)}
    $contractPath=Join-Path $fixtureSaved 'contract.json';$manifestPath=Join-Path $fixtureSaved 'manifest.json';$packagePath=Join-Path $fixtureSaved 'package.json'
    '{"candidate":"LAN-20990101-01","caps":{"packageRuns":1,"packagedRuns":5},"counters":{"packageRuns":0}}'|Set-Content -LiteralPath $contractPath
    [ordered]@{files=@([ordered]@{path='Plugins/Test/Private/Development/BwayAllKitCombatProbe.cpp';sha256=('a'*64)})}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $manifestPath
    [ordered]@{sourceManifest=$manifestPath}|ConvertTo-Json|Set-Content -LiteralPath $packagePath
    $before=(Get-FileHash -LiteralPath $contractPath).Hash
    $args=@{Candidate='LAN-20990101-01';Attempt='offline';ExpectedExeSHA256=('a'*64);PackageResultPath=$packagePath;GateContractPath=$contractPath;EvidenceDirectory='Saved/Logs';PythonExecutable=(Get-Process -Id $PID).Path;ValidateOnly=$true}
    $runner=Join-Path $fixtureScripts 'Run-Goal4PresentationTransport.ps1'
    $plan=& $runner @args|ConvertFrom-Json
    Require ($plan.status -eq 'preflight-only' -and $plan.runtimeRunsPlanned -eq 5 -and $plan.packetLagMs -eq 100 -and $plan.topology -eq '2 D3D12 + 2 NullRHI') 'Wrong validated topology.'
    Require (($plan.phases -join ',') -eq 'AllKit,SlideReview,RelicFlight,BuildableCombat,Siege') 'Default phase coverage/order changed.'
    Require ($plan.nativePhaseBotCounts.BuildableCombat -eq 4 -and $plan.nativePhaseBotCounts.RelicFlight -eq 0 -and $plan.nativePhaseBotCounts.Siege -eq 0) 'Native fixture population requirements changed.'
    Require ((Get-FileHash -LiteralPath $contractPath).Hash -eq $before) 'ValidateOnly changed the contract.'
    Require (!(Test-Path -LiteralPath (Split-Path -Parent $plan.output))) 'ValidateOnly created runtime evidence.'
    $duplicateArgs=$args.Clone();$duplicateArgs.Phases=@('Siege','Siege');Reject {& $runner @duplicateArgs}
    $null=New-Item -ItemType Directory -Path (Split-Path -Parent $plan.output)
    Reject {& $runner @args}
}
$passed=@($checks|Where-Object {!$_.passed}).Count -eq 0
[ordered]@{schema='goal4-transport-offline-1';passed=$passed;scope='Offline controls only. No Unreal process, candidate preflight, gameplay, pixels, audio or package acceptance.';generatedUtc=[datetime]::UtcNow.ToString('o');checks=@($checks.ToArray())}|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $OutputJson -Encoding UTF8
[ordered]@{passed=$passed;checks=$checks.Count;failures=@($checks|Where-Object {!$_.passed});output=$OutputJson}|ConvertTo-Json -Depth 5
if(!$passed){exit 1}
