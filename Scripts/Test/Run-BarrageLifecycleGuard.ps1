[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^LAN-[0-9]{8}-[0-9]+$')][string]$Candidate,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Attempt,
    [Parameter(Mandatory)][ValidateSet(0,100)][int]$ExpectedLagMs,
    [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ExpectedExeSHA256,
    [Parameter(Mandatory)][string]$PackageResultPath,
    [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ReviewedNativeSHA256,
    [Parameter(Mandatory)][ValidateSet('normal','aim','damage-only','heal-damage','heal-only','death','round-reset')][string]$Scenario,
    [Parameter(Mandatory)][string]$GateContractPath,
    [string]$EvidenceDirectory = 'Saved/Logs/goal1-barrage-lifecycle-20261016',
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Set-Location -LiteralPath $root
$d=[IO.Path]::GetFullPath((Join-Path $root $EvidenceDirectory))
$savedRoot=[IO.Path]::GetFullPath((Join-Path $root 'Saved'))+[IO.Path]::DirectorySeparatorChar
if(!$d.StartsWith($savedRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Evidence must remain under project Saved.'}
$exe=Join-Path $root "Saved/StagedBuilds/$Candidate/Windows/ProjectB/Binaries/Win64/LyraGame.exe"
$settings=Join-Path $root "Saved/StagedBuilds/$Candidate/Windows/ProjectB/Saved/Config/Windows/GameUserSettings.ini"
$runner=Join-Path $root 'Scripts/Test/Run-BarrageLifecycleTransport.ps1'
$base=Join-Path $d "$Candidate-$Attempt"
$guardPath="$base-settings-guard.json"
$backup="$base-GameUserSettings.before.ini"
if((Test-Path $guardPath) -or (Test-Path $backup)){throw 'Preserve existing guarded run evidence.'}
if((Get-FileHash $runner).Hash -ne 'D9846F2381423B63719F75CB6CA3EB859406E882A85C2F149B1D3444FB2326F4'){throw 'Reviewed runner changed.'}
if((Get-FileHash $exe).Hash -ne $ExpectedExeSHA256){throw 'Candidate executable changed.'}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function TextHash([string]$Text){
    $hasher=[Security.Cryptography.SHA256]::Create()
    try {([BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-','')}
    finally {$hasher.Dispose()}
}
function ChildPath([string]$Parent,[string]$Relative){
    if(!$Relative -or [IO.Path]::IsPathRooted($Relative)){throw 'Relative manifest path required.'}
    $parentPath=[IO.Path]::GetFullPath($Parent).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $resultPath=[IO.Path]::GetFullPath((Join-Path $parentPath $Relative))
    if(!$resultPath.StartsWith($parentPath,[StringComparison]::OrdinalIgnoreCase)){throw 'Manifest path escapes its root.'}
    $resultPath
}
function AssertBytes([string]$Path,$Record){
    if(!$Record.sha256 -or $Record.sha256 -notmatch '^[A-Fa-f0-9]{64}$' -or !(Test-Path -LiteralPath $Path -PathType Leaf) -or
        (Get-Item -LiteralPath $Path).Length -ne [long]$Record.bytes -or (Hash $Path) -ne $Record.sha256){throw "Manifest bytes changed: $Path"}
}
$packagePath=(Resolve-Path -LiteralPath $PackageResultPath).Path
$package=Get-Content -LiteralPath $packagePath -Raw|ConvertFrom-Json
$frozenManifestPath=(Resolve-Path -LiteralPath $package.sourceManifest).Path
$frozenManifestHash=(Get-FileHash -LiteralPath $frozenManifestPath).Hash
$frozen=Get-Content -LiteralPath $frozenManifestPath -Raw|ConvertFrom-Json
if($package.status -ne 'passed' -or $package.exitCode -ne 0 -or $package.candidate -ne $Candidate -or !$package.moduleRestored -or
    !$package.sourceManifestSha256 -or $frozenManifestHash -ne $package.sourceManifestSha256 -or
    $frozenManifestPath -ne (Resolve-Path -LiteralPath $package.sourceManifest).Path -or
    !$frozen.effectiveSourceIdentity -or $package.effectiveSourceIdentity -ne $frozen.effectiveSourceIdentity -or
    $package.sourceHead -ne $frozen.sourceHead -or !$frozen.archive){throw 'Immutable successful package source manifest required.'}
$frozenSnapshotPath=(Resolve-Path -LiteralPath $package.sourceSnapshot).Path
$frozenSnapshotHash=Hash $frozenSnapshotPath
if(!$package.sourceSnapshotSha256 -or $frozenSnapshotHash -ne $package.sourceSnapshotSha256){throw 'Frozen package snapshot bytes changed.'}
$frozenSnapshot=Get-Content -LiteralPath $frozenSnapshotPath -Raw|ConvertFrom-Json
if($frozenSnapshot.sourceHead -ne $frozen.sourceHead -or $frozenSnapshot.sourceIdentity -ne $package.sourceIdentity){throw 'Frozen package snapshot identity changed.'}
foreach($kind in @('patch','status','untracked')){
    $file=$frozenSnapshot.files.$kind
    if(!$file){throw "Missing frozen package snapshot $kind."}
    AssertBytes $file.path $file
}
$snapshotIdentity=TextHash ($frozen.sourceHead+'|'+$frozenSnapshot.files.patch.sha256+'|'+$frozenSnapshot.files.status.sha256+'|'+$frozenSnapshot.files.untracked.sha256)
if($snapshotIdentity -ne $package.sourceIdentity){throw 'Snapshot content identity mismatch.'}
$head=(& git -C $root rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or $head -ne $frozen.sourceHead){throw 'Current HEAD differs from package source.'}
# Build inputs must match exactly. Documentation and runner scripts are separately
# recorded evidence; advancing those files cannot license a changed build input.
$inputPattern='(?i)(\.(cpp|h|hpp|inl|cs|uasset|umap|ini|uproject|uplugin)$|(^|/)(Config|Content)/)'
$frozenInputs=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($file in @($frozen.files)){
    if(!$file.path -or $frozenInputs.ContainsKey($file.path)){throw 'Invalid or duplicate frozen source path.'}
    $frozenInputs.Add($file.path,$file)
    AssertBytes (ChildPath $frozen.archive $file.path) $file
    AssertBytes (ChildPath $root $file.path) $file
}
if(!$frozenInputs.Count){throw 'Empty archived source manifest.'}
$identityText=$frozen.sourceHead+"`n"+(($frozen.files|ForEach-Object {$_.path+':'+$_.sha256}) -join "`n")
if((TextHash $identityText) -ne $frozen.effectiveSourceIdentity){throw 'Archived effective source identity mismatch.'}
$changed=@(& git -C $root diff HEAD --name-only)
if($LASTEXITCODE -ne 0){throw 'Cannot enumerate modified build inputs.'}
$untracked=@(& git -C $root ls-files --others --exclude-standard)
if($LASTEXITCODE -ne 0){throw 'Cannot enumerate new build inputs.'}
$currentInputs=@($changed+$untracked|Where-Object {$_ -match $inputPattern}|Sort-Object -Unique)
$recordedInputs=@($frozen.files.path|Where-Object {$_ -match $inputPattern}|Sort-Object -Unique)
if(Compare-Object $recordedInputs $currentInputs){throw 'Build input set differs from frozen package.'}
$nativePath='Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/Development/BwayBarrageTransportProbe.cpp'
if(!$frozenInputs.ContainsKey($nativePath) -or $frozenInputs[$nativePath].sha256 -ne $ReviewedNativeSHA256){throw 'Explicit independently reviewed native hash does not match compiled archive.'}
$compileTargets=@{}
$nativePattern='\.(cpp|h|cs|uproject|uplugin)$'
$nativeInputs=@($frozen.files|Where-Object path -Match $nativePattern|Sort-Object path)
$nativeIdentity=TextHash ($head+"`n"+(($nativeInputs|ForEach-Object {$_.path+':'+$_.sha256}) -join "`n"))
foreach($entry in @($package.compileRecords)){
    if(!$entry.sha256 -or (Hash $entry.path) -ne $entry.sha256){throw 'Compile record bytes changed.'}
    $record=Get-Content -LiteralPath $entry.path -Raw|ConvertFrom-Json
    if($compileTargets.ContainsKey($entry.target) -or $entry.target -notin @('LyraEditor','LyraGame') -or $record.target -ne $entry.target -or
        $record.status -ne 'passed' -or $record.exitCode -ne 0 -or $record.sourceHead -ne $head -or
        !$record.PSObject.Properties['failureSignals'] -or @($record.failureSignals).Count -or !$record.startedUtc){throw 'Successful bound compile record required.'}
    if(!$record.originalCompileRecordSha256 -or (Hash $record.originalCompileRecord) -ne $record.originalCompileRecordSha256 -or
        !$record.nativeSourceManifestSha256 -or (Hash $record.nativeSourceManifest) -ne $record.nativeSourceManifestSha256){throw 'Original compile/native archive binding changed.'}
    $original=Get-Content -LiteralPath $record.originalCompileRecord -Raw|ConvertFrom-Json
    foreach($field in @('status','exitCode','sourceHead','target','startedUtc','generatedUtc','log')){
        if($original.$field -ne $record.$field){throw "Bound compile record changed original field: $field"}
    }
    $compileManifest=Get-Content -LiteralPath $record.nativeSourceManifest -Raw|ConvertFrom-Json
    if($compileManifest.sourceHead -ne $head -or [datetime]$compileManifest.createdUtc -gt [datetime]$record.startedUtc){throw 'Native archive must predate its compile.'}
    foreach($file in @($compileManifest.files)){AssertBytes (ChildPath $compileManifest.archive $file.path) $file}
    $compiledInputs=@($compileManifest.files|Where-Object path -Match $nativePattern|Sort-Object path)
    $compiledIdentity=TextHash ($head+"`n"+(($compiledInputs|ForEach-Object {$_.path+':'+$_.sha256}) -join "`n"))
    if($compiledIdentity -ne $nativeIdentity -or $record.nativeSourceIdentity -ne $nativeIdentity -or
        $record.nativeInputCount -ne $nativeInputs.Count -or $compiledInputs.Count -ne $nativeInputs.Count -or
        $record.binarySha256 -notmatch '^[A-Fa-f0-9]{64}$' -or !$record.binaryLastWriteUtc -or
        [datetime]$record.binaryLastWriteUtc -lt [datetime]$record.startedUtc -or
        ($entry.target -eq 'LyraGame' -and $record.binarySha256 -ne $ExpectedExeSHA256)){throw 'Final native inputs/produced binary differ from bound compile.'}
    $compileTargets[$entry.target]=$true
}
if($compileTargets.Count -ne 2){throw 'Editor and game compile records required.'}
$candidateRoot=Join-Path $root "Saved/StagedBuilds/$Candidate"
if([IO.Path]::GetFullPath($package.stage) -ne [IO.Path]::GetFullPath($candidateRoot)){throw 'Package stage path differs from candidate.'}
$artifactManifest=(Resolve-Path -LiteralPath $package.hashManifest).Path
if($artifactManifest -ne (Join-Path $candidateRoot 'sha256.csv')){throw 'Candidate-root artifact manifest required.'}
$artifacts=@(Import-Csv -LiteralPath $artifactManifest)
$artifactPaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($entry in $artifacts){
    $artifactPath=ChildPath $candidateRoot $entry.path
    if(!$artifactPaths.Add($artifactPath)){throw 'Duplicate package artifact.'}
    AssertBytes $artifactPath $entry
}
$runtimeSaved=(Join-Path $candidateRoot 'Windows/ProjectB/Saved')+[IO.Path]::DirectorySeparatorChar
$extraArtifacts=@(Get-ChildItem -LiteralPath $candidateRoot -File -Recurse|Where-Object {
    $_.FullName -ne $artifactManifest -and !$artifactPaths.Contains($_.FullName) -and
    (!$_.FullName.StartsWith($runtimeSaved,[StringComparison]::OrdinalIgnoreCase) -or $_.Extension -in '.exe','.dll','.pak','.ucas','.utoc','.uasset','.umap')
})
if(!$artifacts.Count -or $artifacts.Count -ne $package.artifactCount -or $extraArtifacts.Count -or !$artifactPaths.Contains($exe) -or
    $package.executableSha256 -ne $ExpectedExeSHA256){throw 'Complete package artifact binding required.'}
function Get-LiveUnrealProcesses {
    @(Get-CimInstance Win32_Process -ErrorAction Stop|Where-Object {
        $_.Name -match '^(LyraGame|UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool)\.exe$' -or
        ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
    }|Select-Object ProcessId,Name,ExecutablePath,CommandLine)
}
$processes=@(Get-LiveUnrealProcesses)
if($processes.Count){throw 'Existing Unreal game/editor/build process; do not launch duplicate.'}
& $runner -Candidate $Candidate -Attempt $Attempt -ExpectedLagMs $ExpectedLagMs -ExpectedExeSHA256 $ExpectedExeSHA256 -Scenario $Scenario -EvidenceDirectory $EvidenceDirectory -ValidateOnly
$contractPath=(Resolve-Path -LiteralPath $GateContractPath).Path
$contract=Get-Content $contractPath -Raw|ConvertFrom-Json
if($contract.candidate -ne $Candidate -or !$contract.counts.PSObject.Properties['packagedRuns']){throw 'Ledger contract candidate/counts differ.'}
if($ValidateOnly){
    [ordered]@{status='preflight-only';candidate=$Candidate;scenario=$Scenario;sourceHead=$head;effectiveSourceIdentity=$frozen.effectiveSourceIdentity;sourceSnapshotSHA256=$frozenSnapshotHash;reviewedNativeSHA256=$ReviewedNativeSHA256;artifactCount=$artifacts.Count}|ConvertTo-Json -Compress
    return
}
$null=New-Item -ItemType Directory -Path $d -Force
$existed=Test-Path -LiteralPath $settings -PathType Leaf
$beforeHash=if($existed){(Get-FileHash -LiteralPath $settings).Hash}else{$null}
if($existed){[IO.File]::WriteAllBytes($backup,[IO.File]::ReadAllBytes($settings))}
$guard=[ordered]@{status='prepared';settings=$settings;existedBefore=$existed;backup=$backup;beforeSHA256=$beforeHash;afterSHA256=$null;restoredSHA256=$null;restored=$false;remainingProcesses=@();error=$null;cleanupError=$null}
$guard|ConvertTo-Json -Depth 5|Set-Content $guardPath -Encoding UTF8
[ordered]@{utc=[datetime]::UtcNow.ToString('o');candidate=$Candidate;attempt=$Attempt;sourceHead=$head;effectiveSourceIdentity=$frozen.effectiveSourceIdentity;sourceManifest=$frozenManifestPath;sourceManifestSHA256=$frozenManifestHash;sourceSnapshot=$frozenSnapshotPath;sourceSnapshotSHA256=$frozenSnapshotHash;sourceIdentity=$snapshotIdentity;sourceArchive=$frozen.archive;reviewedNativeSHA256=$ReviewedNativeSHA256;runtimeInputCount=$currentInputs.Count;sourceReuseScope='Exact HEAD and build inputs; documentation and scripts may advance and are separately bound.';artifactManifest=$artifactManifest;artifactManifestSHA256=(Hash $artifactManifest);package=$packagePath;packageSHA256=(Hash $packagePath);launchPath=$exe;exeSHA256=$ExpectedExeSHA256;runner=$runner;runnerSHA256=(Hash $runner);guardRunner=$PSCommandPath;guardRunnerSHA256=(Hash $PSCommandPath);requestedTopology='two rendered D3D12 processes (host and client1 at 640x360 MaxFPS=30) plus two NullRHI witness processes (client2 and client3 at MaxFPS=30); fixed heroes and existing Dorado starts';requestedLagMs=$ExpectedLagMs;observedBeforeLaunch=0;processes=@();bridgeReady='native cooked coordinator and witnesses';uiChannel='unavailable';scenario=$Scenario;expectedObservable=($Scenario+' Barrage; strict selected-mode verifier, real cooldown and fresh cast; synthetic input only');settingsGuard=$guardPath}|
    ConvertTo-Json -Depth 5|Set-Content "$base-launch-preflight.json" -Encoding UTF8
$contract.counts.packagedRuns++
$contract.nextAction="Wait on owned Barrage $Scenario $Attempt; then verify selected-mode native evidence, natural cooldown/reuse, frame bound and settings cleanup."
$contract|ConvertTo-Json -Depth 30|Set-Content "$contractPath.tmp" -Encoding UTF8
Move-Item -LiteralPath "$contractPath.tmp" -Destination $contractPath -Force
$guard.status='running'
$guard|ConvertTo-Json -Depth 5|Set-Content $guardPath -Encoding UTF8
try {
    & $runner -Candidate $Candidate -Attempt $Attempt -ExpectedLagMs $ExpectedLagMs -ExpectedExeSHA256 $ExpectedExeSHA256 -Scenario $Scenario -EvidenceDirectory $EvidenceDirectory
} catch {
    $guard.error=$_.Exception.Message
    throw
} finally {
    try {
        # Check names, not path visibility: an inaccessible executable path must
        # never turn a live game into a false absence.
        $remaining=@(Get-LiveUnrealProcesses)
        $guard.remainingProcesses=$remaining
        if($remaining.Count){$guard.status='restore-blocked-live-process';throw 'Unreal process remains; settings restoration withheld.'}
        if(Test-Path -LiteralPath $settings){
            $guard.afterSHA256=(Get-FileHash -LiteralPath $settings).Hash
            [IO.File]::WriteAllBytes("$base-GameUserSettings.after.ini",[IO.File]::ReadAllBytes($settings))
        }
        if($existed){
            [IO.File]::WriteAllBytes($settings,[IO.File]::ReadAllBytes($backup))
            $guard.restoredSHA256=(Get-FileHash -LiteralPath $settings).Hash
            $guard.restored=($guard.restoredSHA256 -eq $beforeHash)
        } else {
            # This exact candidate-owned file was absent before this run.
            if(Test-Path -LiteralPath $settings){Remove-Item -LiteralPath $settings -Force}
            $guard.restored=!(Test-Path -LiteralPath $settings)
        }
        $guard.status=if($guard.restored){'restored'}else{'restore-failed'}
        if(!$guard.restored){throw 'Exact original settings bytes/existence were not restored.'}
    } catch {
        $guard.cleanupError=$_.Exception.Message
        if($guard.status -eq 'running'){$guard.status='cleanup-failed'}
    } finally {
        $guard|ConvertTo-Json -Depth 5|Set-Content $guardPath -Encoding UTF8
        [ordered]@{settingsRestored=$guard.restored;cleanupError=$guard.cleanupError;guard=$guardPath}|ConvertTo-Json -Compress
    }
}
if(!$guard.restored){throw 'Guarded run cleanup failed; inspect saved evidence before another launch.'}
