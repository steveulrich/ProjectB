# Respawn countdown acceptance

## Requirement and implementation

An eligible local player's death must show the time remaining until the server's scheduled
respawn. Reconstructing the widget must preserve that deadline. Respawn, round cancellation,
and match reset must clear it. Menus retain focus and input ownership.

Read-only exports of `LAS_BW_MatchHUD`, `WBP_BW_MatchHUDLayout`, and `W_BW_HealthWidget`
found no active countdown. The sample `W_RespawnTimer` has only `GA_AutoRespawn` as an asset
referencer; that ability is not granted by the Breakaway experience. Exports and the successful
commandlet record are under `Saved/Logs/respawn-parity-20260926/`, in `active-hud-assets.json`
and `inspection05-result.json`. No assets were saved by that inspection.

`ABwayPlayerState` now replicates one `FBwayRespawnSchedule` containing a server end time and
round number. The round component publishes the deadline from the actual timer's remaining
duration and clears it when consuming or cancelling that timer. Round and match resets also
clear the schedule. The server retains control of respawning humans and bots.

`ABwayPlayerController` creates one `UBwayRespawnHUDWidget` for its local player. The widget
subscribes to the PlayerState schedule, uses synchronized GameState time, and refreshes every
0.1 seconds only while a schedule exists. It shows whole seconds, then `Respawning...` while
awaiting the server update. It is collapsed outside the scheduled Playing round, is never
focusable, and uses hit-test-invisible visibility. Destruction removes subscriptions and timers.
Controller shutdown removes the widget; PlayerState replacement rebinds it.

The default UMG tree provides a centered text panel inside a safe zone. A designer can assign a
Blueprint subclass through `RespawnHUDWidgetClass`, supply `Text_RespawnStatus`, or use
`OnCountdownChanged` to skin it without changing gameplay timing. Art integration and actual
legibility still need rendered review. This implementation does not establish the original
game's increasing respawn-delay formula; the existing configured delay remains three seconds.

## Runtime evidence

Evidence directory: `Saved/Logs/respawn-countdown-20260926/`.

The selected gate is four-world Dorado listen-server PIE with four humans, four bots and
`Net PktLag=100`. `bway.Test.RespawnHUDAudit` uses the real death path, kills all four humans
twice without eliminating a team, reconstructs each active death HUD, opens and closes the
production shop during the second death, then checks elimination cancellation and round two.
The fixture restores surviving held movement and bot-brain state and closes its owned shops.

`editor01.log` is the failed baseline: all four local widgets existed and all four schedules
arrived, but client 263's input router retained Menu mode and its countdown stayed hidden.
The other three countdowns reconstructed successfully. `runtime01-verification.json` records
984 samples and the failed client checks. This attempt does not pass the gate.

The repair checks activated, visible widgets in the local player's Menu and Modal layers,
rather than inferring menu visibility from the input router's mode. The baseline does not
establish whether that input-mode state affected physical controls; retain that question for
the physical-input gate. `compile-editor02.json` passed with no failure signals, and the
linked runtime DLL timestamp advanced. The second PIE attempt is recorded separately below.

`editor02.log` and `runtime02-active-ui.json` exposed the underlying screen lifetime defect:
all three remote clients still had an activated, visible `WBP_BW_HeroSelect` during gameplay.
The corrected menu-layer check properly suppressed their death HUD and shop access. The host
completed its countdown reconstruction and shop open/close. The 980-sample second run remains
a failure, not an accepted partial multiplayer result.

The selection widget now reconciles replicated state in its existing active-screen timer:
when its own PlayerState is locked, its controller has a pawn, and the phase is Playing or
PostRound, it dismisses through the controller's existing cleanup path. Shared Prematch
selection and unlocked late arrivals remain eligible to stay open. This handles a late
show/activation following a one-shot phase hide request. The current run verifies the locked
gameplay case; packaged late-join and rematch regressions remain required.

`compile-editor03.json` and `runtime03-verification.json` pass: 73 checks, nine negative
controls, 984 samples across four distinct PIE worlds and owners. Each owner displayed the
configured server countdown through two real deaths, reconstructed during the first countdown,
and opened/closed the production shop during the second. The countdown hid behind that menu
and resumed with the same deadline. PostRound and round-two recovery cleared the schedule.
All three remote selection screens logged dismissal after their gameplay pawn arrived.

The verifier rejects missing worlds, duplicate widgets, wrong displayed seconds, focusable
HUDs, unfinished fixtures, incorrect deadlines, stale PostRound state, missing reconstruction,
and a countdown that remains visible behind the shop. Timings are compared with the configured
delay logged by the production fixture, with explicit clock and sample tolerances in
`verify-hud.py`; its accepted copy is `verify-hud03-final.py`.

`cleanup03.json` confirms all three owned editors exited, the nine tested source-file hashes
are unchanged, and the original listen-server/four-client/one-process settings remain. The
successful editor log contains two Pixel Streaming target-window startup errors, no other
error lines, and no critical errors; `runtime03-error-review.json` retains their exact text.
They remain actionable for rendered/UI acceptance. This scoped widget-state test does not
claim a clean visual channel.

## Scope and next proof

### Candidate10 packaged countdown

`LAN-20260926-10` adds the current UI and a development-only packaged client observer.
`compile-game01.json` passed, UAT completed in 100.97 seconds, and all 131 staged file hashes
matched before and after the run. The game executable SHA-256 is
`6FD8A2E5CC6C1919DAD7B5532F96C4686B90B390294B9EB02219A0528393E670`.
The frozen changed-input manifest records 185 files and no build input newer than the compile.

`packaged-countdown01-result.json` and `packaged01-verification.json` pass the four-process
countdown scope at 200 ms packet lag: 76 checks, 13 negative controls, 674 local HUD samples
(193 host; 151, 166, and 164 clients). Every human dies twice, reconstructs its own HUD during
the first death, and opens/closes its own production shop during the second. The observer
requires the same server deadline before and after the shop, correct displayed time, no focus
capture, round-end cancellation and round-two recovery. The three clients exit with code 0;
the host remains until they finish, then receives owned fixture termination (exit -1).
`packaged01-flow.json` records zero errors, unknown errors and critical lines across all four
logs. `packaged01-cleanup.json` confirms no owned game process remains.

The first verifier required seeing both `2` and `1` during the shop-covered death. Two peers
instead correctly showed `3`, hid behind the shop, and resumed at `1`. Their samples retain the
same deadline and agree with server time. Preserve `packaged01-initial-verification.json` and
`verify-packaged-hud10-initial.py`. The accepted `verify-packaged-hud10-final.py` requires a
visible countdown before and after the menu, decreasing time and the unchanged deadline.
Its added negative control rejects a countdown that never resumes. The first unobscured
death still requires the `2` and `1` values. No build, package or runtime retry was needed
for this verifier correction.

The same frozen candidate passes the separate browser/results/return/rehost/rediscovery
regression in `frontend-regression-brief.json`: four passes, six LAN discoveries, and 80 roster
rows. `frontend-uniqueness.json` verifies 16 team groups across both matches and rejects three
negative controls. Each team contains four different heroes. This fixture uses shortened rules
and can complete a round through production `EndRound`; it does not establish natural full-match
acceptance. It verifies the results-screen return action and browser-entry joins through their
production handlers, without physical input.

The four browser logs contain zero errors and 343/30/29/28 warnings respectively; warnings
remain in the saved inventory. All three clients exited 0, the host received owned teardown,
and `frontend-cleanup.json` confirms no owned process remains. All 131 artifact hashes still
match. This flow does not directly count countdown widgets across travel or test an unlocked
gameplay late arrival, so those UI lifetime assertions remain separate.

Through candidate10, the gate used four compiles (three editor, one game), three PIE lifecycles,
one package, and two four-process packaged runs after one read-only commandlet. Each editor rebuild follows a
different observed UI-lifetime diagnosis. Candidate `LAN-20260926-09` remains the
frozen prior respawn-lifecycle package and contains none of this new countdown implementation.

Pending: actual visual layout and physical input, original delay progression, second-PC LAN,
and final integrated multiplayer acceptance. Neither headless inspection nor widget-property observations
prove visual polish. Changes to the schedule, round lifecycle, local UI ownership, menu layers,
or widget skin invalidate the corresponding evidence.

### Direct widget lifetime follow-up

`BwayUIScreenLifecycleAudit.cpp` adds a development-only, packaged observer. Passive mode
records the local controller, PlayerState, world, attached countdown count, stale attached
widgets, visibility, focusability, and activated selection count. It keeps only a weak
GameInstance reference and resolves the current world on each tick. Its lifetime is bounded
to 480 seconds; the owned process runner performs final cleanup.

For the direct-map late-join fixture, baseline mode submits a valid hero through the existing
server RPC. Late-join mode first requires an unlocked, visible selection screen during Playing
for three uninterrupted seconds. It then submits the choice and observes selection dismissal,
pawn arrival, and all four human states. This is semantic selection evidence, not physical input.
The direct-map fixture has no bots; the separate browser fixture retains four humans/four bots.

Candidate11's two setup attempts remain failures. The first used `SkipHeroSelection=1`, which
gave the host a pawn without a lock or selection screen. Removing that shortcut for attempt two
still did not create an active host selection screen. The original observer incorrectly required
a screen even for baseline semantic selection, so both runs stopped before the fourth client.
Neither candidate11 attempt proves late join. The corrected fixture allows baseline selection without that
screen while retaining the three-second visibility requirement for the late client. Candidate12
contains that development-only correction; production UI and gameplay are unchanged.

`verify-screen-lifecycle.py` checks captured UI samples independently of the transport runner.
The expected result is one attached countdown per current Breakaway controller, none on a
non-Breakaway controller, no stale attached widget or focus capture, hidden UI without an
eligible schedule, and completed late selection or repeated travel.

Candidate12 passes both direct UI scopes at 200 ms:

- `screen12-latejoin-verification.json`: 483 samples and eight negative controls. Client3 kept
  its unlocked selection screen for 3.10 seconds before selecting and receiving a pawn.
- `screen12-travel-verification.json`: 2,653 samples and nine negative controls through two
  staging worlds, two gameplay worlds, results, connected frontend return, disconnect, and
  fresh browser rejoin. The independent browser verifier also passes four peers, six discoveries,
  and 80 roster rows; `screen12-travel-uniqueness.json` passes 16 team groups and three controls.

Both runs contain zero error and critical lines. Late-join warning counts are 70/17/17/17;
travel warning counts are 351/30/27/27. Warnings remain in the saved evidence. All six client
processes exited 0; owned host teardown is recorded separately. `screen12-cleanup.json` confirms
zero remaining owned processes. The browser fixture uses shortened rules, so its first-match
results and second playable match do not replace final full-match acceptance.

The first travel verifier incorrectly required a three-second idle interval in the host's
initial frontend. Hosting starts immediately: that world has one valid snapshot, while the
returned frontend has 83 samples over 25.88 seconds. Preserve
`screen12-travel-initial-verification.json` and `verify-screen-lifecycle12-initial.py`.
The accepted `verify-screen-lifecycle12-final.py` requires the initial frontend snapshot,
a distinct stable post-game frontend, and both stable staging/gameplay worlds. Added negative
controls remove the returned frontend or second gameplay world. No runtime rerun was needed.
The late-join result retains the initial verifier's hash; its late-join checks are unchanged.

All 131 candidate12 staged hashes match before and after both runs. Its executable SHA-256 is
`03D81F86EB36116C82F290DA19CD93242500A6F4A68A46177504BF70D673441C`.
The complete countdown gate now totals six compiles (three editor, three game), three packages,
three PIE attempts, and six packaged attempts. Two packaged attempts failed during fixture
setup as described above. Candidate12 packaging recovered from one internal Zen connection
retry and completed in 105.66 seconds; no external package retry was launched.

Next, audit the unfinished damage-contribution/assist-credit path in
`BwayCharacterWithAbilities::OnDeathStarted` against the match-stat and reward requirements.
Keep original respawn-delay progression explicitly unresolved until its tuning and reset
boundary have reference evidence. Physical input, visual quality, second-PC and final integrated
acceptance remain required.

During compile01, UBT's JSON-lines sidecar temporarily overwrote the running compile record
because both used the same base filename. The existing build completed successfully and the
final record was published correctly. Compile02 uses `compile-editor02-ubt.log` and
`compile-editor02.json`; the build guide now documents distinct names. No duplicate build was
started to compensate for the temporary record format.
