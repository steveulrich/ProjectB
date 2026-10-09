# LAN front-end verification

Updated September 23, 2026. Candidate `LAN-20260923-05` passes the packaged four-player
browser, connected return, and rehost gate. Candidate `LAN-20260923-06` also passes full
client disconnect and fresh browser rediscovery/rejoin. Both use confirmed 200 ms packet
lag. Physical navigation, visual polish, and second-PC LAN remain separate gaps.

## Disconnected rediscovery

Candidate23-06 extends the development fixture through each client's production
`ReturnToMainMenu` path. All three clients recover an active standalone frontend,
open new browser searches, and join through displayed entries. Their initial advertised
session is `7F609A824BD467A2C76B039ED7E5C9F9`; after rehosting, every client discovers
`B115206F41CA7035DF508FBD5DAB7063`. Weak search references and distinct session IDs
prevent a cached first-session result from passing the rediscovery assertion.

The run passes six browser discoveries/entry joins, three standalone recoveries,
11 restored missing-PlayerState layout checks, and 80 roster rows across both matches
and the connected frontend. All three clients exit 0 after the second eight-player
match; owned host teardown is separate. The first match uses the fixture's production
`EndRound` call, so this proves match/session flow rather than natural human combat.
There are no runtime errors. Warnings remain: 359 host and 41 per client.

All 29 copied-evidence controls are rejected, including missing standalone recovery,
stale searches, reused session identity, missing second joins, incorrect teams, lost host
rules, missing latency, and bad artifact identity. Evidence is under
`Saved/Logs/lan-reconnect-20260923/`: `runtime06-lag-brief.json`,
`packaged-LAN-20260923-06-browser-lag-result.json`,
`verifier-controls-LAN-20260923-06-lag.json`, and the frozen candidate06 contract.
All 131 staged hashes match; the executable SHA-256 is
`26708A002FA5AF701558F629FE25A58B2E8B25A00F2188BBF0DC4C6D1E7433DD`.
This gate required one build, one package, and one runtime attempt, with no editor launch.

`Scripts/Test/Run-PackagedLANBrowserProbe.ps1` now parameterizes the existing launcher
for future connected or rediscovery gates. Its command/path routing, script syntax, and
existing-result refusal are validated; the accepted run above used the preserved
candidate-specific runner. Do not treat the shared script's validation-only output as
new gameplay evidence.

## Current packaged browser gate

The accepted `browser-lag` run starts all four rendered processes on the frontend.
Three clients use actual advertised playlist entries to join. Both matches have matching
eight-player rosters: four humans, four bots, two balanced teams, and valid locked heroes.
All four show eight-column results and return to active menus while retaining the four
human connections. Rehosting reaches a second playable match. All three clients exit 0;
the runner separately closes its owned host. The first match reaches results without
the fixture's optional `EndRound` call; this is not evidence of human combat input.

Eight synchronous missing-PlayerState checks pass at startup and connected return.
The checker validates each match's carried host rules separately and rejects all 23
copied-evidence controls. It records 80 roster rows, three discoveries, four final passes,
and zero errors in all four logs. Warnings remain: 354 host and 41 per client, including
initialization diagnostics and invalid-listener cleanup warnings that need separate work.
All 131 staged hashes match the compiled artifact, SHA-256
`46D0F6E9B3DA4133945EC5D6CEB8461579814FDF0027DD7517D28BE6533DA5F2`.

Evidence is under `Saved/Logs/lan-frontend-20260923/`: `runtime05-lag-brief.json`,
`packaged-LAN-20260923-05-browser-lag-result.json`,
`verifier-controls-LAN-20260923-05-lag.json`, and the frozen candidate05 contract.
The gate used eight builds, five packages, five runtime attempts, one inspection editor,
and two automation attempts. The latency retry reused the package. Failed attempts and
the unused candidate04 package are retained with their distinct retry reasons below.

### Attempt history

Candidate `LAN-20260923-01` starts one host and three clients on the actual front-end map.
Each client pushes `W_SessionBrowserScreen` onto the local menu layer and invokes the
displayed entry's `JoinSession` event. The inspected widget graphs are preserved in
`Saved/Logs/lan-frontend-20260923/{browser,entry}-graph.json`.

All three rendered clients discovered the real LAN advertisement, loaded the playlist
entry, and joined staging. The listings reported capacity eight and decreasing open
connections. No client used a direct-IP launch. The gate then timed out on its gameplay
roster predicate; its initial diagnostics did not identify the rejecting field. No full
roster/return/rehost pass is claimed. The unattended match reached natural 0–3 results
before the timeout. All owned processes were cleaned up.

The same run exposed lost host settings. The staging URL contained `PointsToWin=1`,
`RoundDuration=180`, and other requested rules. The absolute destination URL omitted
them, and gameplay used the defaults: three wins and 90-second rounds.
`TryTravelToPostHeroSelectionMap` rebuilt only the map, experience, listen, and selection
flags. The source fix copies an explicit list of host-authored match-rule keys while
preserving destination overrides and excluding staging, hero-input, and identity options.
The frontend suite now passes all four tests, including
`Breakaway.FrontEnd.StagingMatchRules`. Its first run caught full-URL versus option-fragment
parsing in the new helper; the corrected helper preserves explicit destination rules and
does not append duplicates. Candidate23-02 confirms gameplay receives one win, 180-second
rounds, four bots, and the requested phase timing.

Candidate23-02 also identifies the roster timeout: the fixture used Lyra team IDs as if they
were canonical Breakaway indices. Diagnostics show eight valid, locked heroes with pawns,
four humans, and four bots. `BwayTeamBridge` intentionally maps indices 0/1 to IDs 1/2.
The candidate23-03 fixture checks canonical membership, balance, and that mapping on every
peer. This changes the test, not gameplay team assignment. Complete return/rehost is still
pending; preserve both failed attempts and their diagnostics.

Candidate23-03 passes the first match's roster and eight-column results on all four
processes. On connected return, clients 1 and 2 remain on `Try Show Main Screen`;
only the host and client 3 display the menu and verify the second match. The runner
fails and cleans up its remaining processes. Keep this failed result in
`Saved/Logs/lan-frontend-20260923/runtime03-brief.json`.

The primary layout helper used `GetPrimaryPlayerController(false)`, which requires
the controller's replicated PlayerState even when the local player's root layout
already exists. The one-shot frontend flow cannot recover from that null lookup.
The repair resolves the primary local player's layout directly. Candidate23-05
will test the full flow and explicitly check layout resolution without PlayerState
at startup and return on each process. That development-only check restores the
reference synchronously before returning; it does not tick or replicate the change.

Candidate23-04 compiled and packaged successfully but was not launched. Source review
caught an overly strict regression precondition: the old primary-controller lookup can
return a remote controller on the listen host. Candidate23-05 checks that the old lookup
cannot resolve a local layout, whether it returns null or a remote controller. The
production repair is unchanged. This correction required one additional build and package.

The first candidate23-05 run completed browser joins, both eight-player gameplay
rosters, eight-column results, all four connected frontend menus, and three normal
client exits. Its eight missing-PlayerState layout checks passed. The checker correctly
withheld acceptance because the requested 200 ms latency was not observed. Startup
`ExecCmds` ran before a network driver existed. The retry uses `-PktLag=200`, which
`UNetDriver::InitPacketSimulationSettings` reads when each driver initializes; require
the emitted `LogNet: PktLag set to 200` marker. The same frozen package is reused with
separate `browser-lag` logs and a result record; no rebuild or recook is needed.

The first editor build for the regression was blocked by the known BF-046 bridge JSON-key
conversion regression at two FoliageHandlers sites and one LevelHandlers site. Restoring
explicit string construction made the next build pass. The runtime DLL had already relinked
during the first build; its freshness record preserves that sequence. No unrelated bridge
behavior was changed.

Candidate23-01 evidence: `Saved/Logs/lan-frontend-20260923/runtime01-brief.json`,
`packaged-LAN-20260923-01-browser-result.json`, and the frozen candidate contract.
Its 131 artifact hashes passed. Cost: one LyraGame build, one package, one read-only
editor inspection, and one four-process attempt. Candidate23-02 adds roster diagnostics
and rule preservation; do not repeat the unchanged failed fixture. Physical navigation,
visual quality, disconnected rediscovery, second-PC LAN, and final end-to-end acceptance
remain separate requirements.

## Observed gap and change

The existing W_SessionBrowserScreen calls CommonSessionSubsystem.CreateOnlineSearchSessionRequest. That factory hardcoded Online, while the custom matchmaking subsystem defaulted to Offline. A dedicated LAN playlist already exists at /Game/System/DA_Playlist_CTR_LAN, so the browser must be able to discover LAN-hosted sessions.

CommonSessionSubsystem now exposes DefaultOnlineMode in Engine config, with its general default still Online. ProjectB sets it to LAN. Factory-created host and search requests use that mode, with lobby, lobby voice, and presence features disabled for LAN. Custom matchmaking defaults to LAN. Quick play copies its selected host mode to the search request and applies online-only flags consistently. Explicit playlist hosting modes still override factory defaults.

## Verified evidence

- LyraEditor Win64 Development succeeded (exit 0): Saved/Logs/codex-lan-defaults-build-retry.log.
- Editor restarted successfully and the MCP bridge reconnected.
- In the restarted editor, created host and search requests from the loaded CommonSessionSubsystem defaults. Assertions passed for both OnlineMode values being LAN, host/search lobbies being disabled, host presence being disabled, and LyraMatchmakingSubsystem.DevHostOnlineMode being LAN. The PASS output is recorded in Saved/Logs/ProjectB.log.
- This test checks actual reflected request objects and loaded project configuration; it does not create or discover a network session.

## Remaining acceptance gates

1. From the front end, host the dedicated LAN playlist. Verify a listen server is created and its advertisement is visible from a separate client process.
2. Use the actual session browser to find and join from three clients. Verify the listing, connection result, loading state, error handling, and back navigation.
3. Verify hero-selection staging, team assignment, four human players plus bots filling to eight, and synchronized travel into the match.
4. Finish a match, show results, and return all clients to the correct front-end state. Host and join another session without stale listings or session-state errors.
5. Repeat from a packaged Development build across LAN machines. A direct IP connection or PIE auto-connect is not a substitute for browser discovery/join.

Other playlist assets still have explicit Online modes. A separate UI/content pass must establish which playlists are exposed by the slice and test each supported entry path. The mock matchmaking timer is not a completed matchmaking implementation.

## Live hosting attempt

September 5, 2026: started a single standalone PIE instance on the Lyra front-end map and invoked the existing LAN playlist's CreateHostingRequest followed by CommonSessionSubsystem.HostSession with the actual local player. The request selected L_BW_HeroSelect_Staging, LAN mode, and Dorado as the target match map. The session creation delegate reported success and GameNetDriver listened on port 7777. A separate `-game` client connected by direct address; the server logged Join succeeded and the client loaded staging. This verifies hosting and direct connection, not a UI click or LAN discovery.

Two additional clients timed out after 20 seconds. The editor subsequently crashed at GameThreadExecutor.cpp:56. The crash identified a bridge callback retaining stack references after a request timeout; the executor now shares completion state and owns captured inputs through callback completion. This run does not pass four-player or staging-to-match acceptance.

The first travel also generated an old-world GC ensure whose reference chain pointed to Python probe globals. Those wrappers were released; future runtime probes must be function-scoped. This is test-harness evidence, not an established gameplay defect.

Evidence: Saved/Logs/codex-lan-host-crash-20260905.log, Saved/Logs/codex-lan-client1.log through codex-lan-client3.log, and Saved/Crashes/UECC-Windows-70CE9E194057C19922333688FC4822CD_0002. All three test game processes were stopped after the failed run. No user content was dirty before testing.

## Bridge recovery verification

The timeout-lifetime fix compiled successfully (Saved/Logs/codex-bridge-timeout-build-retry.log). After editor restart, ProjectB.MCP.QueuedTimeoutLifetime passed at 23:43:18 UTC on September 5. The test expires a worker request while the game thread is occupied, releases the caller's stack, then verifies that the delayed queued handler is skipped without a crash.

The automation runner initially waited because background editor throttling limited it to 3 FPS. Temporarily disabling EditorPerformanceSettings.bThrottleCPUWhenNotForeground allowed the test to run. The prior throttle setting was restored afterward, along with four clients and PIE_ListenServer. RunUnderOneProcess was observed true and was left unchanged. Future separate-client PIE-host tests should temporarily disable background throttling and limit rendering load deliberately. The earlier connection timeouts are not conclusively attributed to that setting; retry is required.

## Four-human staging retry and travel fix

With background throttling disabled and separate clients launched sequentially, the host confirmed four PlayerStates (one PIE host and three separate game processes). Each client connected by direct address. This passes the four-human connection check, not browser discovery or complete match flow. Evidence: Saved/Logs/codex-lan-four-player-staging-before-fix.log and codex-lan-retry-client1.log through client3.log.

The retry exposed two staging defects: inherited round management advanced into Playing during hero selection, and completed selection failed to travel. A runtime probe showed the shared staging helper returned true while Unreal GameplayStatics.HasOption returned false for the same GameMode OptionsString. The merge helper wrote ampersand separators, incompatible with Unreal option readers. It now emits question-mark separators. Round management skips initialization on staging and rejects round lifecycle entry there, including legacy Blueprint callbacks.

The changes compiled successfully in Saved/Logs/codex-staging-travel-build.log. Breakaway.FrontEnd.GameplayUrlEngineCompatibility checks both accepted input formats, existing-value precedence, and engine readability of staging and destination options. Runtime travel verification follows; compilation alone does not close that gate. Test clients were stopped and prior PIE settings restored before rebuilding.

The URL compatibility test passed at 23:54:11 UTC. A runtime-only retry subsequently reached Dorado with the host and one remote player. Destination initialization nevertheless imported HeroSelectStaging from the previous map LastURL, leaving match flow disabled. URL gathering now prefers World.URL and accepts only its own matching-map LastURL; cross-world PIE fallbacks were removed.

Running automation during the earlier live staging test ended PIE and exposed EndPlay calling successful selection completion, including ServerTravel. EndPlay now only clears phase state and removes its ready delegate. Keep automation and runtime sessions separate. The later runtime retry logged an old temporary-world GC ensure referencing engine subsystems; it followed that interrupted run and needs a clean-editor retest before attribution. Evidence: Saved/Logs/codex-staging-travel-before-url-scope-fix.log and codex-lan-travel-retry-client1.log.

## Clean-editor verification after lifecycle and URL fixes

LyraEditor compiled successfully (Saved/Logs/codex-staging-lifecycle-build.log). In the clean restarted editor, LAN hosting reached staging; the 60-second selection timeout selected and locked fallback heroes, then seamlessly carried the host and one separate client to Dorado. Destination OptionsString contained the match experience, listen, SkipHeroSelection, and SeamlessTravel, with no HeroSelectStaging. Round 1 started. The server confirmed eight PlayerStates: two humans and six bots, all hero-locked. The remote log recorded match experience load completion and RoundActive replication. No old-world GC ensure occurred in this clean run. Evidence: Saved/Logs/codex-lan-clean-travel-host.log and codex-lan-clean-travel-client1.log.

A second staging session was stopped while selection remained open. The match-travel log count stayed at one (the previous successful match); stopping staging produced no new travel request or teardown error. The separate test client was stopped. The original DevMap, four-client listen PIE settings, and background throttle were restored; no content or map packages were dirty.

These checks establish direct-IP connection and two-human staging-to-active-match flow. Four-human staging connectivity was verified in the earlier retry. They do not establish four-human match travel, manual hero selection, LAN browser discovery, full match completion/results/return, packaged LAN operation, or final-alpha gameplay parity.

## Standalone host and actual browser discovery/join

A development-only bway.Test.HostLAN console command now hosts the existing LAN playlist after a 10-second front-end startup delay. It uses the actual CommonSessionSubsystem and does not manufacture search results. Launch a separate UnrealEditor -game instance on /Game/System/FrontEnd/Maps/L_LyraFrontEnd with -ExecCmds="t.MaxFPS 30,bway.Test.HostLAN". The command is excluded from Shipping.

The initial standalone host failed because the map ID was unavailable to Asset Manager. Scanning /BreakawayCore/Maps before playlist request creation fixed that observed failure. The fallback staging map ID also now uses the full package path required by UWorld. Build: Saved/Logs/codex-lan-map-scan-build.log; successful standalone advertisement: codex-lan-browser-host-retry.log.

The actual W_SessionBrowserScreen was pushed onto the local player's UI.Layer.Menu using CommonUIExtensions. Its activation issued a LAN search and populated one result and one visible list entry. The entry initially failed to load its advertised playlist and joining stalled: BwayUserFacingExperienceDefinition inherited a different primary asset type from the type reconstructed by Lyra UI, and /Game/System was outside the playlist scan paths. The subclass now returns the canonical LyraUserFacingExperienceDefinition type. DefaultGame.ini scans the actual directory, and all four DA_Playlist_CTR assets were resaved to update registry tags. Build: codex-playlist-identity-build.log.

After these changes, the browser again populated one real LAN host. Invoking the displayed entry's JoinSession event (the same event wired to its SessionButton) successfully connected the PIE client to the separate host and loaded hero staging. This verifies the browser's discovery, entry-data loading, and join code path, but not physical mouse/controller navigation or its visual polish. Evidence: Saved/Logs/codex-lan-browser-fixed-client.log and codex-lan-browser-fixed-host.log.

The standalone host then reported zero heroes via Asset Manager and could not assign a fallback at selection timeout. Runtime hero discovery remains a blocker to this host's full match flow. Direct Python invocations of hero RPC methods did not establish server-side execution and are not replication evidence. All test processes were stopped, prior editor settings and DevMap restored, and no asset/map packages remained dirty.

## Standalone hero discovery and team restoration

The standalone hero scan is now refreshed at UBwayHeroRegistry initialization. Configured paths come from AssetManagerSettings, not the runtime type-info snapshot. Direct Asset Registry scanning of mounted directories bypasses AssetManager's cached pre-mount scan paths; the subsequent primary-asset scan registers all four heroes. Evidence: Saved/Logs/codex-lan-heroes-refresh-host.log reports four assets, successful hosting, selection timeout completion, travel to Dorado, and Round 1 startup. The browser-joined client received eight locked hero selections (codex-lan-heroes-refresh-client.log).

That run exposed human team index -1 after travel. Team assignment existed in PostLogin, which seamless arrivals bypass. BwayPlayerState now copies its team ID, and HandleStartingNewPlayer registers carried membership before spawn deferrals. AssignControllerToTeam preserves valid carried team IDs. The rebuilt run verified two humans on opposing teams before and after travel, and a replicated roster of eight players with team counts [4,4]. Round 1 progressed naturally to Round 2. Evidence: Saved/Logs/codex-lan-team-travel-host.log and codex-lan-team-travel-client.log. Build: codex-seamless-team-build.log.

At this historical checkpoint, the manager asset explicitly allowed duplicate heroes. This was
resolved on September 23: the saved override is false, gameplay availability reads the live
roster, and bot assignment waits for travelling human teams/heroes. Candidate `LAN-20260923-09`
passes remote rejection/disconnect checks and two normal-flow gameplay rosters with unique
heroes per team. See [hero selection acceptance](../../../../AI_Planning/HERO_SELECTION_ACCEPTANCE.md).

The same runtime also exposed a roughly 50-second difference between client local world time and synchronized server time. RoundStartTime is server-authored, but the countdown used local World time. GetRoundTimeRemaining now uses GameState.GetServerWorldTimeSeconds. Verification of the rebuilt timer follows. These runs do not yet establish complete match results/return or packaged four-human LAN acceptance.

## Synchronized countdown verification

Build Saved/Logs/codex-server-clock-build.log succeeded. A clean separate host progressed through three rounds; the actual LAN browser entry joined a PIE client directly into the ongoing Dorado match. At 00:45:21 UTC on September 6, the client reported Round 3 active with 77 seconds remaining, matching the host round start at 00:45:08. A second probe reported 47 seconds remaining about 30 seconds later. Client local time was approximately 258 seconds behind synchronized server time, so this run exercises the corrected clock basis rather than matching clocks by coincidence. Evidence: codex-lan-clock-host.log and codex-lan-clock-client.log.

The first editor attempt crashed in Unreal Core InheritedContext/DevHttp/DDC worker code during PIE startup. Its log is preserved as codex-clock-editor-crash.log. Restarting the editor allowed this browser and countdown verification; the engine crash is not resolved by the gameplay changes.

The unattended rounds repeatedly expired with the relic on midfield and no winner; this run does not validate scoring through match completion. PIE was stopped, the separate test host was stopped, and the editor was restored to DevMap, four-client listen PIE, and background throttling enabled. Dirty content and map package counts were both zero. Full scoring/results/return and four-human travel remain acceptance gates.

## Results persistence and host return

A PIE listen host and separate client reached a 3-0 result through three authority-injected EndRound calls, with normal PostRound transitions between them. Both logged the same final score and total rounds. This is a controlled results-flow fixture, not proof of relic scoring or player-driven hero selection. Evidence: codex-results-before-fix-host.log and codex-results-client.log.

The run exposed BW_Phase_PostMatch automatically restarting the map ten seconds after results. Its Gameplay Ability Graph retained the legacy Delay/NewGameTime, delayed effect, and PlayNextGame chain, beside a TODO explicitly requesting postgame screens instead. Removed that timed chain while preserving the phase's initial global effect. The Blueprint compiled and saved successfully; no native source changed in this pass.

A new single-host fixture set PointsToWin=1 at runtime and injected one round win. PostMatch began at 00:53:12 UTC on September 6 and still held the visible eight-column breakdown at 00:53:30, beyond the former restart deadline. Invoking the breakdown's RequestReturnToLobby action then loaded L_LyraFrontEnd with B_LyraFrontEnd_Experience. A subsequent probe confirmed W_LyraFrontEnd_C_0 active and visible. Evidence: codex-postmatch-restart-fixed-host.log. This proves results persistence and the local host action path; remote return, physical input navigation, rehosting, and four-human completion remain unverified.

New follow-up defects from this fixture: a human joining after bot fill briefly produced a nine-player roster; the results controller attempts UIOnly focus on a non-focusable orchestration widget; the visible CommonUI breakdown is not activated. These require fixes before the UI is considered polished. Test processes were stopped, DevMap/four-client listen PIE/background throttle restored, and dirty package counts were zero. Prevention catalog BF-059 records the legacy restart failure.

## Results activation and focus

The results controller previously focused its collapsed orchestration widget after creating the visible child screens. It now adds the orchestrator to the viewport and establishes UI-only input before applying results. The interstitial and breakdown activate after data is applied, take local-user focus, and deactivate during removal. The breakdown binds its existing Btn_ReturnToLobby control as its desired focus target. This keeps authoritative results in the existing gameplay/controller path and presentation/focus in the local UMG + CommonUI screens; no MVVM or UI stack replacement was introduced.

Build codex-results-focus-build.log passed. After editor restart, a one-round authority-injected fixture reached PostMatch. Runtime assertions confirmed the breakdown active, desired target Btn_ReturnToLobby, and local-user focus inside that CommonButton's internal widget (HasUserFocusedDescendants=true). CommonUI logged Focused desired target Btn_ReturnToLobby; the prior Non-Focusable widget input error was absent. Invoking RequestReturnToLobby removed the breakdown and loaded the frontend, whose active W_LyraFrontEnd held local-user focus. Evidence: Saved/Logs/codex-results-focus-verified.log. Physical keyboard/gamepad navigation, resolution/accessibility coverage, and remote results interaction remain unverified. BF-060 records root cause and prevention. PIE was stopped and four-client listen settings/background throttling restored.

## Late join replacing a bot

The earlier nine-player fixture occurred because bot counts were only reconciled at experience load and round start. AssignPlayerToTeam now asks the bot component to trim existing fillers first; trimming stops AI and removes team membership before pawn/controller destruction. It does not create new bots during login or alter explicit bot-count overrides.

The first rebuilt run maintained eight PlayerStates but revealed 5-3 teams: a fresh Lyra player's valid generic team ID was mistaken for carried travel state. BwayPlayerState now marks copied team identity in CopyProperties; only that provenance allows assignment to preserve the generic ID. Fresh players use the smaller team. Evidence for the failed intermediate case: codex-latejoin-unbalanced-host.log. Final build: codex-latejoin-team-build.log (passed).

The final retry first asserted eight players on teams [4,4] before launching the separate client. After the real connection completed, runtime assertions confirmed two human controllers, eight total PlayerStates, team membership counts [4,4], and round number 0. Thus no round reset hid the late-join defect. Evidence: codex-latejoin-balanced-host.log and codex-latejoin-balanced-client.log. BF-061 records both causes and prevention. Mid-match joins, disconnect refill, and a fresh seamless-travel regression pass remain outstanding. Test processes were stopped and four-client listen PIE/background throttle restored.
