<#
.SYNOPSIS
    Consolidated 4-process packaged Goal 6 Match HUD, Portrait Respawn Timers, Ability Overlays,
    Round Banners, 8-Player Scoreboard/Breakdown & Rematch Reset verification at 200ms RTT.
.DESCRIPTION
    Executes on one explicit frozen packaged candidate (no build, cook, or editor actions).
    Topology: 2 D3D12 (host + client1 at 640x360 windowed -ForceRes 30 FPS) + 2 NullRHI (client2 + client3 30 FPS),
    each with -PktLag=100 (200ms RTT). Staggers client joins so client1 (D3D12) is the first remote peer and
    client3 (NullRHI) is the last remote peer (exercising real mid-match disconnect reconciliation in Step 11
    while client1 remains connected through Post-Match Breakdown screenshots and Rematch travel).
    ValidateOnly performs read-only provenance, artifact hash, process, UDP 7777, and budget checks.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^LAN-[0-9]{8}-[0-9]{2}$')][string]$Candidate,
    [Parameter(Mandatory)][ValidatePattern('^[a-z0-9][a-z0-9-]{0,27}$')][string]$Attempt,
    [Parameter(Mandatory)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ExpectedExeSHA256,
    [Parameter(Mandatory)][string]$PackageResultPath,
    [Parameter(Mandatory)][string]$GateContractPath,
    [string]$EvidenceDirectory = 'Saved/Logs/goal6-hud-20261029',
    [ValidateRange(120, 480)][int]$TimeoutSeconds = 240,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $PSScriptRoot 'PackagedTransportCommon.ps1')

function Get-FileSha256([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Resolve-ChildPath([string]$Parent, [string]$Relative) {
    if (!$Relative -or [IO.Path]::IsPathRooted($Relative)) { throw "Relative path required: $Relative" }
    $parentFull = [IO.Path]::GetFullPath($Parent).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $resolved = [IO.Path]::GetFullPath((Join-Path $parentFull $Relative))
    if (!$resolved.StartsWith($parentFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes declared root: $Relative"
    }
    $resolved
}

function Get-LiveUnrealProcesses {
    @(Get-CimInstance Win32_Process -ErrorAction Stop | Where-Object {
        $_.Name -match '^(LyraGame|UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool|AutomationTool)\.exe$' -or
        ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool')
    })
}

$evidence = [IO.Path]::GetFullPath((Join-Path $root $EvidenceDirectory))
if (!$evidence.StartsWith((Join-Path $root 'Saved') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'EvidenceDirectory must remain under project Saved/.'
}

$token = "goal6-$Candidate-$Attempt"
$directory = Join-Path $evidence $token
if (Test-Path -LiteralPath $directory) {
    throw "Preserve existing packaged attempt directory ($directory); choose a fresh -Attempt."
}

$contractPath = (Resolve-Path -LiteralPath $GateContractPath).Path
$packagePath = (Resolve-Path -LiteralPath $PackageResultPath).Path
$packageHashBefore = Get-FileSha256 $packagePath
$package = Get-Content -LiteralPath $packagePath -Raw | ConvertFrom-Json
if ($package.status -ne 'passed' -or $package.exitCode -ne 0 -or $package.candidate -ne $Candidate -or !$package.moduleRestored) {
    throw 'PackageResultPath must reference a passed, module-restored package record for Candidate.'
}

$candidateRoot = [IO.Path]::GetFullPath((Join-Path $root "Saved/StagedBuilds/$Candidate"))
$exe = Join-Path $candidateRoot 'Windows/ProjectB/Binaries/Win64/LyraGame.exe'
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw "Packaged executable missing: $exe"
}
$exeHash = Get-FileSha256 $exe
if ($exeHash -ne $ExpectedExeSHA256 -or $package.executableSha256 -ne $ExpectedExeSHA256) {
    throw "Candidate executable SHA256 mismatch (actual=$exeHash expected=$ExpectedExeSHA256 package=$($package.executableSha256))."
}

$artifactManifest = (Resolve-Path -LiteralPath $package.hashManifest).Path
$artifactManifestHashBefore = Get-FileSha256 $artifactManifest
$artifactRows = @(Import-Csv -LiteralPath $artifactManifest)
if (!$artifactRows.Count -or $artifactRows.Count -ne [int]$package.artifactCount) {
    throw 'Packaged sha256.csv row count disagrees with package-result.json.'
}
foreach ($row in $artifactRows) {
    $full = Resolve-ChildPath $candidateRoot $row.path
    $item = Get-Item -LiteralPath $full -ErrorAction Stop
    if ($item.Length -ne [long]$row.bytes -or (Get-FileSha256 $full) -ne $row.sha256) {
        throw "Staged artifact changed since packaging: $($row.path)"
    }
}

$head = (& git -C $root rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $head -ne $package.sourceHead) {
    throw "Current git HEAD ($head) differs from packaged sourceHead ($($package.sourceHead))."
}

$contract = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
if ($contract.candidate -ne $Candidate) {
    throw "Gate contract candidate ($($contract.candidate)) must match -Candidate ($Candidate)."
}
$budget = Get-BwayPackagedRunBudget $contract $(if ($ValidateOnly) { 0 } else { 1 })

$liveProcs = @(Get-LiveUnrealProcesses)
if ($liveProcs.Count -gt 0) {
    throw "Refusing packaged transport run while Unreal processes exist: $(($liveProcs | ForEach-Object { "$($_.Name):$($_.ProcessId)" }) -join ', ')"
}
$udpBefore = @(Get-NetUDPEndpoint -ErrorAction Stop | Where-Object LocalPort -eq 7777)
if ($udpBefore.Count -gt 0) {
    throw 'UDP port 7777 is already owned.'
}

$plan = [ordered]@{
    schema             = 'goal6-match-hud-transport-1'
    status             = 'preflight-only'
    candidate          = $Candidate
    attempt            = $Attempt
    token              = $token
    output             = (Join-Path $directory 'result.json')
    topology           = '2 D3D12 (640x360 -ForceRes 30 FPS) + 2 NullRHI (30 FPS)'
    packetLagMs        = 100
    simulatedRttMs     = 200
    frameCap           = 30
    numBots            = 4
    pointsToWin        = 3
    roundDurationSec   = 240
    runtimeRunsBefore  = $budget.current
    executable         = $exe
    executableSHA256   = $exeHash
    packageResult      = $packagePath
    packageResultSHA256 = $packageHashBefore
    sourceHead         = $head
    gateContract       = $contractPath
    scope              = 'Same-PC 4-process packaged listen-server + 3 clients at 200ms RTT (2 D3D12 + 2 NullRHI) with 4 bots = 8 match participants across Warmup, Live HUD/Portrait Respawn Countdowns/Cooldown & Relic Carrier Overlays, 8-Player Tab Scoreboard, 3 PostRound Win Conditions, Real Client Disconnect Reconciliation, Frozen Post-Match Breakdown & Host Rematch Reset. Physical input, 4-human play, and second-PC LAN remain Goal 9.'
}

if ($ValidateOnly) {
    $plan | ConvertTo-Json -Depth 7
    return
}

$runNumber = Add-BwayPackagedRun $contract
$contract | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath $contractPath -Encoding UTF8

$null = New-Item -ItemType Directory -Path $directory -Force
$shotsDir = Join-Path $directory 'screenshots'
$null = New-Item -ItemType Directory -Path $shotsDir -Force

$settings = Join-Path $candidateRoot 'Windows/ProjectB/Saved/Config/Windows/GameUserSettings.ini'
$settingsExisted = Test-Path -LiteralPath $settings -PathType Leaf
$settingsBytes = if ($settingsExisted) { [IO.File]::ReadAllBytes($settings) } else { $null }
if ($settingsExisted) {
    [IO.File]::WriteAllBytes((Join-Path $directory 'settings.before.ini'), $settingsBytes)
}

$owned = @{}
$records = [Collections.Generic.List[object]]::new()
$texts = @{}
$cursors = @{}
$watch = [Diagnostics.Stopwatch]::StartNew()
$errorText = $null
$hostReady = $false
$fourConnected = $false
$clientsExitedZero = $false
$settingsRestored = $false
$proof = $null

$nativeError = '(?m)^(?:\[[^\]\r\n]*\]\s*)*(?:[A-Za-z_]\w*:\s*(?:Error|Fatal):|Log\w*(?:Error|Fatal):|(?:Fatal error|Assertion failed|Ensure condition failed):)[^\r\n]*'

$result = [ordered]@{
    schema              = $plan.schema
    status              = 'running'
    candidate           = $Candidate
    attempt             = $Attempt
    token               = $token
    packagedRunNumber   = $runNumber
    topology            = $plan.topology
    packetLagMs         = 100
    simulatedRttMs      = 200
    executable          = $exe
    executableSHA256    = $exeHash
    packageResult       = $packagePath
    packageResultSHA256 = $packageHashBefore
    startedUtc          = [datetime]::UtcNow.ToString('o')
    hostReady           = $false
    fourAliveAfterConnect = $false
    clientExitZero      = $false
    allOwnedExited      = $false
    settingsRestored    = $false
    listenEndpoints     = @()
    processes           = @()
    screenshots         = @()
    proof               = $null
    warnings            = @()
    runError            = $null
}

function Save-Result {
    $result.processes = @($records.ToArray())
    $result | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath (Join-Path $directory 'result.json') -Encoding UTF8
}

function Read-OwnedLogs {
    foreach ($r in $records) {
        if (!(Test-Path -LiteralPath $r.log)) { continue }
        $stream = [IO.File]::Open($r.log, 'Open', 'Read', 'ReadWrite')
        try {
            if ($stream.Length -lt $cursors[$r.role]) { throw "Owned log rotated: $($r.log)" }
            $null = $stream.Seek($cursors[$r.role], 'Begin')
            $reader = [IO.StreamReader]::new($stream)
            $texts[$r.role] += $reader.ReadToEnd()
            $cursors[$r.role] = $stream.Position
        } finally {
            $stream.Dispose()
        }
        $bad = [regex]::Match($texts[$r.role], $nativeError)
        if ($bad.Success) {
            throw "$($r.role): $($bad.Value)"
        }
        $auditFail = [regex]::Match($texts[$r.role], '(?m)^(?:\[[^\]\r\n]*\]\s*)*LogTemp: Warning: BwayGoal6Audit: FAIL [^\r\n]+')
        if ($auditFail.Success) {
            throw "$($r.role): $($auditFail.Value)"
        }
    }
}

function Wait-Owned([scriptblock]$Condition, [int]$Seconds, [switch]$RequireAllAlive) {
    $deadline = [math]::Min($TimeoutSeconds, $watch.Elapsed.TotalSeconds + $Seconds)
    while ($watch.Elapsed.TotalSeconds -lt $deadline) {
        Read-OwnedLogs
        if ($owned.ContainsKey('host') -and $owned['host'].HasExited) {
            throw "Host exited prematurely (ExitCode=$($owned['host'].ExitCode))."
        }
        foreach ($r in $records | Where-Object { $_.role -ne 'host' }) {
            $p = $owned[$r.role]
            if ($p.HasExited) {
                if ($RequireAllAlive) {
                    throw "$($r.role) exited before 4-peer connection checkpoint (ExitCode=$($p.ExitCode))."
                }
                if ($p.ExitCode -ne 0) {
                    throw "$($r.role) exited with nonzero code ($($p.ExitCode))."
                }
            }
        }
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 250
    }
    throw "Goal 6 packaged transport deadline expired ($Seconds s window, elapsed=$([math]::Round($watch.Elapsed.TotalSeconds, 1)) s)."
}

function Start-Native([string]$Role, [string]$URL) {
    $userDir = Join-Path $directory "user-$Role"
    $log = Join-Path $directory "$Role.log"
    if ((Test-Path -LiteralPath $userDir) -or (Test-Path -LiteralPath $log)) {
        throw "Fresh process userDir/log required for $Role."
    }
    $null = New-Item -ItemType Directory -Path $userDir -Force

    $args = @(
        $URL,
        '-unattended',
        '-nosplash',
        '-nop4',
        '-nosound',
        '-MaxFPS=30',
        '-PktLag=100',
        '-ExecCmds="t.MaxFPS 30, DisableAllScreenMessages"',
        '-ExecCmds="Net PktLag=100"',
        '-ini:GameUserSettings:[/Script/LyraGame.LyraSettingsLocal]:FrameRateLimit=30.0',
        "-abslog=`"$log`"",
        "-UserDir=`"$userDir`"",
        "-enginesaveddirsuffix=$token-$Role"
    )
    if ($Role -eq 'host') {
        $args += '-ExecCmds=bway.Test.Goal6RuntimeAudit'
    } else {
        $args += '-testexit="BwayGoal6Audit: PeerReleased"'
    }

    $rendered = $Role -in @('host', 'client1')
    if ($rendered) {
        $winX = if ($Role -eq 'host') { '-WinX=20' } else { '-WinX=680' }
        $args += @('-d3d12', '-Windowed', '-ResX=640', '-ResY=360', '-ForceRes', $winX, '-WinY=20')
    } else {
        $args += '-NullRHI'
    }

    $p = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory (Split-Path -Parent $exe) -WindowStyle Hidden -PassThru
    $null = $p.Handle
    $owned[$Role] = $p
    $texts[$Role] = ''
    $cursors[$Role] = 0L

    $rec = [pscustomobject]@{
        role               = $Role
        processId          = $p.Id
        rendered           = $rendered
        args               = $args
        log                = $log
        userDirectory      = $userDir
        normalExit         = $false
        closeRequested     = $false
        forcedKill         = $false
        exited             = $false
        exitCode           = $null
        logSHA256          = $null
        logBytes           = 0
        observedExecutable = $null
    }
    $records.Add($rec)
    Save-Result

    for ($i = 0; $i -lt 20 -and !$rec.observedExecutable; ++$i) {
        try { $rec.observedExecutable = $p.MainModule.FileName } catch {}
        if (!$rec.observedExecutable) { Start-Sleep -Milliseconds 250 }
    }
    if (!$rec.observedExecutable -or [IO.Path]::GetFullPath($rec.observedExecutable) -ne [IO.Path]::GetFullPath($exe)) {
        throw "Owned process executable differs from frozen candidate for $Role."
    }
}

try {
    Save-Result
    $hostUrl = '/BreakawayCore/Maps/L_BW_HeroSelect_Staging?listen?HeroSelectStaging=1?HeroSelectTargetMap=/BreakawayCore/Maps/L_BW_Dorado?HeroSelectTargetExperience=B_BW_Experience_CaptureTheRelic?Experience=B_BW_Experience_HeroSelectStaging?NumBots=4?PointsToWin=3?RoundDuration=240?WarmupDuration=5?PostRoundDuration=3?DisableRelicBotAI=1'
    Start-Native 'host' $hostUrl

    Wait-Owned {
        $texts['host'] -match '(?m)^(?:\[[^\]\r\n]*\]\s*)*LogNet: (?:Display: )?[^\r\n]*GameNetDriver[^\r\n]*listening on port 7777\b'
    } 60 -RequireAllAlive

    $endpoints = @(Get-NetUDPEndpoint -ErrorAction Stop | Where-Object LocalPort -eq 7777)
    if (!$endpoints.Count -or @($endpoints | Where-Object OwningProcess -ne $owned['host'].Id).Count) {
        throw 'Host does not exclusively own UDP port 7777.'
    }
    $hostReady = $true
    $result.listenEndpoints = @($endpoints | Select-Object LocalAddress, LocalPort, OwningProcess)

    # Stagger joins deterministically and wait for each lagged client to complete staging map load + initial actor replication before launching the next peer.
    for ($index = 1; $index -le 3; ++$index) {
        $clientRole = "client$index"
        Start-Native $clientRole '127.0.0.1:7777'
        Wait-Owned {
            ([regex]::Matches($texts['host'], '(?m)^(?:\[[^\]\r\n]*\]\s*)*LogNet: (?:Display: )?Join succeeded:').Count -ge $index) -and
            ($texts[$clientRole] -match 'UPendingNetGame::TravelCompleted')
        } 45 -RequireAllAlive
        if ($index -lt 3) {
            Start-Sleep -Seconds 2
        }
    }

    $fourConnected = ($owned.Count -eq 4) -and (@($owned.Values | Where-Object HasExited).Count -eq 0)
    if (!$fourConnected) {
        throw 'Four live connected processes were not observed after staggered join.'
    }

    Wait-Owned {
        @(Get-BwayNativeLines $texts['host'] 'BwayGoal6Audit: PASS ').Count -eq 1
    } $TimeoutSeconds

    Wait-Owned {
        @($records | Where-Object { $_.role -ne 'host' -and !$owned[$_.role].HasExited }).Count -eq 0
    } 45

    $clientsExitedZero = $true
    foreach ($r in $records | Where-Object { $_.role -ne 'host' }) {
        if ($owned[$r.role].ExitCode -ne 0) {
            throw "$($r.role) exited with nonzero code $($owned[$r.role].ExitCode)."
        }
        $r.normalExit = $true
    }

    Read-OwnedLogs

    foreach ($r in $records) {
        if ($texts[$r.role] -notmatch '(?m)^(?:\[[^\]\r\n]*\]\s*)*LogNet: (?:Display: )?PktLag set to 100\b') {
            throw "$($r.role) log lacks 'PktLag set to 100' confirmation."
        }
    }

    $hostAuditLines = @(Get-BwayNativeLines $texts['host'] 'BwayGoal6Audit:')
    $passMatch = Get-BwayOneMarker $hostAuditLines '^BwayGoal6Audit: PASS token=\S+ worlds=1 arrivedHumans=3 realClientDisconnect=1 stages=6 frozenVerified=1 rematchResetVerified=1$' 'Host Goal6 PASS'

    $probeSummary = [ordered]@{}
    $copiedShots = [Collections.Generic.List[object]]::new()
    $disconnectRole = $null
    foreach ($candidateDisconnectRole in @('client2', 'client3')) {
        $rel = @(Get-BwayNativeLines $texts[$candidateDisconnectRole] 'BwayGoal6Audit: PeerReleased')
        if (@($rel | Where-Object { $_ -match '^BwayGoal6Audit: PeerReleased local=(-?\d+) reason=disconnect-probe$' }).Count -eq 1) {
            if ($disconnectRole) {
                throw "Multiple NullRHI clients reported disconnect-probe ($disconnectRole and $candidateDisconnectRole)."
            }
            $disconnectRole = $candidateDisconnectRole
        }
    }
    if (!$disconnectRole) {
        throw 'Expected exactly one NullRHI client (client2 or client3) to execute the Step 11 disconnect-probe.'
    }

    foreach ($role in @('host', 'client1', 'client2', 'client3')) {
        $probeLines = @(Get-BwayNativeLines $texts[$role] 'BwayGoal6Probe:')
        $requiredStages = if ($role -eq $disconnectRole) { @(1, 2, 3, 40, 41, 42) } else { @(1, 2, 3, 40, 41, 42, 5, 6) }
        foreach ($st in $requiredStages) {
            $stageHits = @($probeLines | Where-Object { $_ -match "^BwayGoal6Probe: Stage=$st\b" })
            if ($stageHits.Count -lt 1) {
                throw "$role missing BwayGoal6Probe Stage=$st evidence."
            }
        }
        if ($role -ne 'host') {
            $relLines = @(Get-BwayNativeLines $texts[$role] 'BwayGoal6Audit: PeerReleased')
            $expectedReason = if ($role -eq $disconnectRole) { 'disconnect-probe' } else { 'complete' }
            $null = Get-BwayOneMarker $relLines "^BwayGoal6Audit: PeerReleased local=(-?\d+) reason=$expectedReason`$" "$role PeerReleased"
        }

        if ($role -in @('host', 'client1')) {
            $shotMatches = @($probeLines | ForEach-Object {
                $m = [regex]::Match($_, '^BwayGoal6Probe: Screenshot stage=(\d+) role=(\S+) size=(\d+)x(\d+) path=(.+)$')
                if ($m.Success) { $m }
            })
            if ($shotMatches.Count -lt 5) {
                throw "$role captured fewer than 5 rendered D3D12 screenshots ($($shotMatches.Count))."
            }
            foreach ($sm in $shotMatches) {
                $srcPath = $sm.Groups[5].Value.Trim()
                if (!(Test-Path -LiteralPath $srcPath -PathType Leaf)) {
                    throw "$role screenshot file missing on disk: $srcPath"
                }
                $destName = "$role-$(Split-Path -Leaf $srcPath)"
                $destPath = Join-Path $shotsDir $destName
                Copy-Item -LiteralPath $srcPath -Destination $destPath -Force
                $copiedShots.Add([ordered]@{
                    role   = $role
                    stage  = [int]$sm.Groups[1].Value
                    width  = [int]$sm.Groups[3].Value
                    height = [int]$sm.Groups[4].Value
                    path   = $destPath
                    bytes  = (Get-Item -LiteralPath $destPath).Length
                })
            }
        }
        $probeSummary[$role] = $probeLines
    }

    $result.screenshots = @($copiedShots.ToArray())
    $proof = [ordered]@{
        passed               = $true
        hostPassLine         = $passMatch.Value
        arrivedHumansRematch = 3
        realClientDisconnect = $true
        disconnectRole       = $disconnectRole
        screenshotCount      = $copiedShots.Count
        probes               = $probeSummary
    }
} catch {
    $errorText = $_.Exception.Message
} finally {
    foreach ($r in @($records | Sort-Object { if ($_.role -eq 'host') { 1 } else { 0 } })) {
        $p = $owned[$r.role]
        if (!$p.HasExited) {
            $r.closeRequested = $true
            try { [void]$p.CloseMainWindow(); [void]$p.WaitForExit(5000) } catch {}
            if (!$p.HasExited) {
                $r.forcedKill = $true
                try { $p.Kill(); [void]$p.WaitForExit(5000) } catch {}
            }
        }
        $r.exited = $p.HasExited
        if ($r.exited) { $r.exitCode = $p.ExitCode }
    }

    try { Read-OwnedLogs } catch { if (!$errorText) { $errorText = $_.Exception.Message } }

    try {
        foreach ($r in $records) {
            if (Test-Path -LiteralPath $r.log) {
                $r.logSHA256 = Get-FileSha256 $r.log
                $r.logBytes = (Get-Item -LiteralPath $r.log).Length
            }
        }
        $remaining = @(Get-LiveUnrealProcesses)
        if ($remaining.Count -gt 0) {
            throw "Settings restoration withheld; live Unreal processes remain: $(($remaining | ForEach-Object { "$($_.Name):$($_.ProcessId)" }) -join ', ')"
        }
        if (Test-Path -LiteralPath $settings) {
            [IO.File]::WriteAllBytes((Join-Path $directory 'settings.after.ini'), [IO.File]::ReadAllBytes($settings))
        }
        if ($settingsExisted) {
            [IO.File]::WriteAllBytes($settings, $settingsBytes)
            $settingsRestored = (Get-FileSha256 $settings) -eq (Get-FileSha256 (Join-Path $directory 'settings.before.ini'))
        } else {
            if (Test-Path -LiteralPath $settings) { Remove-Item -LiteralPath $settings -Force }
            $settingsRestored = !(Test-Path -LiteralPath $settings)
        }
        if (((Get-FileSha256 $packagePath) -ne $packageHashBefore) -or ((Get-FileSha256 $artifactManifest) -ne $artifactManifestHashBefore)) {
            throw 'Immutable package provenance changed during runtime.'
        }
    } catch {
        if (!$errorText) { $errorText = $_.Exception.Message }
    }

    $result.hostReady = $hostReady
    $result.fourAliveAfterConnect = $fourConnected
    $result.clientExitZero = $clientsExitedZero
    $result.settingsRestored = $settingsRestored
    $result.allOwnedExited = ($records.Count -eq 4) -and (@($records | Where-Object { !$_.exited }).Count -eq 0)
    $result.proof = $proof
    $result.runError = $errorText
    $result.elapsedSeconds = [math]::Round($watch.Elapsed.TotalSeconds, 2)
    $result.finishedUtc = [datetime]::UtcNow.ToString('o')
    $result.warnings = @(foreach ($r in $records) {
        foreach ($w in [regex]::Matches($texts[$r.role], '(?m)^(?:\[[^\]\r\n]*\]\s*)*\w+: Warning:[^\r\n]*')) {
            [pscustomobject]@{ role = $r.role; line = $w.Value }
        }
    })
    $result.status = if (!$errorText -and $hostReady -and $fourConnected -and $clientsExitedZero -and $result.allOwnedExited -and $settingsRestored -and $proof -and $proof.passed) {
        'passed-transport-scope'
    } else {
        'failed-transport-scope'
    }
    Save-Result
}

[ordered]@{
    status         = $result.status
    result         = (Join-Path $directory 'result.json')
    screenshots    = $result.screenshots.Count
    elapsedSeconds = $result.elapsedSeconds
    runError       = $result.runError
} | ConvertTo-Json -Compress

if ($result.status -ne 'passed-transport-scope') { exit 1 }
