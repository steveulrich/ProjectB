<# Creates reviewable native console commands. Does not start an editor, PIE, fixture, or capture. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9_-]{1,48}$')][string]$Token,
    [Parameter(Mandatory)][ValidateRange(0,2147483647)][int]$HostPlayerId,
    [Parameter(Mandatory)][ValidateRange(0,2147483647)][int]$RemotePlayerId
)
$ErrorActionPreference='Stop'
if($HostPlayerId -eq $RemotePlayerId){throw 'Select distinct actual local PlayerState IDs for the host and rendered remote.'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$native=Join-Path $root 'Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/Development'
$probe=Join-Path $native 'BwayAllKitCombatProbe.cpp'
$capture=Join-Path $native 'BwayPresentationCaptureCommand.cpp'
$actions=@([regex]::Matches([IO.File]::ReadAllText($probe),'\{TEXT\("(?<hero>[^"\r\n]+)"\), TEXT\("(?<ability>BwayGameplayAbility_[^"\r\n]+)"\), (?<duration>[.0-9]+)f\}') | ForEach-Object {
    [ordered]@{hero=$_.Groups['hero'].Value;ability=$_.Groups['ability'].Value;fixtureSeconds=[double]::Parse($_.Groups['duration'].Value,[Globalization.CultureInfo]::InvariantCulture)}
})
if($actions.Count -ne 20){throw "Expected 20 source-confirmed AllKit cases; found $($actions.Count). Reconcile the plan with the current probe."}
for($i=0;$i -lt $actions.Count;$i++){$actions[$i].case=$i}
function Sequence([string]$Name,[int]$Count,[double]$Interval){
    $seconds=$Interval.ToString('0.00',[Globalization.CultureInfo]::InvariantCulture)
    "bway.Test.CapturePresentationSequence $Token-$Name $HostPlayerId $Name $Count $seconds $RemotePlayerId"
}
$jobs=@(
    [ordered]@{
        name='all20-actions';commands=@('bway.Test.CueAudit',"bway.Test.CaptureAllKit $Token-allkit $HostPlayerId $RemotePlayerId",'bway.Test.AllKitOwnerHost')
        completion='BwayPresentationCapture: SessionFinished; BwayAllKitOwner: PASS actions=20 controls=20 creditCases=2 observers=4 cleanup=1'
        review='Require actual metadata active/outcome coverage for cases 0..19 in both observer views; identify which cases each observer owns. Three requested active offsets are 0.10, 0.35, and clamp(0.8*fixtureDuration,0.40,1.10) seconds after each observer sees Stage 2. A missing or stale phase is a gap. Inspect body animation, contacts, hit numbers, directional feedback, effects, HUD and duplicate presentation. Rotate the rendered remote when the gate needs every hero owner view.'
    },
    [ordered]@{
        name='siege';commands=@((Sequence 'siege-start' 40 .2),'bway.Test.SiegeCombatAudit')
        laterWindows=@('During each later sequence, arm a fresh sequence token before the next Prepare transition; capture deployment, firing/impact, blocked shot and lethal destruction. Each case lasts at least 14.5 seconds.')
        markers='BwaySiegeCombat: Setup / Verify / Complete'
    },
    [ordered]@{
        name='shrine';commands=@((Sequence 'shrine-start' 40 .2),'bway.Test.ShrineAuraAudit')
        laterWindows=@('Arm a fresh 40 x 0.2-second window during phase 2 for owner-death/respawn continuity in phase 3.','Arm another during phase 8, before destruction in phase 9. Normal phases settle for at least 5.2 seconds.')
        markers='BwayShrineAura: Begin phase=N / Verify / Complete'
    },
    [ordered]@{
        name='ward';commands=@((Sequence 'ward-start' 40 .2),'bway.Test.WardAuraAudit')
        laterWindows=@('Arm during phase 5 for the second deployment in phase 6 and first destruction in phase 7.','Arm during phase 11 for round reset in phase 12 and final destruction in phase 13. Normal phases settle for at least 5.2 seconds.')
        markers='BwayWardAura: Begin phase=N / Verify / Complete'
    },
    [ordered]@{
        name='jail';commands=@((Sequence 'jail-start' 40 .2),'bway.Test.JailCaptureAudit')
        laterWindows=@('Arm during phase 1 to capture remote-owner damage in phase 2 and actual destruction in phase 3.','Arm during phase 4 for new traps/capture in phase 5 and round-reset cleanup in phase 6. Normal phases settle for at least 5.2 seconds.')
        markers='BwayJailCapture: Begin phase=N / Verify / Complete'
    },
    [ordered]@{
        name='slide';commands=@((Sequence 'slide-groundair' 40 .05),"bway.Test.SlideGroundAirDiagnosticPIE $Token-groundair")
        laterWindows=@("On a separate restored fixture, arm a fresh 40 x 0.05-second window before bway.Test.SlideReviewPIE $Token-review.",'Use fresh windows at subsequent authored ramp/flat/edge/landing cases. Review successive raw frames and recorded camera FOV/world time for body motion, camera blend, shake and continuity; the sampling request does not establish 30 FPS or smoothness.')
        markers='BwaySlideGroundAirAcceptance / BwaySlidePackaged / BwaySlideReview'
        acceptance='Require all five authored Dorado cases through Verify-SlideGroundAirAcceptance.py with the current frozen native hash. Keep the legacy observation-only mode and the separate five headless collision cases distinct.'
    },
    [ordered]@{
        name='relic';commands=@((Sequence 'relic-ownership' 40 .05),'bway.Test.RelicOwnership')
        laterWindows=@('Arm fresh short windows around later pickup/pass/throw/score stages; do not overlap independent fixtures.','After cleanup, use bway.Test.Goal2RelicBasePIE for prepared catch/interception/forced-fumble cases, or bway.Test.RelicFlight for production flight/ground-impact observations. Pair each phase with a fresh capture window before its native transition.')
        markers='BwayRelicOwnership / BwayGoal2GateTest / BwayRelicFlight'
    }
)
$plan=[ordered]@{
    schemaVersion=1;generatedUtc=[DateTime]::UtcNow.ToString('o');isEvidence=$false;executionStarted=$false
    token=$Token;hostPlayerId=$HostPlayerId;renderedRemotePlayerId=$RemotePlayerId
    sourceHashes=[ordered]@{allKit=(Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash;capture=(Get-FileHash -LiteralPath $capture -Algorithm SHA256).Hash}
    requirements=@('Current coherent LyraEditor build and authored assets; owned ready four-world Dorado Playing fixture; real IDs from current readiness.','The two IDs must resolve to distinct actual local players in this process, each with a visible exact Slate viewport. In separate packaged processes, invoke each command with only that process local ID.','Issue commands in each job in order, outside gameplay Python guard through the native ticker. Run one gameplay fixture and one capture session at a time.','Fresh tokens for every window. Wait for SessionFinished before arming another window; retain native fixture completion/restoration proof.','AllKit remains the unchanged production action fixture. Rendering readback can perturb timing; preserve failed/partial outcomes and use independent gameplay runs if necessary.','Inspect pixels and metadata together. Missing/offscreen/occluded effects, stopped sessions and phase misses remain gaps. No inferred visual, physical-input, audio, haptic or network acceptance.','Capture includes no audio. A -nosound packaged run cannot prove SFX. Stop the separate cue observer with bway.Test.CueAudit.Stop after the fixture.','Bounds: at most two views, 160 captures and 512 MiB PNG per session; nine-megapixel window/32 MiB PNG maximum; >2-second readback stops remaining samples. Sequence spacing is requested, not guaranteed.')
    actions=$actions;jobs=$jobs
    output='Saved/Logs/PresentationCaptures/<window-token>/player<id>-<label>.png and matching JSON'
    review='Build labeled contact sheets from actual images for coverage, then examine full-resolution frames and short raw timelines. Metadata records world/controller/pawn/view target/action/stage/source/local ownership, RHI, FOV, camera pose, frame/world time and readback cost. Neither a contact sheet nor a native PASS line is itself visual acceptance.'
}
$directory=Join-Path $root "Saved/Logs/PresentationCaptures/$Token"
$path=Join-Path $directory 'capture-plan.json'
if(Test-Path -LiteralPath $path){throw "Preserve existing plan: $path"}
[IO.Directory]::CreateDirectory($directory) | Out-Null
[IO.File]::WriteAllText($path,($plan | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
[ordered]@{path=$path;actionCases=$actions.Count;jobs=$jobs.Count;executionStarted=$false} | ConvertTo-Json -Compress
