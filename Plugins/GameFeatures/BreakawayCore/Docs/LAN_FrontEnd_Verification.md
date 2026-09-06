# LAN front-end verification

September 5, 2026. Configuration checks passed; separate-process discovery and full front-end flow remain pending.

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
