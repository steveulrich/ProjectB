# Packaged LAN acceptance

## Current candidate

- Project base: `e405d746` (September 6, 2026), plus the hero feature type registration in `DefaultGame.ini` and the uncommitted `Server_RequestReturnToFrontEnd` PostMatch guard. The exact source delta is saved in `Saved/Logs/codex-packaged-LAN-20260906-04-project.patch`; this candidate is not a clean-commit release.
- Engine: local UE 5.8.1 source tree, `E:/Github/UnrealEngine`, HEAD `71fe36aac5a8df5ccd66c763ffc902b29b6a9c43`. Local engine modifications must also be captured before accepting the artifact; HEAD alone does not establish reproducibility.
- Platform/configuration: Win64 Development, `LyraGame`, listen server and clients.
- Pipeline: AutomationTool BuildCookRun, build, cook by the book, stage, Pak.
- Stage: `Saved/StagedBuilds/LAN-20260906-04/Windows`; launch `LyraGame.exe`. The actual game binary is `ProjectB/Binaries/Win64/LyraGame.exe`. No release archive or deployment yet.
- Logs: `Saved/Logs/codex-lan-package-20260906-feature-types-final.log` and the referenced UBT log.
- SHA-256 inventory: `Saved/Logs/codex-packaged-LAN-20260906-04-sha256.csv`, covering every staged file, including binaries, Pak/IoStore files, symbols, configuration, and manifests. The earlier `-02` inventory is retained separately.
- Existing cook roots: frontend, DevMap, Dorado, and the configured BreakawayCore content directory. No new broad cook inclusion was added.
- Status: build, cook, and stage passed with AutomationTool exit code 0. Candidate `-04` passed cold-load hero registration, activation, manual selection/lock, and travel checks. Two-process frontend host/discover/join and shop smoke checks passed on candidate `-02`; they have not been repeated on `-04`. Full multiplayer acceptance remains open.

The first cook reported a missing `Alona` asset domain. Engine `AssetReferencingDomains.cpp` derives plugin domains from `IPlugin::GetName`; the registered plugin is `Hero_Alona`. The corrected identifier passed the fresh cook. BF-069 records prevention. Engine tracked changes and status were captured in `Saved/Logs/codex-package-engine-tracked.patch` and `codex-package-engine-status.txt`; untracked engine content is not included in that patch.

Command issued with the editor closed:

```powershell
& 'E:/Github/UnrealEngine/Engine/Build/BatchFiles/RunUAT.bat' `
  '-ScriptsForProject=E:/Unreal Projects/ProjectB/ProjectB.uproject' `
  BuildCookRun '-project=E:/Unreal Projects/ProjectB/ProjectB.uproject' `
  -target=LyraGame -platform=Win64 -clientconfig=Development `
  -build -cook -stage -pak -utf8output -unattended `
  '-stagingdirectory=E:/Unreal Projects/ProjectB/Saved/StagedBuilds/LAN-20260906-04'
```

## Cook repair evidence (September 6)

The first cook exited with code 25 after the standalone build passed. Its terminal errors were the stale Alona domain, incompatible TopDownArena movement override, and failed relic redirector import. The domain correction is committed separately. A full asset-registry referencer scan established that `B_Bway_Ball_Interactable` had no referencers and that `BP_Bway_Relic` was referenced only by that obsolete redirector; both packages contained only redirectors leading to a deleted asset. Those two dead packages were removed. The live relic remains `/BreakawayCore/GameModes/BallMode/Relic/BP_BW_RelicActor`, referenced by game state, spawn data, and the grant pad.

The shared hero parent now requires BwayCharacterMovementComponent. TopDownArenaMovementComponent derives from that class and declares its BreakawayCore module/plugin dependency, preserving its existing speed override. The editor build passed (`Saved/Logs/codex-cook-asset-repair-build.log`). After restart, B_Hero_Arena compiled, its movement default object had the correct class, the live relic loaded, and the removed redirectors were absent from the registry (`Saved/Logs/codex-cook-assets-editor-verified.log`). BF-072 records prevention. The subsequent fresh cook and stage passed; content warnings still require review.

## Packaged smoke evidence (September 6, 04:22–04:32 UTC)

The editor was closed. Candidate `-02` tests used two separate packaged game processes on the source machine, controlled through visible menus with Computer Use.

- Host: **Play Lyra → Start a game → Network: LAN → Local / Listen server + bots** created a session and opened hero selection.
- Client: **Play Lyra → Browse → Refresh search** discovered that session with 1/8 players and 63 ms ping. Selecting its row joined hero selection. Both peers travelled into Dorado after selection timed out. This does not verify manual hero locking; the attempted hero click overlapped timeout.
- The host log reports a final bot target and count of six with the two humans. Team balance still needs an explicit roster check.
- Client **B** opened the native upgrade shop. At 59 gold, the 75-gold purchase was disabled. In round two, a physical button click produced **Purchasing…**, followed by **Purchase complete**, owned attack rank 1/4, and upgrades 1/4. Passive gold continued during the observations, so screenshots alone do not establish the exact debit.
- Evidence: `Saved/Logs/codex-packaged-host2-smoke.log` and `codex-packaged-client1-smoke.log`. Earlier single-host run: `codex-packaged-frontend-smoke.log`.

Polish failures are visible: Lyra/sample and development menu entries, clipped labels, hero-selection TODO text and blank panels, duplicate neutral HUD labels, an in-match “Waiting for additional players” banner, floating hero mesh alignment, and missing Alona animation setup. Startup also records unprecached PSO hitches and invalid `BwayGameFeatureData` primary asset IDs. These are open issues, not acceptance passes.

Initial overlapping game windows exhibited button nonresponse while console input worked. Closing the first process restored the remaining menu. Relaunching the other window at a separate position allowed both menus and the shop to work. Keep windows separated for automation; the cause is not yet established as a game defect.

## Hero feature registration verification (04:38–04:39 UTC)

The hero definitions report primary type `BwayGameFeatureData`. `UAssetManager::RegisterSpecificPrimaryAsset` rejects unknown types, while project settings previously registered only `GameFeatureData`. The fix registers the existing subclass type with four explicit asset paths and the engine GameFeatureData base class. IDs and gameplay state remain unchanged; the Game Features subsystem continues to own activation and unloading.

Candidate `-04` passed a fresh build/cook/stage. A cold packaged launch with `AssetManager.DumpTypeSummary,bway.Test.HostLAN` reported exactly four feature definitions. All four hero plugins reached Active after travel, with no invalid BwayGameFeatureData ID warnings or scan conflicts. The host command is a native fixture, not frontend click evidence. Physical clicks then selected Korryn and locked the selection; logs confirm Hexweaver data on the human PlayerState and Korryn on the spawned match pawn. Evidence: `Saved/Logs/codex-packaged-feature-types-verified.log`. BF-073 records prevention.

The first scan attempt used broad hero roots and encountered ignored conflicts with unrelated primary data types; explicit paths removed those conflicts. Zen briefly disconnected during staging and the existing retry recovered; both staging attempts ultimately exited successfully.

## Hero selection follow-up (source editor, 05:14–05:22 UTC)

These changes are newer than packaged candidate `-04`. Two same-process listen-server PIE worlds reproduced two client faults: selection-active state was authority-only, and the screen cached a null PlayerState before replication arrived. A physical hero click consequently used the offline frontend route and never reached the server.

The manager now replicates its active flag. The screen reacquires its owning PlayerState while active and before requests, binds delegates uniquely, and keeps a locked client visible until the server advances the phase. After rebuilding/restarting, physical Korryn selection and lock updated both the authoritative and client PlayerStates; the client screen remained active while the host was unready. Evidence: `Saved/Logs/codex-selection-client-lock-verified.log`. Auto-lock was disabled only on the transient server component to keep the inspection window open. This test does not prove timeout behavior, staging travel, or packaged acceptance.

The selection layout now has separate hero and details columns, with obsolete level/TODO/empty panels removed and native data-driven stats text bound. UMG compiler GUID metadata was repaired, force-saved, reloaded, and verified across editor restarts. Portraits now fit a fixed-size container independently of source texture dimensions. Remaining presentation issues include sparse class-based card spacing, timer/readiness formatting, and duplicate host selection widgets.

The ability slot configuration previously deserialized as one invalid entry. Correct array entries with `TagName` now resolve all six slots and six authored ability descriptions for each of the four heroes. Final PIE physical selection/lock again passed with the full Korryn portrait and scrollable stats/ability descriptions visible. Both server and client reported Korryn locked while the unready host kept the selection phase active. Evidence: `Saved/Logs/codex-selection-final-verified.log`; no Blueprint runtime errors, Accessed None, or ensure failures appear in that final log. Native build: `Saved/Logs/codex-selection-hydration-final-build.log`, exit 0. PIE stopped and temporary client window/background-throttle settings restored afterward. Packaged ability-bar regression testing remains open because the shared slot configuration also affects match UI.

## Four-player selection ownership and roster verification (06:02–06:03 UTC)

Source-editor follow-up, still newer than packaged candidate `-04`:

- Removed duplicate viewport creation from the phase Blueprint and staging level Blueprint. The owning PlayerController now pushes the single CommonUI menu, including for joining players. The widget declares menu input mode and reacquires GameState/selection-manager references when replication arrives after creation.
- Connected active selection registration/removal to GameState player lifecycle. Before the fix, joining humans were absent from selection while their replaced bots remained registered. Team changes now refresh the selection entry; removal defers readiness evaluation until the current player/bot replacement finishes.
- Hero cards use two compact columns. The countdown displays whole seconds, and readiness reads `N / 8 ready` using the existing presentation timer instead of Blueprint Tick. An initial focus target is configured, but keyboard activation did not select a hero in this test and remains unresolved; gamepad acceptance is unverified.
- Four same-process PIE worlds each had exactly one active selection screen, eight live players, and an exactly matching selection roster. Four bots were ready before any human locked. Physical mouse input selected Argus on the host, Rawlins on client 1, Korryn on client 2, and Alona on client 3. Readiness visibly advanced as clients locked. After the final host lock, every world reported selection inactive, eight ready, an exact roster match, and zero active selection screens. Server and owning-client PlayerStates agreed on locked hero IDs.

Evidence: `Saved/Logs/codex-selection-four-player-verified.log`. Builds `codex-selection-roster-build.log` and `codex-selection-menu-input-build.log` exited 0. No Blueprint runtime error, Accessed None, or ensure failure was found in the final log. One read-only inspection used the wrong Python controller method and was corrected; that tooling error is recorded separately from gameplay. Direct staging entry also attempts relic spawning without arena spawn points and leaves the players in the staging world after selection; this run is not travel, gameplay, separate-process, or packaged-flow evidence. Auto-lock was disabled only on the transient server instance. PIE stopped and test settings were restored afterward.

Level-Blueprint editor-tool caveat: the generic Blueprint mutation tool saved a `.uasset` beside the `.umap`. The level was explicitly saved with `EditorLoadingAndSavingUtils.save_map`, the generated duplicate was removed, and a fresh editor reload confirmed the saved map no longer contains the old BeginPlay UI entry. Human disconnect/rejoin, deferred readiness after disconnect, keyboard/gamepad activation, and packaged regression remain open.

## Remaining acceptance checks

1. Record terminal build/cook/stage results and first causal errors. Preserve manifests, executable/Pak hashes, effective configuration, and matching symbols for the exact tested output.
2. Launch the staged executable with the editor closed. Confirm the frontend opens without loose project content or a cook server. A source-machine smoke test does not prove clean-machine compatibility.
3. Host LAN through the frontend, discover it from three separate client processes, join, select heroes and teams, and enter Dorado. Verify four humans plus four bots, four players per team, and consistent selection on every peer. Direct-IP joining is supplementary evidence.
4. Exercise movement, attacks, abilities, relic pickup/pass/throw/fumble/score, buildable placement/damage/persistence, base healing, and shop transactions from owning clients. Compare authority and client state; include insufficient funds, duplicate requests, and forbidden purchase windows.
5. Exercise deaths and repeated respawns, upgrade persistence, disconnects, and joining during allowed phases. Verify input, camera, HUD, team counts, and bot replacement after each transition.
6. Complete a best-of-five match through gameplay, covering goal, elimination, and time/territory round outcomes across the test set. Verify round summaries, final results, and gold earned independently of spending.
7. Return each peer to a usable frontend. Host/join a second match and verify fresh match gold, upgrades, stats, scores, buildables, and session state.
8. Repeat the gameplay and transition checks on DevMap. Run a clean second-PC LAN test with required runtimes and firewall configuration, and record startup/dependency failures separately from gameplay failures.

All items remain open until backed by logs and visible behavior from the packaged candidate. Native probes may supplement gameplay but do not prove physical input or complete frontend navigation. Two same-process PIE worlds do not satisfy these gates.
