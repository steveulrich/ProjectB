<# Isolated Barrage lifecycle cooked transport. Derived from run-rawlins-packaged06.ps1;
   no process is started by parsing/importing this file. Root owns invocation. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^LAN-[0-9]{8}-[0-9]+$')][string]$Candidate,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,70}$')][string]$Attempt,
    [Parameter(Mandatory)][ValidateSet(0,100)][int]$ExpectedLagMs,
    [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ExpectedExeSHA256,
    [string]$EvidenceDirectory = 'Saved/Logs/goal1-barrage-lifecycle-20261016',
    [Parameter(Mandatory)][ValidateSet('normal','aim','damage-only','heal-damage','heal-only','death','round-reset')][string]$Scenario,
    [switch]$ValidateOnly
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$exe = Join-Path $root "Saved/StagedBuilds/$Candidate/Windows/ProjectB/Binaries/Win64/LyraGame.exe"
$evidence = [IO.Path]::GetFullPath((Join-Path $root $EvidenceDirectory))
$savedRoot = [IO.Path]::GetFullPath((Join-Path $root 'Saved')) + [IO.Path]::DirectorySeparatorChar
if (-not $evidence.StartsWith($savedRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Evidence directory must remain under project Saved.' }
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Packaged executable not found: $exe" }
$exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
if ($exeHash -ne $ExpectedExeSHA256) { throw 'Packaged executable differs from the root-approved frozen artifact.' }
$prefix = "packaged-$Candidate-$Attempt"
$resultPath = Join-Path $evidence "$prefix-result.json"
$userRoot = Join-Path $evidence "$prefix-users"
$logs = @{}
foreach ($role in @('host','client1','client2','client3')) { $logs[$role] = Join-Path $evidence "$prefix-$role.log" }
foreach ($path in @($resultPath,$userRoot)+@($logs.Values)) {
    if (Test-Path -LiteralPath $path) { throw "Preserve prior evidence; choose a fresh attempt: $path" }
}
$active = @(Get-Process -Name LyraGame,UnrealEditor,UnrealEditor-Cmd,UnrealBuildTool -ErrorAction SilentlyContinue)
if ($active.Count) { throw 'An editor/game/build process is already running.' }
$udpBefore = @(Get-NetUDPEndpoint -ErrorAction Stop | Where-Object LocalPort -eq 7777)
if($udpBefore.Count){throw "UDP 7777 is already owned by process(es): $(($udpBefore.OwningProcess|Sort-Object -Unique) -join ',')."}
# The guard owns frozen source/package approval; the verifier requires an
# explicit independently reviewed native hash and audits every native marker.
$hostCommand = 'bway.Test.BarrageTransportHost'
$clientCommand = 'bway.Test.BarrageTransportClient'
$nativePrefix = 'BwayBarrageTransport'
$isLifecycle = $Scenario -notin @('normal','aim')
$completionPrefix = if($isLifecycle){'BwayBarrageLifecycle'}else{$nativePrefix}
$proofScope = switch($Scenario){
    'normal' {'Cooked four-process normal Barrage delivery, real cooldown and ordinary reuse. Synthetic fixture; no physical input, final appearance or second-PC LAN proof.'}
    'aim' {'Cooked four-process Barrage delivery, real cooldown/reuse and changing-yaw observation; strict normal and supplemental aim verifiers both required. Synthetic fixture; no physical input, final appearance or second-PC LAN proof.'}
    default {'Cooked four-process exact remote-owner Barrage lifecycle, production health/death/reset intervention, natural cooldown and fresh cast. Synthetic fixture; no physical input, final appearance or second-PC LAN proof.'}
}
if ($ValidateOnly) {
    [ordered]@{status='validated-artifact-only';candidate=$Candidate;attempt=$Attempt;executable=$exe;executableSHA256=$exeHash;
        expectedLagMs=$ExpectedLagMs;udpPortAvailable=$true;resultJson=$resultPath} | ConvertTo-Json -Compress
    return
}
$null = New-Item -ItemType Directory -Path $evidence -Force
$startedUtc = [DateTime]::UtcNow.ToString('o')
$watch = [Diagnostics.Stopwatch]::StartNew()
$owned = [Collections.Generic.List[object]]::new()
$records = [Collections.Generic.List[object]]::new()
$startupReadiness = [Collections.Generic.List[object]]::new()
$logOffsets = @{}
$runError = $null
$hostReady = $false
$allConnectedAlive = $false
$clientsExitedNaturally = $false
$listenHost = $null
$checkpointAt = $null
$listenEndpoints = @()
$nativeFailurePattern = '(?m)^(?:\[[^\]\r\n]*\]\s*)*\s*(?:[A-Za-z_]\w+:\s*(?:Error|Fatal):|Log\w*(?:Error|Fatal):|(?:Fatal error|Assertion failed|Ensure condition failed):)[^\r\n]*'

function Assert-NoNativeErrors([string]$Role,[string]$Text) {
    $failure = [regex]::Match($Text,$nativeFailurePattern)
    if($failure.Success){
        $sample=$failure.Value.Substring(0,[Math]::Min(600,$failure.Value.Length))
        throw "$Role emitted an unclassified native failure: $sample"
    }
}

function Read-AppendedLog {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return '' }
    if (-not $logOffsets.ContainsKey($Path)) { $logOffsets[$Path] = @{offset=0L;text=''} }
    $state = $logOffsets[$Path]
    $stream = [IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    try {
        if ($stream.Length -lt $state.offset) { throw 'Unexpected log rotation.' }
        $null = $stream.Seek($state.offset,[IO.SeekOrigin]::Begin)
        $reader = [IO.StreamReader]::new($stream)
        $state.text += $reader.ReadToEnd()
        $state.offset = $stream.Position
    } finally { $stream.Dispose() }
    return $state.text
}

function Start-OwnedProcess {
    param([string]$Role,[string[]]$Arguments)
    $userDirectory = Join-Path $userRoot $Role
    if(Test-Path -LiteralPath $userDirectory){throw 'Per-process settings directory must be fresh.'}
    $null = New-Item -ItemType Directory -Path $userDirectory
    $engineSavedSuffix = "$Candidate-$Attempt-$Role"
    $arguments = @($Arguments)+@('-ExecCmds="t.MaxFPS 30"','-LogCmds="LogPlayerController Verbose"',
        '-nosound','-unattended','-nosplash',"-abslog=`"$($logs[$Role])`"", "-UserDir=`"$userDirectory`"", "-enginesaveddirsuffix=$engineSavedSuffix")
    # Fixture-only deterministic existing starts and a cap that survives later settings updates.
    $arguments += @('-BwayBarrageTransportDeterministicStarts',
        '-ini:GameUserSettings:[/Script/LyraGame.LyraSettingsLocal]:FrameRateLimit=30.0',
        '-ExecCmds="getall LyraSettingsLocal FrameRateLimit"')
    if($isLifecycle){$arguments += "-BwayBarrageLifecycle=$Scenario"}
    elseif($Scenario -eq 'aim'){$arguments += '-BwayBarrageTransportChangingAim'}
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $exe) -WindowStyle Hidden -PassThru
    $null = $process.Handle
    $owned.Add($process)
    $records.Add([ordered]@{role=$Role;log=$logs[$Role];processId=$process.Id;args=$arguments;executable=$exe;
        userDirectory=$userDirectory;userDirectoryWasAbsent=$true;engineSavedSuffix=$engineSavedSuffix;
        startedUtc=[DateTime]::UtcNow.ToString('o');startedAt=$watch.Elapsed.TotalSeconds;normalExit=$false;
        closeRequested=$false;forcedKill=$false;cleanupReason=$null})
    return $process
}

function Wait-NativeMarker {
    param([string]$Role,[string]$Pattern,[int]$TimeoutSeconds)
    $deadline = $watch.Elapsed.TotalSeconds+$TimeoutSeconds
    while ($watch.Elapsed.TotalSeconds -lt $deadline) {
        foreach ($p in $owned) {
            if ($p.HasExited) { throw "Owned process $($p.Id) exited before the live-fixture checkpoint (exit $($p.ExitCode))." }
        }
        foreach($record in $records){Assert-NoNativeErrors $record.role (Read-AppendedLog -Path $record.log)}
        $text = Read-AppendedLog -Path $logs[$Role]
        if ($text -match $Pattern) { return $true }
        Start-Sleep -Milliseconds 250
    }
    return $false
}

try {
    $listenHost = Start-OwnedProcess -Role 'host' -Arguments @(
        '/BreakawayCore/Maps/L_BW_HeroSelect_Staging?listen?HeroSelectStaging=1?HeroSelectTargetMap=/BreakawayCore/Maps/L_BW_Dorado?HeroSelectTargetExperience=B_BW_Experience_CaptureTheRelic?Experience=B_BW_Experience_CaptureTheRelic?NumBots=0?PointsToWin=9?RoundDuration=600?WarmupDuration=3?PostRoundDuration=8',
        '-d3d12','-windowed','-ResX=640','-ResY=360','-ForceRes','-MaxFPS=30','-WinX=20','-WinY=20',"-PktLag=$ExpectedLagMs","-ExecCmds=$hostCommand")
    $hostReady = Wait-NativeMarker -Role 'host' -Pattern '(?m)^(?:\[[^\]\r\n]*\]\s*)*LogNet: (?:Display: )?[^\r\n]*GameNetDriver[^\r\n]*listening on port 7777\b' -TimeoutSeconds 45
    if (-not $hostReady) { throw 'Host did not emit its actual GameNetDriver listening marker.' }
    $listenEndpoints = @(Get-NetUDPEndpoint -ErrorAction Stop | Where-Object LocalPort -eq 7777 | Select-Object LocalAddress,LocalPort,OwningProcess)
    if(!$listenEndpoints.Count -or @($listenEndpoints|Where-Object OwningProcess -ne $listenHost.Id).Count){throw 'UDP 7777 ownership does not match the owned listen host.'}
    foreach ($index in 1..3) {
        $role = "client$index"
        $roleArgs = if ($index -eq 1) {
            @('127.0.0.1:7777','-d3d12','-windowed','-ResX=640','-ResY=360','-ForceRes','-MaxFPS=30',
              '-WinX=680','-WinY=20','-seconds=220',"-PktLag=$ExpectedLagMs","-ExecCmds=$clientCommand")
        } else {
            @('127.0.0.1:7777','-NullRHI','-MaxFPS=30','-seconds=220',"-PktLag=$ExpectedLagMs","-ExecCmds=$clientCommand")
        }
        Start-OwnedProcess -Role $role -Arguments $roleArgs | Out-Null
        $ready = Wait-NativeMarker -Role $role -Pattern ('LogTemp: Display: '+[regex]::Escape($nativePrefix)+': Connected role=client') -TimeoutSeconds 20
        $startupReadiness.Add([ordered]@{role=$role;ready=$ready;marker='native staging PlayerState connection';at=$watch.Elapsed.TotalSeconds})
        if (-not $ready) { throw "$role failed independent staging readiness." }
    }
    foreach ($role in @('host','client1','client2','client3')) {
        if (-not (Wait-NativeMarker -Role $role -Pattern ('(?m)^(?:\[[^\]\r\n]*\]\s*)*LogTemp: Display: '+[regex]::Escape($completionPrefix)+': Complete (?=[^\r\n]*\bpass=1\b)(?=[^\r\n]*\bcleanup=1\b)') -TimeoutSeconds 195)) {
            throw "$role did not finish Barrage $Scenario observation."
        }
    }
    $allConnectedAlive = $owned.Count -eq 4 -and @($owned | Where-Object HasExited).Count -eq 0
    if (-not $allConnectedAlive) { throw 'Four completed probes were not simultaneously alive.' }
    $checkpointAt = $watch.Elapsed.TotalSeconds
    while (-not $clientsExitedNaturally) {
        if ($listenHost.HasExited) { throw 'Host exited before every client completed its engine time limit.' }
        foreach($record in $records){Assert-NoNativeErrors $record.role (Read-AppendedLog -Path $record.log)}
        $clientsExitedNaturally = $true
        foreach ($record in @($records | Where-Object role -ne 'host')) {
            $p = @($owned | Where-Object Id -eq $record.processId)[0]
            if ($p.HasExited) {
                $record.normalExit = $true
                if ($p.ExitCode -ne 0) { throw "$($record.role) exited with $($p.ExitCode)." }
            } else {
                $clientsExitedNaturally = $false
                if ($watch.Elapsed.TotalSeconds-$record.startedAt -gt 250) { throw "$($record.role) exceeded its bounded engine exit allowance." }
            }
        }
        if (-not $clientsExitedNaturally) { Start-Sleep -Milliseconds 250 }
    }
} catch { $runError = $_.Exception.Message }
finally {
    foreach ($record in $records) {
        $p = @($owned | Where-Object Id -eq $record.processId)[0]
        if (-not $p.HasExited) {
            $record.cleanupReason = if ($record.role -eq 'host' -and $clientsExitedNaturally) {'owned-host-after-all-client-exits'} else {'owned-failure-teardown'}
            $record.closeRequested = $true
            try { [void]$p.CloseMainWindow(); [void]$p.WaitForExit(5000) } catch { }
            if (-not $p.HasExited) {
                $record.forcedKill = $true
                try { $p.Kill(); [void]$p.WaitForExit(5000) } catch { }
            }
        }
        $record.exited = $p.HasExited
        $record.exitCode = if ($p.HasExited) {[int]$p.ExitCode} else {$null}
        $record.logBytes = 0; $record.logSHA256 = $null; $record.runtimeSettingsFiles = @()
        try {
            if (Test-Path -LiteralPath $record.log -PathType Leaf) {
                $record.logBytes = (Get-Item -LiteralPath $record.log).Length
                $record.logSHA256 = (Get-FileHash -LiteralPath $record.log -Algorithm SHA256).Hash
                Assert-NoNativeErrors $record.role (Read-AppendedLog -Path $record.log)
            }
            $record.runtimeSettingsFiles=@(Get-ChildItem -LiteralPath $record.userDirectory -Filter GameUserSettings.ini -Recurse -File | ForEach-Object {
                [ordered]@{path=$_.FullName;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
            })
        } catch {if(!$runError){$runError=$_.Exception.Message}}
    }
}
$hashAfter = $null
try {$hashAfter = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash}
catch {if(!$runError){$runError=$_.Exception.Message}}
$clientExitZero = @($records | Where-Object role -ne 'host').Count -eq 3 -and @($records | Where-Object {
    $_.role -ne 'host' -and (-not $_.normalExit -or $_.exitCode -ne 0 -or $_.closeRequested -or $_.forcedKill)}).Count -eq 0
$allOwnedExited = $records.Count -eq 4 -and @($records | Where-Object {-not $_.exited}).Count -eq 0
$result = [ordered]@{candidate=$Candidate;attempt=$Attempt;expectedLagMs=$ExpectedLagMs;latencyCommand="-PktLag=$ExpectedLagMs";
    latencyScope='Requested here; effective per-process driver settings are mandatory native verifier evidence.';
    fixture="$hostCommand / $clientCommand";scenario=$Scenario;schema=$(if($isLifecycle){'barrage-lifecycle-1'}else{'barrage-transport-1'});scope=$proofScope;
    executable=$exe;executableSHA256=$exeHash;executableSHA256After=$hashAfter;approvedExecutableSHA256=$ExpectedExeSHA256;
    runnerSHA256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash;
    startupReadiness=@($startupReadiness);hostReady=$hostReady;processes=@($records);clientExitZero=$clientExitZero;
    udpPortAvailableBefore=$true;listenEndpoints=$listenEndpoints;
    allConnectedAlive=$allConnectedAlive;liveCheckpointAt=$checkpointAt;allOwnedExited=$allOwnedExited;
    hostTeardown='clients-engine-seconds-limit; host-owned-teardown-after-all-client-exits';
    runtimeSettingsPolicy='Fresh per-process UserDir under attempt evidence and unique engine Saved suffix; original staged settings bytes/existence also preserved.';
    runError=$runError;startedUtc=$startedUtc;finishedUtc=[DateTime]::UtcNow.ToString('o');elapsedSeconds=$watch.Elapsed.TotalSeconds;
    status=if ($hostReady -and $allConnectedAlive -and $clientExitZero -and $allOwnedExited -and $hashAfter -eq $exeHash -and $null -eq $runError) {'passed-transport-scope'} else {'failed-transport-scope'}}
$tempPath = $resultPath+'.tmp'
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $tempPath -Encoding UTF8
Move-Item -LiteralPath $tempPath -Destination $resultPath
[ordered]@{status=$result.status;candidate=$Candidate;attempt=$Attempt;resultJson=$resultPath;hostReady=$hostReady;
    clientExitZero=$clientExitZero;allOwnedExited=$allOwnedExited;runError=$runError} | ConvertTo-Json -Compress
if($result.status -ne 'passed-transport-scope'){throw "Barrage $Scenario transport failed: $runError"}
