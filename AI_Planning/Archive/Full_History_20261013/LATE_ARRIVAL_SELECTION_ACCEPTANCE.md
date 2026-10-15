# Late-arrival hero selection acceptance

## Status and expected result

**Candidate51 passes packaged delayed arrival and browser regressions; Candidate53 passes packaged
rejected-travel recovery.** Twelve authority regressions and the corresponding PIE scopes also pass.
Physical controls, perceived presentation, and second-PC LAN remain open.
Recorded September 30, 2026 (October 1 UTC evidence timestamps).

`Saved/Logs/late-arrival-20261001/compile-editor05.json` passes with empty failure signals and a
newly linked runtime DLL. `automation04.json` passes all ten cases with zero test errors or
warnings, framework exit zero, and process exit zero. The cases cover live-roster validation,
preserved locks and timeout fallback, disconnect, duplicate notifications, invalid carried choices,
Prematch admission and recovery, removed-player callbacks, and teardown. They call production
selection, admission, and round-management code; the travel boundary is observed without loading
another map, so this result does not prove network travel or physical selection UI.

Earlier attempts exposed fixture setup failures: LocalPlayer ownership/component lifecycle, an
unregistered engine world context, and a missing GameInstance hero registry. The final fixture
uses the real initialized registry and authored catalog; it injects no fallback hero and retains
all timeout assertions. Full failures remain in automation01 through automation03. The latest
change only corrects fixture setup; no production assertion was weakened.

## Rejected-travel recovery and current runtime evidence

Review found that a rejected `ServerTravel` could be retried by the manager's unchanged global
timeout, while locked players had no recovery action. The phase now replicates a failure latch,
preserves existing locks, and blocks automatic retries. Only a local authority host can explicitly
retry, after a fresh live-roster check. The existing selection control shows **Retry travel** to the
host and **Waiting for host** to clients; an unlocked newcomer can still choose and lock normally.
Accepted travel or a deliberate new phase clears the failure. Duplicate click paths in one frame
cannot produce duplicate requests.

`compile-editor07.json` and `automation05.json` pass the expanded twelve-case suite. The two new
cases cover rejected travel, missing target, timeout/readiness suppression, remote denial,
membership changes, preserved locks, and explicit successful recovery. Build06 found a missing
cross-module export on `ULyraButtonBase::SetButtonText`; the method now exports `LYRAGAME_API`.

`pie-delayed01.json` passes the real three-to-four-player listen PIE test. The native probe uses
`UEditorEngine::RequestLateJoin`: the fourth connection arrives 0.58 seconds after three locks,
remains unlocked and pawnless beyond the original travel deadline, chooses through production
selection RPCs, then reaches Playing. All four worlds agree on all four hero/pawn pairs, and
exactly one travel occurs. The observer finishes, PIE stops, settings are restored, and no content
or maps are dirty. `delayed01-editor.log` preserves the evidence and three Pixel Streaming
target-window errors outside this structured gameplay scope; no physical or rendered claim is made.

`recovery-pie-state.json` separately proves real engine URL rejection, replicated failure and
visible controls on three peers, preserved nine player/hero samples, no pawns, host-only retry
availability, and rejection of both clients' retry calls. Its initial recovery transport is invalid:
calling the host's reflected retry directly from Python enables `FEditorScriptExecutionGuard`.
`AActor::GetFunctionCallspace` then forces nested ClientTravel RPCs to execute locally. The host
reached Dorado while clients remained in empty staging worlds. This fixture defect is preserved in
`recovery-transition01.json`; it is not a passing recovery result or a demonstrated production
network defect. A development-only native deferred command will repeat the real recovery outside
that guard. The original failed attempt remains immutable; use a fresh attempt record.

`compile-editor08.json` passes the development-only deferred retry command; production code and
the twelve-test suite are unchanged from editor07. `recovery-pie-native02-state.json` now passes
the complete recovery scope: three replicated failure flags, visible host/client controls with
correct labels, nine preserved locked player samples, no premature pawns, and no automatic retry
during a 63-second stable observation. Both remote retry requests are denied. After exact option
restoration, the native command records one accepted call with `scriptGuard=0`, the original host
identity, and one observer cleanup. All three peers reach Playing with nine matching hero/pawn
samples; failure flags and recovery widgets clear. The production travel count is exactly one.
`editor02.log` retains two intentional rejection errors and two Pixel Streaming target-window
errors, with no other error or critical lines. PIE stops, no content/maps are dirty, the editor
exits normally, and `editor-cleanup02.json` records restored closed-editor settings.

Runtime setup also established two Python limits: `PlayNetMode` cannot be converted from the
editor byte enum, and protected widget fields cannot be read directly. Preflight plus native
net-mode assertions cover topology; `WidgetLibrary` and object discovery read actual owned
widgets, visible ancestors, and displayed text. The closed-editor INI retained its last PIE values
despite successful in-memory restoration; `editor-cleanup01.json` records exact restoration of
the three changed keys after shutdown.

A human who joins after the current players lock heroes must still receive a selection opportunity.
Existing locked choices must remain unchanged. Travel and spawning must not strand an unlocked
player or grant a default pawn without the selected hero's gameplay setup.

This is now the selected gate after [generic slide lifecycle acceptance](<E:/Unreal Projects/ProjectB/AI_Planning/SLIDE_LIFECYCLE_ACCEPTANCE.md>).
Waiting for all four staging players before the slide fixture was a setup workaround, not this repair.

## Observed evidence

The first four-world attempt in [editor04.log](<E:/Unreal Projects/ProjectB/Saved/Logs/slide-lifecycle-20260930/editor04.log:3579>) records this sequence
(timestamps are the log's October 1 UTC timestamps):

| Evidence | Observation |
| --- | --- |
| Lines 3579–3593, `04:23:30.131` | Selection closes, schedules travel after five seconds, and reports three players ready. |
| Lines 3626–3656, `04:23:30.362` | Player four joins 231 ms later, spawns in staging with no selected hero, and receives no selection recovery. |
| Lines 4179–4180, `04:23:36.775` | Destination Prematch completion waits for all human hero locks. |
| Line 4187, `04:24:20.565` | Another `AutoLockAll` request fails because selection is inactive. |

The saved [late-arrival runtime observation](<E:/Unreal Projects/ProjectB/Saved/Logs/slide-lifecycle-20260930/late-arrival-prematch.json>) records
four Dorado worlds, each with four PlayerStates and zero characters. This is an observed
same-process PIE deadlock. The JSON preserves the initial diagnosis; the source review here refines
its scope. No additional build or test process was launched for this investigation.

## Production cause and scope

- [Hero selection completion](<E:/Unreal Projects/ProjectB/Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/HeroSystems/BwayHeroSelectionPhaseComponent.cpp:235>) closes the phase and manager before the five-second travel delay. `TryTravelToPostHeroSelectionMap` at line 519 never revalidates the current roster.
- [GameState arrival registration](<E:/Unreal Projects/ProjectB/Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/BwayGameState.cpp:54>) registers newcomers only while the manager is active. A join during the delay therefore bypasses selection.
- [Individual late-join selection](<E:/Unreal Projects/ProjectB/Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/BreakawayGameMode.cpp:166>) covers PostLogin during Warmup, Playing, or PostRound. It excludes staging and Prematch. Seamless arrivals use `HandleStartingNewPlayer` at line 263, which shows selection only while the global manager is active.
- [Prematch completion](<E:/Unreal Projects/ProjectB/Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/GameState/BwayRoundManagementComponent.cpp:390>) waits for human locks under an editor-only predicate. [That predicate](<E:/Unreal Projects/ProjectB/Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/HeroSystems/BwayHeroSelectionFlowLibrary.cpp:63>) also includes gameplay PIE reached through staging travel.

The arrival hole is production code. The complete Prematch deadlock is PIE-specific. In a packaged
build, Prematch can advance and spawn the uncovered player using default pawn data;
`ApplyHeroDataToNewPawn` returns when the hero ID is missing. This packaged missing-hero outcome
is **source-derived and requires runtime proof**. Do not report the packaged deadlock as reproduced.

## Intended repair

Keep newcomer registration and selection available while travel is pending. Revalidate every
connected human immediately before committing travel, and explicitly own and cancel the pending
travel timer. Preserve existing locks; calling `StartHeroSelection()` again resets them.

Share individual arrival handling between PostLogin and seamless arrival, including an unlocked
arrival during Prematch. Establish its selection and spawn gate before any restart attempt. Preserve
the deliberate ForceHumanoid path, valid carried hero selections, and PostMatch results behavior.
Weakening the editor's Prematch lock check alone does not repair the missing-hero path.

The source review identified these implementation constraints. Candidate50 does not contain a
late-arrival repair or establish acceptance for this issue.

- Keep pending travel under the selection phase component, with one timer handle cleared on
  roster changes and `EndPlay`. Revalidate the live `GameState.PlayerArray`, not only the manager's
  selection projection. Repeated `OnAllPlayersReady` notifications must not extend the settle
  window or issue duplicate travel. A failed travel request must not leave selection permanently closed.
- Preserve locked choices when admitting a newcomer. `BwayHeroSelectionManager::StartHeroSelection`
  resets them at line 248. Leaving the manager active also preserves its existing global timeout;
  a newcomer arriving near that deadline still needs a meaningful selection opportunity.
- Make shared GameMode admission idempotent: `Super::PostLogin` can invoke
  `HandleStartingNewPlayer`. Repeated presentation calls recreate the existing widget in
  `BwayPlayerController::Client_ShowHeroSelection` at line 283.
- Establish the per-controller awaiting-selection gate before restart. The
  [SkipHeroSelection bypass](<E:/Unreal Projects/ProjectB/Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/BreakawayGameMode.cpp:714>) otherwise permits destination spawning without checking a missing hero.
- Complete admission through the round orchestrator. The deferred
  [late-join lock callback](<E:/Unreal Projects/ProjectB/Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/BreakawayGameMode.cpp:741>) currently removes the gate and calls `RestartPlayer`, which Prematch blocks.
  Notify `TryCompletePrematchAfterHeroLocks` after the manager finishes its update; let Warmup own
  initial spawning. Adding Prematch registration alone would leave the observed PIE wait unresolved.
- Preserve deferred readiness evaluation in `UnregisterPlayer` while disconnect/bot replacement
  finishes. Keep callbacks weak and remove pending admission/restart state on logout and teardown.

## Acceptance cases

| Case | Required result |
| --- | --- |
| Fourth player joins during the settle window after three locks | Newcomer can choose; first three hero IDs and locks remain unchanged; no unselected pawn appears; exactly one travel occurs after valid selections. |
| Unlocked player arrives during seamless travel or Prematch | Destination registers the player, offers selection, prevents premature spawning, and eventually reaches Playing with the correct hero applied. |
| Newcomer disconnects before locking | Disconnected state and callbacks are removed; remaining ready players can progress without duplicate travel or a permanent wait. |
| Arrival races with the travel callback or global selection timeout | Live roster validation catches an unregistered newcomer; selection remains usable; no immediate fallback consumes the newcomer's opportunity. |
| Duplicate arrival and lock notifications | Existing locks and selection UI remain stable; one pending travel and at most one initial spawn per player. |
| Teardown or failed travel with a pending timer | No delayed travel/spawn runs during teardown; failure leaves an actionable selection state without silently retrying unchanged. |
| Existing arrival behavior | Active-match late join still selects and spawns correctly; PostMatch arrival still receives results; valid carried choices and ForceHumanoid behavior survive; rematch still deliberately resets selection. |

Use a deterministic authority regression for the pending-travel roster change, then exercise real
remote-owner selection requests in four-player listen-server PIE. Repeat the delayed-arrival cases
with a fresh packaged candidate and independent processes. Require four valid locked heroes,
correct pawn hero setup, and eventual Playing. Physical selection UI and second-PC LAN retain their
separate acceptance requirements.

Preserve topology, source snapshot, candidate identity, timing, per-player hero/lock/pawn state,
travel count, cleanup, and first causal error. Retry only after a changed fix or setup hypothesis.
Changes to admission, selection, travel, or spawn gating invalidate the corresponding proof.

## Candidate51 packaged admission

`compile-game01.json` passes with empty failure signals. Candidate51 Cook/Stage/Pak exits zero;
the manifest contains 131 staged hashes. Its frozen source includes the full tracked patch and
144 untracked files with archived contents and individual hashes. No runtime input changed after
the game compile. Candidate50 remains the earlier slide reference.

`packaged-LAN-20260930-51-delayed02-result.json` passes with four independent processes on one PC.
The fourth connection arrives 1.10 seconds after the original three locks, remains unlocked and
pawnless past the original five-second deadline, and requests a different hero after a 6.01-second
hold. Its selection is echoed before the actual remote lock request. Original choices remain
unchanged. Exactly one production travel occurs; all four peers agree on 16 hero/pawn rows in
Playing. The three clients exit zero before the retained host is terminated by owned cleanup.
`packaged-delayed02-flow-brief.json` records zero error or critical lines in all four logs.

Attempt01 is invalid fixture evidence: PowerShell's case-insensitive failure pattern matched the
normal startup field `timeout=180`, aborting before the fourth client launched. The downstream
host fixture error occurred during that aborted setup. The corrected parser uses case-sensitive
failure markers; all 13 controls pass, including normal startup lines and an actual timeout.
The retry reused the identical frozen package, without a build or cook. Preserve `delayed01-diagnosis.json`
and both attempts. Current gate overhead is eight editor builds, one game build, five automation
runs, four PIE starts, one package, and two packaged admission attempts.

This proves admission and selection transport through direct listen-server connection, with
NullRHI and synthetic selection requests. It does not prove browser navigation, perceived UI,
physical input, packaged rejected-travel recovery, or second-PC networking.

## Candidate51 browser regressions

All three affected browser regressions pass on the same frozen Candidate51, with four rendered
independent processes at 200 ms packet lag. Each detailed checker passes, all clients exit zero,
and all twelve logs contain zero error or critical lines.

| Scope | Result | Evidence directory under `Saved/Logs/` |
| --- | --- | --- |
| SelectionEdges | Invalid/duplicate requests, repeated locks, selection-time disconnect and browser rejoin; 32 peer rows, four passes. | `selection-edges-20260930-51/` |
| Connected | Actual browser joins, results-widget return, connected frontend roster, rehost and second gameplay roster; 80 peer rows, three discoveries, four passes. | `lan-connected-20260930-51/` |
| Rediscovery | Full disconnect and standalone recovery, new session discovery/rejoin and second gameplay roster; 80 peer rows, six discoveries, four passes. | `lan-rediscovery-20260930-51/` |

Each directory contains `runtime-brief.json`, `flow-brief.json`, the transport result, and four
raw logs. `codex-packaged-LAN-20260930-51/artifact-verification-reviewed.json` verifies all 131
staged hashes, 144 archived untracked inputs, current compiled-input content, and matching
compiled/staged executable hashes. The chunk-layout file was touched by packaging but its
nine-byte content exactly matches the frozen input; the original timestamp-only rejection and
source-based classification are preserved. Only two planning documents differ from the freeze.

These checks add three packaged runs, without another compile or package. Their synthetic match
completion and widget calls do not establish physical navigation, perceived presentation, natural
human matches, or second-PC LAN.

## Candidate53 packaged rejected-travel recovery

Candidate53 passes the development-only recovery fixture in four rendered independent processes
with observed 200 ms packet lag. Every peer verifies its actual local activated widget, visible
button/details, correct host **Retry travel** or client **Waiting for host** label, and enabled state.
All four original locked choices remain unchanged and pawnless throughout 65.01–65.08 seconds.
The real global selection timeout fires once without an automatic retry. All three remote retry
calls are denied. One explicit native host retry, after every peer completes the failure checks,
produces exactly one accepted travel and 16 matching initialized hero/pawn rows in Playing.

A second shared barrier keeps clients alive until all four peers verify recovery. All four release
their observers; all three clients exit zero before owned host teardown. Exact original travel
options are restored, the attempt-owned control file is removed, and no game/editor process remains.
The two intentional invalid-URL rejection errors are retained and classified separately: one engine
error and one selection-phase error. There are zero unexpected errors or critical lines. All 136
warnings remain in the result and a separate inventory; this is not a warning-free presentation claim.

Evidence directory: `Saved/Logs/selection-recovery-LAN-20260930-53-recovery01/`.
`acceptance-brief.json` joins the verdict and counts; `result.json`, `flow-brief.json`,
`warning-inventory.json`, and four logs preserve the complete run. Candidate53's source and all
131 staged hashes pass `codex-packaged-LAN-20260930-53/artifact-verification.json`; 145 untracked
inputs are archived with their contents and hashes.

The probe adds no reflected types, assets, or production behavior, so only LyraGame needed rebuilding.
Two game builds and two packages were used. Candidate52 packaged successfully but was superseded
before runtime after late review added a replicated player-ID readiness guard. The final runner's
36 controls and independent review pass, including malformed/early release, divergent rosters,
raw critical errors, late failure, and bounded Windows file-sharing retries. Candidate53's first
runtime attempt passes. Freeze both native and runner reviews before future packaging to avoid
another preventable superseded artifact.

**Next action:** physical selection/retry input, perceived presentation, second-PC LAN, and final
integrated acceptance remain open. Continue the independent slide gravity and camera-shake ownership
repair next; the admission, browser, and structured recovery scopes above are complete.
