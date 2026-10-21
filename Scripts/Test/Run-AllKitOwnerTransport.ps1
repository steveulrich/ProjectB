<# Four-process AllKit transport. Synthetic owner activation; no physical-input or second-PC claim. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^LAN-[0-9]{8}-[0-9]+$')][string]$Candidate,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,60}$')][string]$Attempt,
    [Parameter(Mandatory)][ValidateSet(0,100)][int]$ExpectedLagMs,
    [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ExpectedExeSHA256,
    [Parameter(Mandatory)][string]$PackageResultPath,
    [Parameter(Mandatory)][string]$GateContractPath,
    [string]$EvidenceDirectory='Saved/Logs/goal1-review-20261016',
    [switch]$RemoteArgus,
    [ValidateSet('client1','client2','client3')][string]$RenderedRemoteRole='client1',
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$exe=Join-Path $root "Saved/StagedBuilds/$Candidate/Windows/ProjectB/Binaries/Win64/LyraGame.exe"
$evidence=[IO.Path]::GetFullPath((Join-Path $root $EvidenceDirectory))
if(!$evidence.StartsWith((Join-Path $root 'Saved')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Evidence must remain under project Saved.'}
$token="$Candidate-$Attempt"
$prefix=Join-Path $evidence "allkit-$token"
$resultPath="$prefix-result.json"
$settings=Join-Path $root "Saved/StagedBuilds/$Candidate/Windows/ProjectB/Saved/Config/Windows/GameUserSettings.ini"
$roles=@('host','client1','client2','client3')
$logs=@{}; foreach($role in $roles){$logs[$role]="$prefix-$role.log"}
foreach($path in @($resultPath,"$prefix-settings.before.ini","$prefix-settings.after.ini","$prefix-users")+@($logs.Values)){
    if(Test-Path -LiteralPath $path){throw "Preserve existing attempt evidence: $path"}
}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function TextHash([string]$Text){
    $hasher=[Security.Cryptography.SHA256]::Create()
    try{([BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-','')}finally{$hasher.Dispose()}
}
function ChildPath([string]$Parent,[string]$Relative){
    if(!$Relative -or [IO.Path]::IsPathRooted($Relative)){throw 'Relative manifest path required.'}
    $parentPath=[IO.Path]::GetFullPath($Parent).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $resultPath=[IO.Path]::GetFullPath((Join-Path $parentPath $Relative))
    if(!$resultPath.StartsWith($parentPath,[StringComparison]::OrdinalIgnoreCase)){throw 'Manifest path escapes its root.'}
    $resultPath
}
function AssertBytes([string]$Path,$Record){
    if($Record.sha256 -notmatch '^[A-Fa-f0-9]{64}$' -or !(Test-Path -LiteralPath $Path -PathType Leaf) -or
        (Get-Item -LiteralPath $Path).Length -ne [long]$Record.bytes -or (Hash $Path) -ne $Record.sha256){throw "Manifest bytes changed: $Path"}
}
function Get-LiveUnrealProcesses {
    @(Get-CimInstance Win32_Process -ErrorAction Stop|Where-Object {
        $_.Name -match '^(LyraGame|UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool)\.exe$' -or
        ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
    })
}
$exeHash=Hash $exe
if($exeHash -ne $ExpectedExeSHA256){throw 'Candidate executable hash mismatch.'}
$packagePath=(Resolve-Path -LiteralPath $PackageResultPath).Path
$packageHashBefore=Hash $packagePath
$package=Get-Content -LiteralPath $packagePath -Raw | ConvertFrom-Json
if($package.status -ne 'passed' -or $package.exitCode -ne 0 -or $package.candidate -ne $Candidate -or !$package.sourceManifestSha256 -or !$package.moduleRestored){throw 'A successful matching package record with immutable source binding is required.'}
$manifestPath=(Resolve-Path -LiteralPath $package.sourceManifest).Path
$manifestHashBefore=Hash $manifestPath
if((Hash $manifestPath) -ne $package.sourceManifestSha256){throw 'Package source manifest changed.'}
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if(!$manifest.effectiveSourceIdentity -or $manifest.effectiveSourceIdentity -ne $package.effectiveSourceIdentity -or $manifest.sourceHead -ne $package.sourceHead -or !$manifest.archive){throw 'Package/source identity mismatch.'}
if(@($manifest.files | Where-Object {$_.path -match '/(Private|Public)/Development/BwayAllKitCombatProbe\.(cpp|h)$'}).Count -ne 2){throw 'The frozen package does not contain the new AllKit owner fixture.'}
$snapshotPath=(Resolve-Path -LiteralPath $package.sourceSnapshot).Path
$snapshotHashBefore=Hash $snapshotPath
if(!$package.sourceSnapshotSha256 -or $snapshotHashBefore -ne $package.sourceSnapshotSha256){throw 'Frozen source snapshot JSON bytes changed.'}
$snapshot=Get-Content -LiteralPath $snapshotPath -Raw | ConvertFrom-Json
if($snapshot.sourceHead -ne $package.sourceHead -or $snapshot.sourceIdentity -ne $package.sourceIdentity){throw 'Frozen snapshot identity mismatch.'}
foreach($kind in @('patch','status','untracked')){AssertBytes $snapshot.files.$kind.path $snapshot.files.$kind}
if((TextHash ($snapshot.sourceHead+'|'+$snapshot.files.patch.sha256+'|'+$snapshot.files.status.sha256+'|'+$snapshot.files.untracked.sha256)) -ne $package.sourceIdentity){throw 'Snapshot content identity changed.'}
$head=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or $head -ne $manifest.sourceHead){throw 'Runtime checkout HEAD differs from package.'}
# Documentation and tooling may advance after packaging; executable inputs may not.
$inputPattern='(?i)(\.(cpp|h|hpp|inl|cs|uasset|umap|ini|uproject|uplugin)$|(^|/)(Config|Content)/)'
$frozenInputs=@{}
foreach($file in @($manifest.files)){
    if(!$file.path -or $frozenInputs.ContainsKey($file.path)){throw 'Invalid or duplicate source manifest path.'}
    AssertBytes (ChildPath $root $file.path) $file
    AssertBytes (ChildPath $manifest.archive $file.path) $file
    $frozenInputs[$file.path]=$true
}
if(!$frozenInputs.Count -or (TextHash ($head+"`n"+(($manifest.files|ForEach-Object {$_.path+':'+$_.sha256}) -join "`n"))) -ne $manifest.effectiveSourceIdentity){throw 'Effective archived source identity changed.'}
$changed=@(& git -C $root diff HEAD --name-only)
if($LASTEXITCODE -ne 0){throw 'Cannot enumerate modified inputs.'}
$untracked=@(& git -C $root ls-files --others --exclude-standard)
if($LASTEXITCODE -ne 0){throw 'Cannot enumerate new inputs.'}
$currentInputs=@($changed+$untracked|Where-Object {$_ -match $inputPattern}|Sort-Object -Unique)
$recordedInputs=@($manifest.files.path|Where-Object {$_ -match $inputPattern}|Sort-Object -Unique)
if(Compare-Object $recordedInputs $currentInputs){throw 'Build input set differs from the frozen package.'}
$nativeInputs=@($manifest.files|Where-Object path -Match '\.(cpp|h|cs|uproject|uplugin)$'|Sort-Object path)
$nativeIdentity=TextHash ($head+"`n"+(($nativeInputs|ForEach-Object {$_.path+':'+$_.sha256}) -join "`n"))
$compileTargets=@{}
foreach($entry in @($package.compileRecords)){
    if(!$entry.sha256 -or (Hash $entry.path) -ne $entry.sha256){throw 'Bound compile record bytes changed.'}
    $record=Get-Content -LiteralPath $entry.path -Raw|ConvertFrom-Json
    if($compileTargets.ContainsKey($entry.target) -or $entry.target -notin @('LyraEditor','LyraGame') -or $record.target -ne $entry.target -or
        $record.status -ne 'passed' -or $record.exitCode -ne 0 -or $record.sourceHead -ne $head -or
        !$record.PSObject.Properties['failureSignals'] -or @($record.failureSignals).Count -or !$record.startedUtc){throw 'Successful bound Editor/Game compile records required.'}
    if(!$record.originalCompileRecordSha256 -or (Hash $record.originalCompileRecord) -ne $record.originalCompileRecordSha256 -or
        !$record.nativeSourceManifestSha256 -or (Hash $record.nativeSourceManifest) -ne $record.nativeSourceManifestSha256){throw 'Original compile/native manifest binding changed.'}
    $original=Get-Content -LiteralPath $record.originalCompileRecord -Raw|ConvertFrom-Json
    foreach($field in @('status','exitCode','sourceHead','target','startedUtc','generatedUtc','log')){if($original.$field -ne $record.$field){throw "Bound compile changed original field: $field"}}
    $compileManifest=Get-Content -LiteralPath $record.nativeSourceManifest -Raw|ConvertFrom-Json
    if($compileManifest.sourceHead -ne $head -or [datetime]$compileManifest.createdUtc -gt [datetime]$record.startedUtc){throw 'Native archive must predate its compile.'}
    foreach($file in @($compileManifest.files)){AssertBytes (ChildPath $compileManifest.archive $file.path) $file}
    $compiledInputs=@($compileManifest.files|Where-Object path -Match '\.(cpp|h|cs|uproject|uplugin)$'|Sort-Object path)
    $compiledIdentity=TextHash ($head+"`n"+(($compiledInputs|ForEach-Object {$_.path+':'+$_.sha256}) -join "`n"))
    if($compiledIdentity -ne $nativeIdentity -or $record.nativeSourceIdentity -ne $nativeIdentity -or
        $record.nativeInputCount -ne $nativeInputs.Count -or $compiledInputs.Count -ne $nativeInputs.Count -or
        $record.binarySha256 -notmatch '^[A-Fa-f0-9]{64}$' -or !$record.binaryLastWriteUtc -or [datetime]$record.binaryLastWriteUtc -lt [datetime]$record.startedUtc -or
        ($entry.target -eq 'LyraGame' -and $record.binarySha256 -ne $exeHash)){throw 'Final native inputs or produced game binary differ from bound compile.'}
    $compileTargets[$entry.target]=$true
}
if($compileTargets.Count -ne 2){throw 'Both bound Editor and Game compile records are required.'}
$artifactManifest=(Resolve-Path -LiteralPath $package.hashManifest).Path
$artifactManifestHashBefore=Hash $artifactManifest
$artifactRows=@(Import-Csv -LiteralPath $artifactManifest)
if(!$artifactRows.Count){throw 'Package artifact manifest is empty.'}
$candidateRoot=[IO.Path]::GetFullPath((Join-Path $root "Saved/StagedBuilds/$Candidate"))
if([IO.Path]::GetFullPath($package.stage) -ne $candidateRoot -or $artifactManifest -ne (Join-Path $candidateRoot 'sha256.csv')){throw 'Package stage/artifact manifest differs from candidate root.'}
function Assert-Artifacts {
    if((Get-Item -LiteralPath $candidateRoot).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Candidate root is a reparse point.'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($entry in $artifactRows){
        $path=ChildPath $candidateRoot $entry.path
        if(!$path.StartsWith($candidateRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or !$seen.Add($path)){throw 'Artifact manifest path escaped or is duplicated.'}
        $item=Get-Item -LiteralPath $path
        if($item.Length -ne [long]$entry.bytes -or (Hash $path) -ne $entry.sha256){throw "Packaged artifact changed: $($entry.path)"}
        for($node=$item;$node -and $node.FullName -ne $candidateRoot;$node=$node.Parent){
            if($node.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse point in packaged artifact path.'}
            if($node -is [IO.FileInfo]){$node=$node.Directory;if($node.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse point in packaged artifact directory.'}}
        }
    }
    $runtimeSaved=(Join-Path $candidateRoot 'Windows/ProjectB/Saved')+[IO.Path]::DirectorySeparatorChar
    $extras=@(Get-ChildItem -LiteralPath $candidateRoot -File -Recurse|Where-Object {
        $_.FullName -ne $artifactManifest -and !$seen.Contains($_.FullName) -and
        (!$_.FullName.StartsWith($runtimeSaved,[StringComparison]::OrdinalIgnoreCase) -or $_.Extension -in '.exe','.dll','.pak','.ucas','.utoc','.uasset','.umap')
    })
    if($artifactRows.Count -ne $package.artifactCount -or $extras.Count){throw 'Immutable package payload membership/count changed.'}
}
Assert-Artifacts
# Confirm the package's executable independently of the caller's expected hash.
$exeRows=@($artifactRows | Where-Object {($_.path -replace '\\','/') -match '/Binaries/Win64/LyraGame\.exe$'})
if($exeRows.Count -ne 1 -or $exeRows[0].sha256 -ne $exeHash -or $package.executableSha256 -ne $exeHash){throw 'Packaged executable is absent or mismatched in artifact manifest.'}
$active=@(Get-LiveUnrealProcesses)
if($active.Count){throw 'An Unreal editor, game or build is already running; coordinate ownership first.'}
$udpBefore=@(Get-NetUDPEndpoint -ErrorAction Stop|Where-Object LocalPort -eq 7777)
if($udpBefore.Count){throw 'UDP port 7777 is already owned.'}
$contractPath=(Resolve-Path -LiteralPath $GateContractPath).Path
$contract=Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
if($contract.candidate -ne $Candidate -or !$contract.counts.PSObject.Properties['packagedRuns']){throw 'Selected gate candidate and packagedRuns count must match.'}
if($ValidateOnly){[ordered]@{status='preflight-only';candidate=$Candidate;token=$token;renderedRemoteRole=$RenderedRemoteRole;remoteArgus=[bool]$RemoteArgus;executableSHA256=$exeHash;effectiveSourceIdentity=$manifest.effectiveSourceIdentity;result=$resultPath}|ConvertTo-Json -Compress;return}
$contract.counts.packagedRuns=[int]$contract.counts.packagedRuns+1
$contract | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $contractPath -Encoding UTF8
$null=New-Item -ItemType Directory -Path $evidence -Force
$settingsExisted=Test-Path -LiteralPath $settings -PathType Leaf
$settingsBefore=if($settingsExisted){[IO.File]::ReadAllBytes($settings)}else{$null}
$settingsBeforeHash=if($settingsExisted){Hash $settings}else{$null}
if($settingsExisted){[IO.File]::WriteAllBytes("$prefix-settings.before.ini",$settingsBefore)}
$startedUtc=[DateTime]::UtcNow.ToString('o')
$watch=[Diagnostics.Stopwatch]::StartNew()
$owned=@{}; $records=[Collections.Generic.List[object]]::new(); $cache=@{}
$runError=$null; $hostReady=$false; $allConnectedAlive=$false; $clientExitZero=$false; $settingsRestored=$false; $checkpoint=$null; $listenEndpoints=@()
$nativeFailurePattern='(?m)^(?:\[[^\]\r\n]*\]\s*)*\s*(?:[A-Za-z_]\w+:\s*(?:Error|Fatal):|Log\w*(?:Error|Fatal):|(?:Fatal error|Assertion failed|Ensure condition failed):)[^\r\n]*'
function Assert-NoNativeErrors([string]$Role,[string]$Text){
    $failure=[regex]::Match($Text,$nativeFailurePattern)
    if($failure.Success){throw "$Role emitted an unclassified native failure: $($failure.Value.Substring(0,[Math]::Min(600,$failure.Value.Length)))"}
}
function Read-Log([string]$Role){
    $path=$logs[$Role]; if(!(Test-Path -LiteralPath $path)){return ''}
    if(!$cache.ContainsKey($Role)){$cache[$Role]=@{offset=0L;text=''}}
    $state=$cache[$Role]; $stream=[IO.File]::Open($path,'Open','Read','ReadWrite')
    try{if($stream.Length -lt $state.offset){throw 'Log rotated during owned run.'};$null=$stream.Seek($state.offset,'Begin');$reader=[IO.StreamReader]::new($stream);$state.text+=$reader.ReadToEnd();$state.offset=$stream.Position}finally{$stream.Dispose()}
    return $state.text
}
function Wait-Marker([string]$Role,[string]$Pattern,[int]$Seconds){
    $deadline=[Math]::Min(560,$watch.Elapsed.TotalSeconds+$Seconds)
    while($watch.Elapsed.TotalSeconds -lt $deadline){
        foreach($name in @($owned.Keys)){
            $text=Read-Log $name
            Assert-NoNativeErrors $name $text
            if($owned[$name].HasExited){throw "Owned $name exited before live checkpoint (exit $($owned[$name].ExitCode))."}
        }
        if((Read-Log $Role) -match $Pattern){return $true}
        Start-Sleep -Milliseconds 250
    }
    return $false
}
function Start-Owned([string]$Role,[string]$URL){
    $rendered=$Role -in @('host',$RenderedRemoteRole)
    $command=if($Role -eq 'host'){'bway.Test.AllKitOwnerHost'}else{'bway.Test.AllKitOwnerClient'}
    $userDirectory=Join-Path "$prefix-users" $Role
    if(Test-Path -LiteralPath $userDirectory){throw 'Per-process user directory must be fresh.'}
    $null=New-Item -ItemType Directory -Path $userDirectory
    $args=@($URL,'-nosound','-unattended','-nosplash','-MaxFPS=30',"-PktLag=$ExpectedLagMs","-AllKitOwnerToken=$token", "-abslog=`"$($logs[$Role])`"", "-ExecCmds=$command",'-ExecCmds="t.MaxFPS 30"','-ini:GameUserSettings:[/Script/LyraGame.LyraSettingsLocal]:FrameRateLimit=30.0',"-UserDir=`"$userDirectory`"","-enginesaveddirsuffix=$token-$Role")
    if($RemoteArgus){$args+='-AllKitRemoteArgus'}
    if($Role -ne 'host'){$args+='-AllKitOwnerAutoExit'}
    if($rendered){$args+=@('-d3d12','-windowed','-ResX=640','-ResY=360','-ForceRes','-WinY=20',$(if($Role -eq 'host'){'-WinX=20'}else{'-WinX=680'}))}else{$args+='-NullRHI'}
    $p=Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory (Split-Path -Parent $exe) -WindowStyle Hidden -PassThru
    $null=$p.Handle; $owned[$Role]=$p
    $records.Add([ordered]@{role=$Role;processId=$p.Id;args=$args;rendered=$rendered;log=$logs[$Role];userDirectory=$userDirectory;userDirectoryWasAbsent=$true;engineSavedSuffix="$token-$Role";normalExit=$false;closeRequested=$false;forcedKill=$false;exited=$false;exitCode=$null})
}
try{
    Start-Owned 'host' '/BreakawayCore/Maps/L_BW_HeroSelect_Staging?listen?HeroSelectStaging=1?HeroSelectTargetMap=/BreakawayCore/Maps/L_BW_Dorado?HeroSelectTargetExperience=B_BW_Experience_CaptureTheRelic?Experience=B_BW_Experience_CaptureTheRelic?NumBots=0?PointsToWin=99?RoundDuration=900?WarmupDuration=3?PostRoundDuration=8'
    $hostReady=Wait-Marker 'host' '(?m)^(?:\[[^\]\r\n]*\]\s*)*LogNet: (?:Display: )?[^\r\n]*GameNetDriver[^\r\n]*listening on port 7777\b' 45
    if(!$hostReady){throw 'Host listen startup timeout.'}
    $listenEndpoints=@(Get-NetUDPEndpoint -ErrorAction Stop|Where-Object LocalPort -eq 7777|Select-Object LocalAddress,LocalPort,OwningProcess)
    if(!$listenEndpoints.Count -or @($listenEndpoints|Where-Object OwningProcess -ne $owned['host'].Id).Count){throw 'UDP listen endpoint is not owned by the host process.'}
    foreach($role in @('client1','client2','client3')){Start-Owned $role '127.0.0.1:7777';if(!(Wait-Marker $role 'LogTemp: Display: BwayAllKitOwner: StartupConnected role=client' 30)){throw "$role startup timeout."}}
    if(!(Wait-Marker 'host' 'LogTemp: Display: BwayAllKitOwner: PASS actions=20 controls=20 creditCases=2 observers=4 cleanup=1' 450)){throw 'Host final proof timeout.'}
    foreach($role in $roles){if(!(Wait-Marker $role 'LogTemp: Display: BwayAllKitOwner: PeerFinalPASS observer=\d+ cleanup=1' 5)){throw "$role final cleanup missing."}}
    $allConnectedAlive=$owned.Count -eq 4 -and @($owned.Values | Where-Object HasExited).Count -eq 0
    if(!$allConnectedAlive){throw 'Four peers were not alive at completed fixture checkpoint.'}
    $checkpoint=$watch.Elapsed.TotalSeconds
    do{
        if($owned['host'].HasExited){throw 'Host exited before all clients.'}
        foreach($name in @($owned.Keys)){Assert-NoNativeErrors $name (Read-Log $name)}
        $clientExitZero=$true
        foreach($r in @($records | Where-Object role -ne 'host')){$p=$owned[$r.role];if(!$p.HasExited){$clientExitZero=$false}elseif($p.ExitCode -ne 0){throw "$($r.role) exited with $($p.ExitCode)."}else{$r.normalExit=$true}}
        if(!$clientExitZero){if($watch.Elapsed.TotalSeconds-$checkpoint -gt 35){throw 'Clients did not exit after final cleanup.'};Start-Sleep -Milliseconds 250}
    }while(!$clientExitZero)
}catch{$runError=$_.Exception.Message}
finally{
    foreach($r in $records){
        $p=$owned[$r.role]
        if(!$p.HasExited){$r.closeRequested=$true;try{[void]$p.CloseMainWindow();[void]$p.WaitForExit(5000)}catch{};if(!$p.HasExited){$r.forcedKill=$true;try{$p.Kill();[void]$p.WaitForExit(5000)}catch{}}}
        $r.exited=$p.HasExited;$r.exitCode=if($p.HasExited){[int]$p.ExitCode}else{$null}
        $r.logBytes=0;$r.logSHA256=$null
        try{if(Test-Path -LiteralPath $r.log){$r.logSHA256=Hash $r.log;$r.logBytes=(Get-Item -LiteralPath $r.log).Length;Assert-NoNativeErrors $r.role (Read-Log $r.role)}}catch{if(!$runError){$runError=$_.Exception.Message}}
    }
    try{$remaining=@(Get-LiveUnrealProcesses);if(!$remaining.Count){
        if(Test-Path -LiteralPath $settings){[IO.File]::WriteAllBytes("$prefix-settings.after.ini",[IO.File]::ReadAllBytes($settings))}
        if($settingsExisted){[IO.File]::WriteAllBytes($settings,$settingsBefore);$settingsRestored=(Hash $settings) -eq $settingsBeforeHash}else{if(Test-Path -LiteralPath $settings){Remove-Item -LiteralPath $settings};$settingsRestored=!(Test-Path -LiteralPath $settings)}
    }else{if(!$runError){$runError='Settings restoration withheld: an Unreal process remains.'}}}catch{if(!$runError){$runError=$_.Exception.Message}}
}
$allOwnedExited=$records.Count -eq 4 -and @($records | Where-Object {-not $_.exited}).Count -eq 0
$hashAfter=$null; $packageHash=$null; $manifestHash=$null
try{
    $hashAfter=Hash $exe;$packageHash=Hash $packagePath;$manifestHash=Hash $manifestPath
    if($packageHash -ne $packageHashBefore -or $manifestHash -ne $manifestHashBefore -or (Hash $artifactManifest) -ne $artifactManifestHashBefore -or (Hash $snapshotPath) -ne $snapshotHashBefore){throw 'Immutable package provenance changed during runtime.'}
    Assert-Artifacts
}catch{if(!$runError){$runError=$_.Exception.Message}}
$result=[ordered]@{schema='allkit-owner-1';candidate=$Candidate;attempt=$Attempt;token=$token;expectedLagMs=$ExpectedLagMs;remoteArgus=[bool]$RemoteArgus;renderedRemoteRole=$RenderedRemoteRole;
    gateContract=$contractPath;packagedRunNumber=$contract.counts.packagedRuns;
    packageResult=$packagePath;packageResultSHA256=$packageHash;sourceManifest=$manifestPath;sourceManifestSHA256=$manifestHash;effectiveSourceIdentity=$manifest.effectiveSourceIdentity;
    sourceHead=$head;sourceSnapshot=$snapshotPath;sourceSnapshotSHA256=$snapshotHashBefore;sourceIdentity=$snapshot.sourceIdentity;nativeSourceIdentity=$nativeIdentity;artifactManifest=$artifactManifest;artifactManifestSHA256=$artifactManifestHashBefore;
    executable=$exe;executableSHA256=$exeHash;executableSHA256After=$hashAfter;runner=$PSCommandPath;runnerSHA256=(Hash $PSCommandPath);processes=@($records);
    udpPortAvailableBefore=$true;listenEndpoints=$listenEndpoints;settings=$settings;settingsExistedBefore=$settingsExisted;settingsBeforeSHA256=$settingsBeforeHash;settingsBackup="$prefix-settings.before.ini";
    hostReady=$hostReady;allConnectedAlive=$allConnectedAlive;liveCheckpointAt=$checkpoint;clientExitZero=$clientExitZero;allOwnedExited=$allOwnedExited;settingsRestored=$settingsRestored;
    hostTeardown='owned-close-after-natural-client-exit; forced host cleanup recorded separately';scope='All 42 synthetic cases with host and the selected remote role rendered; other owners use NullRHI. No all-rendered, physical input, visual acceptance or second-PC LAN claim.';
    runtimeSettingsPolicy='Fresh per-process UserDir under attempt evidence and unique engine Saved suffix; original staged settings bytes/existence also preserved.';
    runError=$runError;startedUtc=$startedUtc;finishedUtc=[DateTime]::UtcNow.ToString('o');elapsedSeconds=$watch.Elapsed.TotalSeconds;
    status=$(if(!$runError -and $hostReady -and $allConnectedAlive -and $clientExitZero -and $allOwnedExited -and $settingsRestored -and $hashAfter -eq $exeHash){'passed-transport-scope'}else{'failed-transport-scope'})}
$result | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath $resultPath -Encoding UTF8
[ordered]@{status=$result.status;result=$resultPath;runError=$runError}|ConvertTo-Json -Compress
if($result.status -ne 'passed-transport-scope'){exit 1}
