<# Consolidated same-PC transport on one explicit frozen candidate. No build, cook or editor actions.
   Fresh topology per native fixture: hero selection and client auto-exit are fixture-owned.
   ValidateOnly performs immutable provenance/path/budget checks without consuming an attempt.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^LAN-[0-9]{8}-[0-9]{2}$')][string]$Candidate,
    [Parameter(Mandatory)][ValidatePattern('^[a-z0-9][a-z0-9-]{0,27}$')][string]$Attempt,
    [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ExpectedExeSHA256,
    [Parameter(Mandatory)][string]$PackageResultPath,
    [Parameter(Mandatory)][string]$GateContractPath,
    [string]$EvidenceDirectory='Saved/Logs/goal4-feel-20261022',
    [ValidateSet('AllKit','SlideReview','RelicFlight','BuildableCombat','Siege')][string[]]$Phases=@('AllKit','SlideReview','RelicFlight','BuildableCombat','Siege'),
    [ValidateRange(180,600)][int]$PhaseTimeoutSeconds=560,
    [string]$PythonExecutable='python',
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $PSScriptRoot 'PackagedTransportCommon.ps1')
$allKitRunner=Join-Path $PSScriptRoot 'Run-AllKitOwnerTransport.ps1'
$allKitVerifier=Join-Path $PSScriptRoot 'Verify-AllKitOwnerTransport.py'
$slideRunner=Join-Path $root 'Saved/run-slide-trajectory.ps1'
$evidence=[IO.Path]::GetFullPath((Join-Path $root $EvidenceDirectory))
if(!$evidence.StartsWith((Join-Path $root 'Saved')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Evidence must remain under project Saved.'}
if(!$Phases.Count -or @($Phases|Sort-Object -Unique).Count -ne $Phases.Count){throw 'Select nonempty, unique phases.'}
$phaseOrder=@('AllKit','SlideReview','RelicFlight','BuildableCombat','Siege')
$Phases=@($phaseOrder|Where-Object {$_ -in $Phases})
$token="goal4-$Candidate-$Attempt"
$directory=Join-Path $evidence $token
if(Test-Path -LiteralPath $directory){throw 'Preserve the existing consolidated attempt; choose a fresh Attempt.'}
$contractPath=(Resolve-Path -LiteralPath $GateContractPath).Path
$packagePath=(Resolve-Path -LiteralPath $PackageResultPath).Path
$contract=Get-Content -LiteralPath $contractPath -Raw|ConvertFrom-Json
$budget=Get-BwayPackagedRunBudget $contract $Phases.Count
$null=Get-Command $PythonExecutable -ErrorAction Stop
foreach($dependency in @($allKitRunner,$allKitVerifier,$slideRunner,(Join-Path $PSScriptRoot 'Verify-SlideReview.py'))){if(!(Test-Path -LiteralPath $dependency -PathType Leaf)){throw "Missing current helper: $dependency"}}
$allKitAttempt="$Attempt-allkit"
$slideAttempt="$Attempt-slide"
$allKitResult=Join-Path $evidence "allkit-$Candidate-$allKitAttempt-result.json"
$allKitAnalysis=Join-Path $directory 'allkit-verification.json'
$slideResult=Join-Path $root "Saved/Logs/slide-trajectory-$Candidate-$slideAttempt/result.json"
# The established AllKit preflight binds both successful target builds, archived source,
# snapshot, current runtime inputs, every packaged byte, exact executable, port and process ownership.
$preflightArgs=@{Candidate=$Candidate;Attempt="$Attempt-guard";ExpectedLagMs=100;ExpectedExeSHA256=$ExpectedExeSHA256;PackageResultPath=$packagePath;GateContractPath=$contractPath;EvidenceDirectory=$EvidenceDirectory;ValidateOnly=$true}
function Assert-CurrentCandidate {
    if($plan -and ((Get-FileHash -LiteralPath $packagePath).Hash -ne $plan.packageResultSHA256)){throw 'Package result changed during consolidated run.'}
    & $allKitRunner @preflightArgs
}
$preflight=Assert-CurrentCandidate|ConvertFrom-Json
if($preflight.status -ne 'preflight-only'){throw 'Immutable candidate preflight did not pass.'}
if('AllKit' -in $Phases){$null=& $allKitRunner -Candidate $Candidate -Attempt $allKitAttempt -ExpectedLagMs 100 -ExpectedExeSHA256 $ExpectedExeSHA256 -PackageResultPath $packagePath -GateContractPath $contractPath -EvidenceDirectory $EvidenceDirectory -ValidateOnly}
if('SlideReview' -in $Phases){$slidePreflight=& $slideRunner -Candidate $Candidate -Attempt $slideAttempt -OwnerTargetHz 30 -PackageResultPath $packagePath -ReviewRegression -PythonExecutable $PythonExecutable -ValidateOnly|ConvertFrom-Json;if($slidePreflight.status -ne 'validated-artifact-and-paths'){throw 'Slide preflight did not pass.'}}
$package=Get-Content -LiteralPath $packagePath -Raw|ConvertFrom-Json
$manifest=Get-Content -LiteralPath $package.sourceManifest -Raw|ConvertFrom-Json
$probe=@($manifest.files|Where-Object path -Match '/Private/Development/BwayAllKitCombatProbe\.cpp$')
if($probe.Count -ne 1){throw 'Exactly one frozen AllKit source identity required.'}
$exe=Join-Path $root "Saved/StagedBuilds/$Candidate/Windows/ProjectB/Binaries/Win64/LyraGame.exe"
$plan=[ordered]@{schema='goal4-presentation-transport-1';status='preflight-only';candidate=$Candidate;attempt=$Attempt;token=$token;phases=$Phases;output=(Join-Path $directory 'result.json');topology='2 D3D12 + 2 NullRHI';packetLagMs=100;frameCap=30;runtimeRunsBefore=$budget.current;runtimeRunsPlanned=$Phases.Count;executable=$exe;executableSHA256=$ExpectedExeSHA256;packageResult=$packagePath;packageResultSHA256=(Get-FileHash -LiteralPath $packagePath).Hash;effectiveSourceIdentity=$preflight.effectiveSourceIdentity;gateContract=$contractPath;scope='Sequential owned four-process sessions on the same frozen candidate. Synthetic transport and lifecycle only; no audio, pixels, physical-input, haptics or second-PC LAN acceptance. Existing five-case Dorado PIE proof remains separate.'}
$plan.nativePhaseBotCounts=[ordered]@{RelicFlight=0;BuildableCombat=4;Siege=0}
$plan.buildablePopulationScope='BuildableCombat native fixture requires eight pawns: four human processes plus four bots. RelicFlight requires exactly four human players. Bot count does not change the four-process transport topology.'
if($ValidateOnly){$plan|ConvertTo-Json -Depth 7;return}

function Reserve-RuntimeRun {
    $latest=Get-Content -LiteralPath $contractPath -Raw|ConvertFrom-Json
    if($latest.candidate -ne $Candidate){throw 'Gate candidate changed before runtime reservation.'}
    $number=Add-BwayPackagedRun $latest
    $latest|ConvertTo-Json -Depth 64|Set-Content -LiteralPath $contractPath -Encoding UTF8
    $number
}

function Invoke-NativePhase([string]$Phase) {
    $phaseDir=Join-Path $directory $Phase
    if(Test-Path -LiteralPath $phaseDir){throw 'Phase evidence must be fresh.'}
    $null=New-Item -ItemType Directory -Path $phaseDir
    $runNumber=Reserve-RuntimeRun
    $numBots=[int]$plan.nativePhaseBotCounts[$Phase]
    $owned=@{};$records=[Collections.Generic.List[object]]::new();$texts=@{};$cursors=@{}
    $watch=[Diagnostics.Stopwatch]::StartNew();$errorText=$null;$proof=$null;$hostReady=$false;$connected=$false;$clientsExited=$false;$settingsRestored=$false
    $nativeError='(?m)^(?:\[[^\]\r\n]*\]\s*)*(?:[A-Za-z_]\w*:\s*(?:Error|Fatal):|Log\w*(?:Error|Fatal):|(?:Fatal error|Assertion failed|Ensure condition failed):)[^\r\n]*'
    $settings=Join-Path $root "Saved/StagedBuilds/$Candidate/Windows/ProjectB/Saved/Config/Windows/GameUserSettings.ini"
    $settingsExisted=Test-Path -LiteralPath $settings -PathType Leaf
    $settingsBytes=if($settingsExisted){[IO.File]::ReadAllBytes($settings)}else{$null}
    if($settingsExisted){[IO.File]::WriteAllBytes((Join-Path $phaseDir 'settings.before.ini'),$settingsBytes)}
    $result=[ordered]@{phase=$Phase;status='running';packagedRunNumber=$runNumber;numBots=$numBots;startedUtc=[datetime]::UtcNow.ToString('o');processes=@();hostReady=$false;fourAliveAfterConnect=$false;clientExitZero=$false;allOwnedExited=$false;settingsRestored=$false;proof=$null;warnings=@();runError=$null}
    function Save-Phase { $result.processes=@($records.ToArray());$result|ConvertTo-Json -Depth 14|Set-Content -LiteralPath (Join-Path $phaseDir 'result.json') -Encoding UTF8 }
    function Read-OwnedLogs {
        foreach($r in $records){
            if(!(Test-Path -LiteralPath $r.log)){continue}
            $stream=[IO.File]::Open($r.log,'Open','Read','ReadWrite')
            try{
                if($stream.Length -lt $cursors[$r.role]){throw 'Owned log rotated.'}
                $null=$stream.Seek($cursors[$r.role],'Begin');$reader=[IO.StreamReader]::new($stream);$texts[$r.role]+=$reader.ReadToEnd();$cursors[$r.role]=$stream.Position
            }finally{$stream.Dispose()}
            $bad=[regex]::Match($texts[$r.role],$nativeError)
            if($bad.Success){throw "$($r.role): $($bad.Value)"}
            $terminalFailure=[regex]::Match($texts[$r.role],'(?m)^(?:\[[^\]\r\n]*\]\s*)*LogTemp: Display: Bway(?:BuildableCombat|SiegeAudit): Complete reason=(?!observed\b)[^\r\n]+')
            if($terminalFailure.Success){throw $terminalFailure.Value}
        }
    }
    function Wait-Owned([scriptblock]$Condition,[int]$Seconds,[switch]$RequireAllAlive) {
        $deadline=[math]::Min($PhaseTimeoutSeconds,$watch.Elapsed.TotalSeconds+$Seconds)
        while($watch.Elapsed.TotalSeconds -lt $deadline){
            Read-OwnedLogs
            if($owned.host.HasExited){throw 'Host exited before client cleanup.'}
            foreach($r in $records|Where-Object role -ne 'host'){
                $p=$owned[$r.role]
                if($p.HasExited -and ($RequireAllAlive -or $p.ExitCode -ne 0)){throw "$($r.role) exited before required checkpoint or with nonzero code."}
            }
            if(& $Condition){return}
            Start-Sleep -Milliseconds 250
        }
        throw 'Owned phase deadline expired.'
    }
    function Start-Native([string]$Role,[string]$URL) {
        $userDir=Join-Path $phaseDir "user-$Role";$log=Join-Path $phaseDir "$Role.log"
        if((Test-Path -LiteralPath $userDir) -or (Test-Path -LiteralPath $log)){throw 'Fresh process user/log paths required.'}
        $null=New-Item -ItemType Directory -Path $userDir
        $args=@($URL,'-unattended','-nosplash','-nop4','-nosound','-MaxFPS=30','-PktLag=100','-ExecCmds="t.MaxFPS 30"','-ExecCmds="Net PktLag=100"','-ini:GameUserSettings:[/Script/LyraGame.LyraSettingsLocal]:FrameRateLimit=30.0',"-abslog=`"$log`"","-UserDir=`"$userDir`"","-enginesaveddirsuffix=$token-$Phase-$Role")
        if($Role -eq 'host'){
            $command=switch($Phase){RelicFlight{'bway.Test.RelicFlight'} BuildableCombat{'bway.Test.BuildableCombatAudit'} Siege{'bway.Test.SiegeLifecycleAudit'}}
            $args+="-ExecCmds=$command"
        }elseif($Phase -eq 'RelicFlight'){$args+='-RelicFlightAutoExit'}else{
            $marker=if($Phase -eq 'Siege'){'BwaySiegeAudit: PeerReleased'}else{'BwayBuildableCombat: PeerReleased'}
            $args+="-testexit=`"$marker`""
        }
        $rendered=$Role -in @('host','client1')
        if($rendered){$args+=@('-d3d12','-Windowed','-ResX=640','-ResY=360','-ForceRes','-WinY=20',$(if($Role -eq 'host'){'-WinX=20'}else{'-WinX=680'}))}else{$args+='-NullRHI'}
        $p=Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory (Split-Path -Parent $exe) -WindowStyle Hidden -PassThru
        $null=$p.Handle;$owned[$Role]=$p;$texts[$Role]='';$cursors[$Role]=0L
        $r=[pscustomobject]@{role=$Role;processId=$p.Id;args=$args;rendered=$rendered;log=$log;userDirectory=$userDir;normalExit=$false;closeRequested=$false;forcedKill=$false;exited=$false;exitCode=$null;logSHA256=$null;logBytes=0;observedExecutable=$null}
        $records.Add($r);Save-Phase
        for($i=0;$i -lt 20 -and !$r.observedExecutable;++$i){try{$r.observedExecutable=$p.MainModule.FileName}catch{};if(!$r.observedExecutable){Start-Sleep -Milliseconds 250}}
        if(!$r.observedExecutable -or [IO.Path]::GetFullPath($r.observedExecutable) -ne [IO.Path]::GetFullPath($exe)){throw 'Owned process executable differs from frozen candidate.'}
    }
    try {
        Save-Phase
        Start-Native host ('/BreakawayCore/Maps/L_BW_HeroSelect_Staging?listen?HeroSelectStaging=1?HeroSelectTargetMap=/BreakawayCore/Maps/L_BW_Dorado?HeroSelectTargetExperience=B_BW_Experience_CaptureTheRelic?Experience=B_BW_Experience_HeroSelectStaging?NumBots='+$numBots+'?PointsToWin=99?RoundDuration=900?WarmupDuration=3?PostRoundDuration=8')
        Wait-Owned { $texts.host -match '(?m)^(?:\[[^\]\r\n]*\]\s*)*LogNet: (?:Display: )?[^\r\n]*GameNetDriver[^\r\n]*listening on port 7777\b' } 60 -RequireAllAlive
        $endpoints=@(Get-NetUDPEndpoint -ErrorAction Stop|Where-Object LocalPort -eq 7777)
        if(!$endpoints.Count -or @($endpoints|Where-Object OwningProcess -ne $owned.host.Id).Count){throw 'Host does not own UDP 7777.'}
        $hostReady=$true;$result.listenEndpoints=@($endpoints|Select-Object LocalAddress,LocalPort,OwningProcess)
        # Complete the first remote join before admitting witnesses; native fixtures select the lowest remote ID.
        for($index=1;$index -le 3;++$index){
            Start-Native "client$index" '127.0.0.1:7777'
            Wait-Owned { [regex]::Matches($texts.host,'(?m)^(?:\[[^\]\r\n]*\]\s*)*LogNet: (?:Display: )?Join succeeded:').Count -ge $index } 45 -RequireAllAlive
        }
        $connected=$owned.Count -eq 4 -and @($owned.Values|Where-Object HasExited).Count -eq 0
        if(!$connected){throw 'Four connected live processes were not observed.'}
        $completion=switch($Phase){RelicFlight{'BwayRelicFlight: COMPLETE pass=1'} BuildableCombat{'BwayBuildableCombat: Complete reason=observed'} Siege{'BwaySiegeAudit: Complete reason=observed'}}
        Wait-Owned { @(Get-BwayNativeLines $texts.host $completion).Count -eq 1 } $PhaseTimeoutSeconds
        Wait-Owned { @($records|Where-Object {$_.role -ne 'host' -and !$owned[$_.role].HasExited}).Count -eq 0 } 40
        $clientsExited=$true
        foreach($r in $records|Where-Object role -ne 'host'){if($owned[$r.role].ExitCode -ne 0){throw 'Client cleanup exit was nonzero.'};$r.normalExit=$true}
        Read-OwnedLogs
        $proof=Assert-BwayFixtureEvidence $Phase $texts
        foreach($r in $records){if($texts[$r.role] -notmatch '(?m)^(?:\[[^\]\r\n]*\]\s*)*LogNet: (?:Display: )?PktLag set to 100\b'){throw "$($r.role) lacks emitted lag configuration evidence."}}
    } catch { $errorText=$_.Exception.Message }
    finally {
        # Client result delivery always precedes host teardown on success. Failed sessions are cleaned up, never accepted.
        foreach($r in @($records|Sort-Object {if($_.role -eq 'host'){1}else{0}})){
            $p=$owned[$r.role]
            if(!$p.HasExited){$r.closeRequested=$true;try{[void]$p.CloseMainWindow();[void]$p.WaitForExit(5000)}catch{};if(!$p.HasExited){$r.forcedKill=$true;try{$p.Kill();[void]$p.WaitForExit(5000)}catch{}}}
            $r.exited=$p.HasExited;if($r.exited){$r.exitCode=$p.ExitCode}
        }
        # Native failure collection must not skip independent log hashing/settings recovery.
        try{Read-OwnedLogs}catch{if(!$errorText){$errorText=$_.Exception.Message}}
        try{
            foreach($r in $records){if(Test-Path -LiteralPath $r.log){$r.logSHA256=(Get-FileHash -LiteralPath $r.log).Hash;$r.logBytes=(Get-Item -LiteralPath $r.log).Length}}
            $remaining=@(Get-CimInstance Win32_Process|Where-Object {$_.Name -match '^(LyraGame|UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool)\.exe$'})
            if($remaining.Count){throw 'Settings restoration withheld: Unreal process remains.'}
            if(Test-Path -LiteralPath $settings){[IO.File]::WriteAllBytes((Join-Path $phaseDir 'settings.after.ini'),[IO.File]::ReadAllBytes($settings))}
            if($settingsExisted){[IO.File]::WriteAllBytes($settings,$settingsBytes);$settingsRestored=(Get-FileHash -LiteralPath $settings).Hash -eq (Get-FileHash -LiteralPath (Join-Path $phaseDir 'settings.before.ini')).Hash}else{if(Test-Path -LiteralPath $settings){Remove-Item -LiteralPath $settings};$settingsRestored=!(Test-Path -LiteralPath $settings)}
        }catch{if(!$errorText){$errorText=$_.Exception.Message}}
        $result.hostReady=$hostReady;$result.fourAliveAfterConnect=$connected;$result.clientExitZero=$clientsExited;$result.settingsRestored=$settingsRestored
        $result.allOwnedExited=$records.Count -eq 4 -and @($records|Where-Object {!$_.exited}).Count -eq 0
        $result.proof=$proof;$result.runError=$errorText;$result.elapsedSeconds=$watch.Elapsed.TotalSeconds;$result.finishedUtc=[datetime]::UtcNow.ToString('o')
        $result.warnings=@(foreach($r in $records){foreach($warning in [regex]::Matches($texts[$r.role],'(?m)^(?:\[[^\]\r\n]*\]\s*)*\w+: Warning:[^\r\n]*')){[pscustomobject]@{role=$r.role;line=$warning.Value}}})
        $result.status=if(!$errorText -and $hostReady -and $connected -and $clientsExited -and $result.allOwnedExited -and $settingsRestored -and $proof.passed){'passed-transport-scope'}else{'failed-transport-scope'}
        Save-Phase
    }
    [pscustomobject]@{phase=$Phase;status=$result.status;result=(Join-Path $phaseDir 'result.json');runError=$errorText}
}

$null=New-Item -ItemType Directory -Path $directory
$plan.status='running';$plan.startedUtc=[datetime]::UtcNow.ToString('o');$plan.results=@();$plan.runError=$null
$plan.helpers=@(foreach($path in @($PSCommandPath,(Join-Path $PSScriptRoot 'PackagedTransportCommon.ps1'),$allKitRunner,$allKitVerifier,$slideRunner,(Join-Path $PSScriptRoot 'Verify-SlideReview.py'))){[ordered]@{path=$path;sha256=(Get-FileHash -LiteralPath $path).Hash}})
function Save-Consolidated { $plan|ConvertTo-Json -Depth 16|Set-Content -LiteralPath $plan.output -Encoding UTF8 }
try {
    Save-Consolidated
    foreach($phase in $Phases){
        $null=Assert-CurrentCandidate
        if($phase -eq 'AllKit'){
            $null=& $allKitRunner -Candidate $Candidate -Attempt $allKitAttempt -ExpectedLagMs 100 -ExpectedExeSHA256 $ExpectedExeSHA256 -PackageResultPath $packagePath -GateContractPath $contractPath -EvidenceDirectory $EvidenceDirectory
            if(!(Test-Path -LiteralPath $allKitResult)){throw 'AllKit did not preserve a result.'}
            $capture=Get-Content -LiteralPath $allKitResult -Raw|ConvertFrom-Json
            if($capture.status -ne 'passed-transport-scope'){throw "AllKit failed: $($capture.runError)"}
            $null=& $PythonExecutable $allKitVerifier $allKitResult --expected-lag 100 --reviewed-native-sha256 $probe[0].sha256 --controls --output $allKitAnalysis
            if($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $allKitAnalysis)){throw 'AllKit strict verifier failed.'}
            $analysis=Get-Content -LiteralPath $allKitAnalysis -Raw|ConvertFrom-Json
            if(!$analysis.passed){throw 'AllKit strict evidence/control verdict failed.'}
            $entry=[pscustomobject]@{phase=$phase;status='passed-transport-scope';result=$allKitResult;analysis=$allKitAnalysis}
        }elseif($phase -eq 'SlideReview'){
            $null=Reserve-RuntimeRun
            $null=& $slideRunner -Candidate $Candidate -Attempt $slideAttempt -OwnerTargetHz 30 -PackageResultPath $packagePath -ReviewRegression -PythonExecutable $PythonExecutable -TimeoutSeconds $PhaseTimeoutSeconds
            if(!(Test-Path -LiteralPath $slideResult)){throw 'Slide did not preserve a result.'}
            $capture=Get-Content -LiteralPath $slideResult -Raw|ConvertFrom-Json
            if($capture.status -ne 'passed-slide-review-scope'){throw "Slide failed: $($capture.firstCausalError)"}
            $entry=[pscustomobject]@{phase=$phase;status='passed-transport-scope';result=$slideResult;analysis=$capture.analysis}
        }else{$entry=Invoke-NativePhase $phase}
        $plan.results+= $entry;Save-Consolidated
        if($entry.status -ne 'passed-transport-scope'){throw "$phase failed: $($entry.runError)"}
        # No new phase starts until old exact handles exited and immutable guards pass again.
        $null=Assert-CurrentCandidate
    }
    $plan.status=if($Phases.Count -eq $phaseOrder.Count){'passed-consolidated-transport-scope'}else{'passed-selected-transport-scope'}
}catch{$plan.status='failed-transport-scope';$plan.runError=$_.Exception.Message}
finally{$plan.finishedUtc=[datetime]::UtcNow.ToString('o');Save-Consolidated}
[ordered]@{status=$plan.status;result=$plan.output;runError=$plan.runError}|ConvertTo-Json -Compress
if($plan.status -eq 'failed-transport-scope'){exit 1}
