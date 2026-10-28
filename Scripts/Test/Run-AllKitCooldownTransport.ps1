<# One complete 16-family run. Rotations are explicit, never launched automatically. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^LAN-[0-9]{8}-[0-9]+$')][string]$Candidate,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,60}$')][string]$Attempt,
    [Parameter(Mandatory)][ValidateSet('Rawlins','Alona','Korryn')][string]$RenderedOwnerHero,
    [Parameter(Mandatory)][ValidateSet(0,100,200)][int]$ExpectedLagMs,
    [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ExpectedExeSHA256,
    [Parameter(Mandatory)][string]$PackageResultPath,
    [Parameter(Mandatory)][string]$ConfigurationPath,
    [Parameter(Mandatory)][string]$GateContractPath,
    [string]$EvidenceDirectory='Saved/Logs/goal1-review-20261016',
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$evidence=[IO.Path]::GetFullPath((Join-Path $root $EvidenceDirectory))
if(!$evidence.StartsWith((Join-Path $root 'Saved')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Evidence must remain under project Saved.'}
$token="$Candidate-$Attempt"; $prefix=Join-Path $evidence "allkit-cooldown-$token"
$resultPath="$prefix-result.json"; $artifactPath="$prefix-provenance.json"
$roles=@('host','client1','client2','client3'); $heroByRole=@{host='Argus';client1='Rawlins';client2='Alona';client3='Korryn'}
$renderedRole=@{Rawlins='client1';Alona='client2';Korryn='client3'}[$RenderedOwnerHero]
$logs=@{}; foreach($role in $roles){$logs[$role]="$prefix-$role.log"}
foreach($path in @($resultPath,$artifactPath,"$prefix-users","$prefix-brief.json","$prefix-settings.before.ini","$prefix-settings.after.ini")+@($logs.Values)){
    if(Test-Path -LiteralPath $path){throw "Preserve existing attempt: $path"}
}
$guardArguments=@{Candidate=$Candidate;PackageResultPath=$PackageResultPath;ConfigurationPath=$ConfigurationPath;ExpectedExeSHA256=$ExpectedExeSHA256}
$provenance=& "$PSScriptRoot/Assert-AllKitCooldownCandidate.ps1" @guardArguments
$exe=$provenance.executable
$active=@(Get-Process -Name LyraGame,UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool -ErrorAction SilentlyContinue)
if($active.Count){throw 'An Unreal game/editor/build is running. Coordinate ownership before execution.'}
if(@(Get-NetUDPEndpoint -LocalPort 7777 -ErrorAction SilentlyContinue).Count){throw 'UDP 7777 is already owned.'}
$contractPath=(Resolve-Path -LiteralPath $GateContractPath).Path
$contract=Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
if(!$contract.counts.PSObject.Properties['packagedRuns']){throw 'Selected contract must preserve packagedRuns count.'}
if($ValidateOnly){[ordered]@{status='preflight-only';candidate=$Candidate;renderedOwner=$RenderedOwnerHero;expectedActivations=32;peerRows=512;result=$resultPath}|ConvertTo-Json -Compress;return}
$contract.counts.packagedRuns=[int]$contract.counts.packagedRuns+1
$contract | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $contractPath -Encoding UTF8
$null=New-Item -ItemType Directory -Path $evidence -Force
$provenance | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $artifactPath -Encoding UTF8
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
$settings=Join-Path $root "Saved/StagedBuilds/$Candidate/Windows/ProjectB/Saved/Config/Windows/GameUserSettings.ini"
$settingsExisted=Test-Path -LiteralPath $settings -PathType Leaf
$settingsBefore=if($settingsExisted){[IO.File]::ReadAllBytes($settings)}else{$null}
$settingsBeforeHash=if($settingsExisted){Hash $settings}else{$null}
if($settingsExisted){[IO.File]::WriteAllBytes("$prefix-settings.before.ini",$settingsBefore)}
$startedUtc=[datetime]::UtcNow.ToString('o'); $watch=[Diagnostics.Stopwatch]::StartNew()
$owned=@{}; $records=[Collections.Generic.List[object]]::new(); $cache=@{}; $readiness=[Collections.Generic.List[object]]::new()
$runError=$null; $hostReady=$false; $allConnectedAlive=$false; $clientExitZero=$false; $settingsRestored=$false; $checkpoint=$null; $udpReady=$null
function Read-Log([string]$Role){
    $path=$logs[$Role]; if(!(Test-Path -LiteralPath $path)){return ''}
    if(!$cache.ContainsKey($Role)){$cache[$Role]=@{offset=0L;text=''}}
    $state=$cache[$Role]; $stream=[IO.File]::Open($path,'Open','Read','ReadWrite')
    try{if($stream.Length -lt $state.offset){throw 'Owned log rotated.'};$null=$stream.Seek($state.offset,'Begin');$reader=[IO.StreamReader]::new($stream);$state.text+=$reader.ReadToEnd();$state.offset=$stream.Position}finally{$stream.Dispose()}
    return $state.text
}
function Wait-Marker([string]$Role,[string]$Pattern,[int]$Seconds,[switch]$RequireUdp){
    $deadline=[math]::Min(1660,$watch.Elapsed.TotalSeconds+$Seconds)
    while($watch.Elapsed.TotalSeconds -lt $deadline){
        foreach($name in @($owned.Keys)){
            if((Read-Log $name) -match 'LogTemp: Error: BwayAllKitCooldown: FAIL|Fatal error:|Assertion failed:'){throw "Fixture/runtime failure in $name."}
            if($owned[$name].HasExited){throw "Owned $name exited before checkpoint ($($owned[$name].ExitCode))."}
        }
        if((Read-Log $Role) -match $Pattern){
            if(!$RequireUdp){return $true}
            $endpoints=@(Get-NetUDPEndpoint -LocalPort 7777 -ErrorAction SilentlyContinue | Where-Object OwningProcess -eq $owned['host'].Id)
            if($endpoints.Count){$script:udpReady=[ordered]@{port=7777;processId=$owned['host'].Id;observedUtc=[datetime]::UtcNow.ToString('o');addresses=@($endpoints.LocalAddress)};return $true}
        }
        Start-Sleep -Milliseconds 250
    }
    return $false
}
function Start-Owned([string]$Role,[string]$URL){
    $rendered=$Role -in @('host',$renderedRole)
    $command=if($Role -eq 'host'){'bway.Test.AllKitCooldownHost'}else{'bway.Test.AllKitCooldownClient'}
    $userDirectory=Join-Path "$prefix-users" $Role; $null=New-Item -ItemType Directory -Path $userDirectory
    $args=@($URL,'-nosound','-unattended','-nosplash','-MaxFPS=30',"-PktLag=$ExpectedLagMs",
        "-abslog=`"$($logs[$Role])`"",'-ExecCmds="t.MaxFPS 30"','-ExecCmds=t.MaxFPS',"-ExecCmds=Net PktLag=$ExpectedLagMs","-ExecCmds=$command",
        '-LogCmds="LogViewport Verbose,LogPlayerController Verbose"',
        '-ini:GameUserSettings:[/Script/LyraGame.LyraSettingsLocal]:FrameRateLimit=30.0',
        "-UserDir=`"$userDirectory`"","-enginesaveddirsuffix=$token-$Role")
    if($rendered){$args+=@('-d3d12','-NoRayTracing','-NoVSync','-windowed','-ResX=640','-ResY=360','-ForceRes','-WinY=20',$(if($Role -eq 'host'){'-WinX=20'}else{'-WinX=680'}))}else{$args+='-NullRHI'}
    $p=Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory (Split-Path -Parent $exe) -WindowStyle Hidden -PassThru
    $null=$p.Handle; $owned[$Role]=$p
    $records.Add([ordered]@{role=$Role;hero=$heroByRole[$Role];processId=$p.Id;args=$args;rendered=$rendered;userDir=$userDirectory;log=$logs[$Role];startedUtc=$p.StartTime.ToUniversalTime().ToString('o');normalExit=$false;closeRequested=$false;forcedKill=$false;exited=$false;exitCode=$null})
}
try{
    Start-Owned 'host' '/BreakawayCore/Maps/L_BW_HeroSelect_Staging?listen?HeroSelectStaging=1?HeroSelectTargetMap=/BreakawayCore/Maps/L_BW_Dorado?HeroSelectTargetExperience=B_BW_Experience_CaptureTheRelic?Experience=B_BW_Experience_CaptureTheRelic?NumBots=0?PointsToWin=99?RoundDuration=2000?WarmupDuration=3?PostRoundDuration=8'
    $hostReady=Wait-Marker 'host' 'LogNet: .*GameNetDriver.*listening on port 7777\b' 45 -RequireUdp
    if(!$hostReady){throw 'Host never owned UDP 7777 with an emitted listen-ready marker.'}
    foreach($role in @('client1','client2','client3')){
        Start-Owned $role '127.0.0.1:7777'
        if(!(Wait-Marker $role 'LogTemp: Display: BwayAllKitCooldown: StartupConnected\b' 30)){throw "$role connection timeout."}
        $readiness.Add([ordered]@{role=$role;ready=$true;observedUtc=[datetime]::UtcNow.ToString('o')})
    }
    if(!(Wait-Marker 'host' 'LogTemp: Display: BwayAllKitCooldown: Host PASS observer=\d+ cases=16 cycles=32 restoredPlayers=4' 1510)){throw 'Full 32-cycle host result timed out.'}
    foreach($role in @('client1','client2','client3')){if(!(Wait-Marker $role 'LogTemp: Display: BwayAllKitCooldown: Client PASS observer=\d+ cases=16 cycles=32 restoredPlayers=4' 5)){throw "$role final restoration missing."}}
    $allConnectedAlive=$owned.Count -eq 4 -and @($owned.Values | Where-Object HasExited).Count -eq 0
    if(!$allConnectedAlive){throw 'Four processes not alive at complete fixture checkpoint.'}
    $checkpoint=$watch.Elapsed.TotalSeconds
    do{
        if($owned['host'].HasExited){throw 'Host exited before clients.'}
        $clientExitZero=$true
        foreach($r in @($records | Where-Object role -ne 'host')){
            $p=$owned[$r.role]
            if(!$p.HasExited){$clientExitZero=$false}elseif($p.ExitCode -ne 0){throw "$($r.role) exited with $($p.ExitCode)."}else{$r.normalExit=$true}
        }
        if(!$clientExitZero){if($watch.Elapsed.TotalSeconds -gt [math]::Min(1660,$checkpoint+60)){throw 'Clients did not exit naturally after fixture teardown.'};Start-Sleep -Milliseconds 250}
    }while(!$clientExitZero)
}catch{$runError=$_.Exception.Message}
finally{
    foreach($r in $records){
        $p=$owned[$r.role]
        if(!$p.HasExited){$r.closeRequested=$true;try{[void]$p.CloseMainWindow();[void]$p.WaitForExit(5000)}catch{};if(!$p.HasExited){$r.forcedKill=$true;try{$p.Kill();[void]$p.WaitForExit(5000)}catch{}}}
        $r.exited=$p.HasExited; $r.exitCode=if($p.HasExited){[int]$p.ExitCode}else{$null}
        try{$r.logSHA256=if(Test-Path -LiteralPath $r.log){Hash $r.log}else{$null}}catch{$r.logSHA256=$null;if(!$runError){$runError=$_.Exception.Message}}
    }
    try{
        if(@(Get-Process -Name LyraGame,UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count){throw 'An Unreal process remains; preserve settings for its owner.'}
        if(Test-Path -LiteralPath $settings){[IO.File]::WriteAllBytes("$prefix-settings.after.ini",[IO.File]::ReadAllBytes($settings))}
        if($settingsExisted){[IO.File]::WriteAllBytes($settings,$settingsBefore);$settingsRestored=(Hash $settings) -eq $settingsBeforeHash}else{if(Test-Path -LiteralPath $settings){Remove-Item -LiteralPath $settings};$settingsRestored=!(Test-Path -LiteralPath $settings)}
    }catch{if(!$runError){$runError=$_.Exception.Message}}
}
$allOwnedExited=$records.Count -eq 4 -and @($records | Where-Object {-not $_.exited}).Count -eq 0
$provenanceAfter=$null
try{
    $provenanceAfter=& "$PSScriptRoot/Assert-AllKitCooldownCandidate.ps1" @guardArguments
    foreach($key in @('packageResultSHA256','sourceManifestSHA256','artifactManifestSHA256','configurationSHA256','stagedHash','guardSHA256')){if($provenanceAfter.$key -ne $provenance.$key){throw "Immutable $key changed during run."}}
}catch{if(!$runError){$runError=$_.Exception.Message}}
$result=[ordered]@{schema='allkit-cooldown-transport-1';candidate=$Candidate;attempt=$Attempt;token=$token;renderedOwnerHero=$RenderedOwnerHero;renderedOwnerRole=$renderedRole;expectedLagMs=$ExpectedLagMs;latencyCommand="Net PktLag=$ExpectedLagMs";
    artifactVerification=$artifactPath;artifactVerificationSHA256=(Hash $artifactPath);configuration=$provenance.configuration;configurationSHA256=$provenance.configurationSHA256;
    packageResult=$provenance.packageResult;packageResultSHA256=$provenance.packageResultSHA256;effectiveSourceIdentity=$provenance.effectiveSourceIdentity;
    gateContract=$contractPath;packagedRunNumber=$contract.counts.packagedRuns;runnerSHA256=(Hash $PSCommandPath);processes=@($records);hostReady=$hostReady;udpReadiness=$udpReady;startupReadiness=@($readiness);
    allConnectedAlive=$allConnectedAlive;liveCheckpointAt=$checkpoint;clientExitZero=$clientExitZero;allOwnedExited=$allOwnedExited;settingsRestored=$settingsRestored;provenanceRechecked=($null -ne $provenanceAfter -and !$runError);
    hostTeardown='clients-probe-destroyed-normal-exit; host-owned-teardown-after-all-client-exits';
    scope="All 16 families/32 cycles on four processes; host and $RenderedOwnerHero rendered; other owners NullRHI. Widget-state, synthetic activation and same-PC transport only; no visual, physical-input, full combat-balance or external-LAN acceptance.";
    startedUtc=$startedUtc;finishedUtc=[datetime]::UtcNow.ToString('o');elapsedSeconds=$watch.Elapsed.TotalSeconds;runError=$runError;
    status=$(if(!$runError -and $hostReady -and $allConnectedAlive -and $clientExitZero -and $allOwnedExited -and $settingsRestored){'passed-transport-scope'}else{'failed-transport-scope'})}
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $resultPath -Encoding UTF8
& "$PSScriptRoot/Verify-AllKitCooldownTransport.ps1" -HostLog $logs.host -ClientLogs @($logs.client1,$logs.client2,$logs.client3) -TransportResult $resultPath -ArtifactVerification $artifactPath -SourceAudit $provenance.configuration -OutputJson "$prefix-brief.json"
$brief=Get-Content -LiteralPath "$prefix-brief.json" -Raw | ConvertFrom-Json
if($result.status -ne 'passed-transport-scope' -or $brief.status -ne 'passed'){throw "Cooldown run unverified: $runError $($brief.firstCausalError)"}
