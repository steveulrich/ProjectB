# Packaged LAN acceptance

## Current candidate

- Candidate: `LAN-20260907-01`, Win64 Development, `LyraGame`, project base `332ff649` plus the recorded source patch. Includes the bot unpossession cleanup and expanded task diagnostics, plus the previously saved duplicate-HUD registration fix. Editor compilation passed after restoring three explicit JSON key conversions in the existing bridge edits. Build/cook/stage passed in 198.16 seconds, AutomationTool exit 0. Exact project/engine revisions, tracked patches, statuses, stage inventory and hashes: `Saved/Logs/codex-packaged-LAN-20260907-01-*`. Untracked source contents are not captured by those patches.
- Stage: `Saved/StagedBuilds/LAN-20260907-01/Windows/LyraGame.exe`; actual executable under `Windows/ProjectB/Binaries/Win64/LyraGame.exe`. Pipeline script: `Saved/package-LAN-20260907-01.ps1`. Existing cook roots and local symbols retained; no deployment or clean-machine validation.
- Offline headless packaged regression passed three natural goals, two round resets, and PostMatch (3–0) at 02:55:22 UTC on September 8 / September 7 local. The test entered Dorado directly with CaptureTheRelic experience, seven bots, selection skipped, three points to win, 90-second rounds, five-second warmup and three-second post-round. No synthetic scoring or movement commands. `-testexit` terminated the process after the real PostMatch log, so this is not results-screen or graceful frontend-return evidence. Log: `Saved/Logs/codex-packaged01-bots-headless-20260907.log`.
- Interactive frontend launched, but the new executable path raised a Windows Security firewall prompt. Computer Use lists PickerHost with no targetable window. Automatic review rejected the attempted **Show more** inspection click because it could not verify the button effect or permission scope. No firewall permission was changed. The visible package remains at that prompt; user handling is needed before interactive LAN verification can continue. The offline run does not validate this route or any client replication.
- Next: clear the visible prompt, host the LAN playlist, confirm moving bots and the single HUD, then complete the remaining multiplayer gates below. The native diagnostic command now also reports task component identity, live movement-task state and path-update postponement.

### Packaged bot movement reproduction and repair

In candidate `-08`, a fresh interactive LAN fixture plus physical Argus selection/lock reached Dorado. All seven bots stayed at spawn. `bway.Debug.RelicBots` reported valid non-partial paths, projected endpoints, correct goal/team Blackboard values, walking mode and speed 600, but idle path following while `Move To[13]` remained active. Gameplay task priority queues on the live pawns were empty. Evidence: `Saved/Logs/codex-packaged08-bot-task-reproduced-20260907.log`. No full match passed in that run.

The controller intentionally disabled automatic brain cleanup on unpossession. Unreal tears down the outgoing pawn's gameplay tasks, while `StartTree` skips an already-started tree. Source now cleans the brain before detaching a pawn, allowing `OnPossess` to start a fresh tree and movement tasks against the replacement pawn. This applies to warmup/round replacement and death/respawn, not only a timed round. Native/editor build: `Saved/Logs/codex-bot-possession-build-fixed-20260907.log`, exit 0. The offline packaged match above verifies natural scoring across round replacement; physical LAN, death/respawn, and disconnect/travel regression remain open.

The control run used the old `-08` executable with the same map, experience, bot count and phase settings. Its first round reached the full 90-second timeout with the relic at midfield, no winner, and no goal. The rebuilt candidate scored at 02:54:17, 02:54:40 and 02:55:19 UTC, then reached PostMatch. Both runs used the native `-testexit` log trigger to end the process at their intended checkpoint (first PostRound for control; PostMatch for the repaired build). Control log: `Saved/Logs/codex-packaged08-bots-headless-control-20260907.log`; compact comparison: `Saved/Logs/codex-bot-package-comparison-20260907.json`. This is an observed before/after regression, not a soak test or a network proof. BF-092 records prevention; BF-046 records the recurring bridge conversions.

## Previous candidate (-08)

- Candidate: `LAN-20260906-08`, Win64 Development, `LyraGame`, project base `fb76cbdf`; includes the frontend Breakaway HUD reference and `bway.Debug.RelicBots`. Build/cook/stage passed with AutomationTool exit 0 in 154.92 seconds. A transient Zen connection failure recovered during staging. Log/exit status: `Saved/Logs/codex-lan-package-20260906-08*`; source/engine identity, patches, status, and complete staged-file SHA-256/size inventory: `Saved/Logs/codex-packaged-LAN-20260906-08-*`.
- Stage: `Saved/StagedBuilds/LAN-20260906-08/Windows/LyraGame.exe`; runtime log: `Saved/Logs/codex-packaged08-bot-host.log`. Launched through the existing LAN test fixture. Interactive diagnostics and HUD verification are pending. Windows Security displayed a firewall prompt; despite user authorization, the computer-use tool exposes PickerHost with an empty windows list and rejects game-targeted clicks over that separate dialog. User assistance was requested for this tool limitation, not for renewed permission.
- Next: run `bway.Debug.RelicBots` on the active packaged host, inspect the six-slot HUD, and continue the complete multiplayer match/front-end acceptance sequence. No candidate -08 runtime acceptance is claimed yet.

## Previous candidate (-07)

### Candidate -08 visible HUD and duplicate registrations

At 16:11 UTC, a screenshot of the running -08 host showed the six-slot ability bar with icons and key labels beneath the still-open firewall dialog. This confirms the Breakaway HUD replacement is active in the packaged frontend match; it does not test ability activation, cooldowns, or client replication. The same view showed duplicate score/timer displays.

Native cold property inspection found direct CaptureTheRelic experience entries for `W_BW_CaptureTheRelic_ScoreWidget` at `HUD.Slot.TeamScore` and `W_BW_RelicStatusWidget` at `HUD.Slot.ModeStatus`. `LAS_BW_MatchHUD` and `EAS_BW_CaptureTheRelic` already supply those same entries. The development experience's direct list was empty. Source now clears the two duplicate entries from `GameFeatureAction_AddWidgets_1`, preserving the shared action sets. The asset compiled/saved; a separate cold commandlet exited 0 and confirmed the direct list empty with one shared entry per widget. Evidence: `codex-duplicate-score-hud-roots.log`, `codex-duplicate-match-widgets-repair-v3.log`, and `codex-duplicate-match-widgets-cold-verify.log` under `Saved/Logs`. Earlier attempts stopped without edits because the widget-entry fields are not exposed to Python. The fix is not in -08; packaged visual verification remains open.

- Candidate: `LAN-20260906-07`, Win64 Development, `LyraGame`, project base `3c6fd82b`; includes the weak-reference round-cleanup repair. Build/cook/stage passed with AutomationTool exit 0 in 145.98 seconds. Pipeline log and exit status: `Saved/Logs/codex-lan-package-20260906-07*`. Source/engine HEAD, status, binary patches, and complete staged-file SHA-256/size inventory: `Saved/Logs/codex-packaged-LAN-20260906-07-*` (untracked contents are not preserved by patches).
- Stage: `Saved/StagedBuilds/LAN-20260906-07/Windows/LyraGame.exe`. Cold startup and the existing `bway.Test.HostLAN` fixture reached hero selection and Dorado. No client has joined this candidate yet: a Windows Security firewall prompt blocked UI work and was handed to the user. Host logs reached round two at 15:45:13 UTC, but without bot replacement this does **not** validate the crash repair. Runtime log: `Saved/Logs/codex-packaged07-lifetime-host.log`.
- Acceptance remains open: repeat late join during an active round, verify bot removal and round cleanup after collection, then complete the multiplayer and frontend gates. This candidate predates the HUD reference repair described next.

### Bot diagnostic command

Candidate -07 late-join regression passed the observed round-reset scenario at 16:01–16:02 UTC. After the firewall prompt cleared, a second packaged process connected directly to localhost during round 12. Server logs record Bot 7 removal at 16:01:25 and successful join. The host started round 13 at 16:02:39; the client received RoundComplete and then RoundActive, and both processes remained alive. No forced garbage collection or complete-match pass is claimed. Evidence: `Saved/Logs/codex-packaged07-lifetime-host.log` and `codex-packaged07-lifetime-client.log`. This is a direct-connection regression, not a new frontend discovery pass. The user explicitly authorized handling verified Unreal build security prompts; that authorization is now recorded in the testing ladder and installed computer-use guidance.

Source adds the non-Shipping console command `bway.Debug.RelicBots`. Run it on the server to capture the current world/net mode, active relic, each bot's pawn/velocity/movement mode and speed, active Behavior Tree tasks, relevant Blackboard values, selected agent NavData, default-extent endpoint projections, and a synchronous path result to the relic. It is an on-demand read-only snapshot, with no movement request or Blackboard mutation. Client controllers are server-only, so a zero client count is expected. `LyraEditor` compilation passed (`Saved/Logs/codex-bot-diagnostic-build.log`, 29.11 seconds); live command execution remains unverified and the command is not in candidate -07.

### Frontend experience HUD mismatch

Read-only cold asset inspection found that `B_BW_Experience_CaptureTheRelic`, used by the frontend playlist, referenced `LAS_ShooterGame_StandardHUD`; the development experience referenced `LAS_BW_MatchHUD`. Shooter's action set installs `W_ShooterHUDLayout` and the three-slot `W_QuickBar`, explaining the packaged three EMPTY slots despite the host receiving six tagged hero abilities. The Breakaway action set installs `WBP_BW_MatchHUDLayout`, which provides the intended match HUD slots.

Source now replaces only the CaptureTheRelic experience's Shooter HUD action-set reference with the existing Breakaway HUD action set. The asset compiled and saved in a commandlet; evidence: `Saved/Logs/codex-capture-relic-hud-repair.log`. A separate cold commandlet exited 0 and confirmed the replacement persisted (`codex-capture-relic-hud-cold-verify.log`, 15:45:49 UTC). Packaged visual verification remains open. The experience's Shooter standard components also differ from the development experience (quickbar, damage-number and character nameplate targets); those references have not been changed. The late joiner's missing selected hero remains a separate issue.

## Previous candidate (-06)

### Saved navigation investigation (September 6, 15:48–15:56 UTC)

The original committed Dorado map passes all twelve spawn-to-midfield path queries after both endpoints are projected onto navigation: 12 valid, non-partial paths, 0 invalid. Midfield projects to Z=30; spawn points project to approximately Z=240–249. Evidence: `Saved/Logs/codex-dorado-nav-original-projected-paths.log`. This is cold source-asset navigation evidence, not cooked navigation or live bot movement evidence.

The first diagnostic incorrectly targeted midfield at Z=350 and returned no paths. A Python rebuild attempt and a native `ResavePackages -BuildNavigationData` attempt also encountered `AsyncLoadLock` (0x20). The native command returned exit 0 despite reporting that navigation was not built. Engine source shows the lock waits for editor ticker callbacks. A command-only `-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False` override allowed the dedicated commandlet to finish its mesh compilation, build static navigation, and save Dorado. Corrected projected queries passed in both rebuilt and original maps. The experimental map change was restored to HEAD; the rebuilt copy is retained under `Saved/L_BW_Dorado_nav_rebuild_verified.umap`. No navigation settings or source map changes are retained.

Next bot diagnostic: inspect the actual packaged server's loaded NavData, projected endpoints, path-following result, and Blackboard/active branch. A running Behavior Tree log alone remains insufficient. The Windows Security dialog still covered candidate -07 during this investigation; no client-replacement crash regression or interactive bot check was performed.

- Candidate: `LAN-20260906-06`, Win64 Development, `LyraGame`, built September 6, 2026. This supersedes `-05` as the packaged candidate.
- Project base: `24e0cd63065ccf9107a35dc9abf816b9aaf871cd`, including bot pursuit, Dorado goal ownership and replication, server-owned rematch travel, results layout, and gameplay HUD suppression during results. Project and engine HEAD, status, and binary patches are retained under `Saved/Logs/codex-packaged-LAN-20260906-06-*`; the patches do not preserve untracked file contents.
- Pipeline: the BuildCookRun command below with staging directory `LAN-20260906-06`. Build, cook, and stage passed; AutomationTool exited 0 after 140.06 seconds. The full log is `Saved/Logs/codex-lan-package-20260906-06.log`, with exit status in the corresponding `-exit.txt` file. Existing gameplay-tag and asset migration warnings remain.
- Artifact: `Saved/StagedBuilds/LAN-20260906-06/Windows/LyraGame.exe`; the actual game executable is `Windows/ProjectB/Binaries/Win64/LyraGame.exe` beneath the candidate directory. The complete staged-file SHA-256 and size inventory is `Saved/Logs/codex-packaged-LAN-20260906-06-sha256.csv`. Symbols remain in this local Development stage; no release archive or clean-device deployment has been performed.
- Acceptance: **failed runtime gate**. Physical frontend host/find/join passed with two packaged processes on one PC, but the host crashed during the first round reset after a joining human replaced a bot. See the crash diagnosis and source repair below. Full match, results, rematch, four humans, and second-PC LAN acceptance remain open.

### Candidate -06 runtime failure and source repair

Physical frontend testing on September 6 at 15:29–15:34 UTC passed cold startup, Play Lyra -> Start a Game -> LAN -> Local (listen server + bots), then a second packaged process's Browse discovery and join. The client joined Dorado during round one at 15:32:31; the server removed Bot 7 to retain eight players. This is two processes on one PC, not the four-human or second-PC gate. Host selection timed out normally before the client joined.

Both visible gameplay HUDs showed empty ability slots and the waiting-for-players banner. The joining player's server log reports no selected hero, while the host received Korryn's six tagged abilities. These are distinct unresolved symptoms. Bots stayed near their spawns in the observed views, and round one expired without a score; packaged objective pursuit remains unverified.

At 15:33:37 the host crashed during the round-two transition. The exact staged executable/PDB and minidump resolve the failure to `RemovePassiveGoldIncome -> GetAbilitySystemComponentFromActor -> GetInterfaceAddress`, reading reclaimed memory. `PassiveGoldEffectHandles` held unreflected `TObjectPtr` keys after the replaced bot's PlayerState was destroyed. Source now uses weak keys for this map and the neighboring non-owning respawn timer cache. `LyraEditor` compilation passed in 28.94 seconds (`Saved/Logs/codex-passive-gold-lifetime-build.log`). The fix is not in candidate -06 and needs the same packaged join/replacement/round-reset regression before runtime acceptance.

Evidence: `Saved/Logs/codex-packaged06-frontend-host.log`, `codex-packaged06-frontend-client.log`, and `codex-packaged06-round-reset-crash-stack.log`. The dump and CrashContext remain beneath candidate -06's `Windows/ProjectB/Saved/Crashes/UECC-Windows-3EC825294629116E567561A51D27A7B0_0000`. The client timed out after 60 seconds and returned to the frontend; its process was then closed. No complete match or rematch pass is claimed.

## Previous candidate (-05)

- Candidate: `LAN-20260906-05`, Win64 Development, `LyraGame`; built September 6, 2026 at approximately 06:13 UTC.
- Project base: `6cfdbe75c303ab6df3046576d096eb864ff6a6da`, including the selection ownership, roster, and layout fixes. The uncommitted PostMatch return guard and other tracked differences are captured in `Saved/Logs/codex-packaged-LAN-20260906-05-project.patch`.
- Engine baseline: the same source engine listed below, with tracked changes and status captured in `codex-packaged-LAN-20260906-05-engine.patch` and `codex-packaged-LAN-20260906-05-engine-status.txt` under `Saved/Logs`. Untracked engine file contents are not captured by the patch.
- Pipeline: the BuildCookRun command below with staging directory changed to `LAN-20260906-05`. Build, cook, and stage passed; AutomationTool exited with code 0 after 145 seconds. A transient Zen connection failure recovered during staging.
- Artifact: `Saved/StagedBuilds/LAN-20260906-05/Windows/LyraGame.exe`; SHA-256 inventory: `Saved/Logs/codex-packaged-LAN-20260906-05-sha256.csv`; pipeline log: `Saved/Logs/codex-lan-package-20260906-05.log`.
- Acceptance: frontend launched. Interactive keyboard verification is pending because a Windows Security network permission dialog covers the game and cannot be targeted by the available UI automation. No four-player packaged or full-match pass is claimed.

### Keyboard investigation

The hero card is a focusable CommonButtonBase with no triggering input action and no hold requirement. Engine `CommonButtonTypes.cpp` suppresses Slate Accept in `SCommonButton::OnKeyDown` and `OnKeyUp` when `CommonButtonAcceptKeyHandling` is `Ignore`. The engine constructor defaults to `Ignore`, and no project or engine config override was found. `TriggerClick` is the engine-supported alternative. A packaged comparison and frontend regression check remain required before accepting that configuration change; the candidate above has no such override.

The source config now explicitly sets `CommonButtonAcceptKeyHandling=TriggerClick`. This is not included in candidate `-05`. Verify physical Enter and Space selection, navigation between cards, lock activation, and frontend navigation after cold startup. Also check that existing action-bound buttons do not activate twice and that gamepad acceptance still works. Source inspection alone does not establish those passes.

Cold-load verification passed at 12:22 UTC on September 6. An unattended `UnrealEditor-Cmd -run=pythonscript -unattended -nullrhi` run exited with code 0; native `getall CommonUISettings CommonButtonAcceptKeyHandling SHOWDEFAULTS` reported `TriggerClick` on both the class default and transient settings objects. Evidence: `Saved/Logs/codex-commonui-cold-config-native.log`. The first inspection script failed because the property was not exposed through the Python property API; its failure is retained in `codex-commonui-cold-config.log`. This verifies config loading, not physical input. The commandlet required source-engine and user-cache access; the earlier sandboxed interactive editor remained stalled at Zen data-path validation.

### Unattended match evidence

The hidden candidate `-05` listen-server process continued running after the attempted window close. At 12:17 UTC it reached round 230; at 12:19 UTC that round again expired with the relic on midfield and no winner. Recent rounds repeat this pattern while the bot controller logs report a running behavior tree. This does not establish why bots fail to progress the objective, nor justify changing draw rules. Investigate navigation, blackboard state, and task execution before claiming a complete unattended match. Evidence excerpt: `Saved/Logs/codex-packaged05-unattended-round230-evidence.log`; original: `codex-packaged05-keyboard-baseline.log`. A successful CloseMainWindow return alone must not be treated as process termination.

## Previous candidate (-04)

### Goal ownership repair (September 6, 14:10–14:13 UTC)

The suspected goal-class mismatch was disproved: B_Bway_Goal derives from BwayGoalVolume. Both Dorado goal spawn points instead had TeamIndex=-1, which the spawn data copied into OwningTeam. The map now sets SP_GoalTeam1 to 0 and SP_GoalTeam2 to 1, matching their existing labels and goal-spawn assets; the relic spawn remains neutral. The map was saved explicitly as a .umap.

Fresh four-player PIE verified opposite goal locations in all four server bot Blackboards. At 14:11:36 a bot scored in the opposing goal, Team 1 received one point, and round two started. No scripted movement, pickup, or scoring calls were used. Round two ended through the territory timeout rule, awarding Team 2 a point; round three started. Full match completion and round-reset bot behavior remain unverified. Client goal ownership still uses the nonreplicated default and needs repair. Evidence: `Saved/Logs/codex-dorado-goal-team-scoring-verified.log`. PIE was stopped.

Inspection hygiene: the earlier pursuit run's PIE teardown reported references retained by top-level Python inspection variables. Subsequent inspections use function-local runtime references and delete the helper after returning. Treat those earlier teardown diagnostics as inspection contamination, not unexplained gameplay crashes.

### Pursuit repair and runtime evidence (September 6, 14:05–14:07 UTC)

After the stalled editor exited, an interactive editor launch with engine/cache access restored the bridge. The authored graph's existing Free Relic selector was reconnected to the root and saved, preserving its existing Move To RelicLocation task. In a fresh four-player Dorado PIE session, selection timed out normally, gameplay started, all four bots moved from spawn to midfield, and a bot acquired the relic through existing proximity/request behavior. This establishes movement and pickup after the reconnect; a new explicit pickup sequence was not necessary for this observed path.

Scoring remains blocked: all four bot Blackboards reported an unset EnemyGoalLocation (MAX_FLT vector), while the server contained two B_Bway_Goal actors at Y=-4000 and Y=4000. The bot helper currently searches for ABwayGoalVolume. Investigate the live goal class and its team contract before changing lookup. This run does not establish scoring, possession-change interruption, full-match completion, or packaged behavior. PIE was stopped. Evidence: `Saved/Logs/codex-bot-pursuit-reconnected-verified.log`.

### Bot asset diagnosis (September 6, 12:24 UTC)

The unattended read-only asset inspection exited with code 0. `BT_BW_RelicBot` uses `BB_BW_RelicBot`; its key names and types match the native service, and the root selector has the update service attached. However, the reachable root children are carrier, enemy-carrier chase, and a `RelicLocation Is Set` branch whose composite (`BTComposite_Selector_0`) has **no children**. The `Move To RelicLocation` tasks (`BTTask_MoveTo_2` and `_3`) are not reachable from the root. This is a concrete asset wiring defect that prevents the tree from pursuing an initially free relic; a successful `RunBehaviorTree` log does not validate it. All inspected Blackboard decorators also have observer aborts disabled.

Repair the authored graph with a reachable free-relic sequence: availability condition, move to relic, then pickup. Configure possession-change aborts deliberately and verify the serialized tree after reopening, then prove server-side movement, pickup, scoring, and match completion. Do not patch only the runtime `Children` array while leaving the editor graph inconsistent. Dorado contains both a NavMeshBoundsVolume and RecastNavMesh, but actor presence does not prove usable cooked paths. Evidence: `Saved/Logs/codex-relic-bot-asset-inspection.log`; inspection script: `Saved/inspect_relic_bot_assets.py`. No asset mutation or runtime navigation pass was performed.

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

## Goal ownership and complete PIE match (September 6, 14:18–14:22 UTC)

Goal ownership now replicates through `OwningTeam`, with an OnRep color refresh. Spawn initialization uses an authority setter that also refreshes the server material after BeginPlay. Previously clients retained default team 0, and post-spawn ownership assignment left the server color stale. The editor build passed (`Saved/Logs/codex-goal-ownership-build.log`, exit 0).

After a cold editor restart, four same-process PIE worlds on Dorado each reported goal ownership `(-4000, 1), (4000, 0)` by Y position. The server and all three clients matched. No synthetic scoring or bot movement requests were used. Bots scored five natural goals, with round winners 1, 1, 2, 2, 2; the match reached PostMatch at 14:21:54 UTC with Team 2 winning 3–2. All four peers logged the same five-round result. Bots therefore resumed successfully across four round resets in this run. PIE stopped afterward.

Evidence: `Saved/Logs/codex-goal-ownership-full-match-verified.log`. This is replicated state and log evidence in same-process PIE, not packaged LAN, physical frontend navigation, visual goal-color acceptance, late joining, or second-match reset evidence. The previous idle-bot observation alone does not establish a round-reset defect; this completed run supersedes that suspicion. Packaged candidate `-05` predates these fixes.

## Results return and second-session PIE smoke (September 6, 14:23–14:30 UTC)

A fresh four-world PIE match completed naturally, 3–0 in three rounds. Physical input on client 1's **Return to Lobby** button returned all four worlds to `L_LyraFrontEnd`; all four had zero relic bot controllers. The former host and client visibly displayed usable frontend menus. The existing PostMatch return guard allowed this valid request; rejection outside PostMatch remains untested.

From the returned host, physical **Play Lyra → Start a game → Network: LAN → Local / Listen server + bots** created a new session. The returned client used **Play Lyra → Browse**, discovered the new 1/8 session at 20 ms, and joined its row. Both travelled to Dorado after selection timeout. Authority had eight players and six bots; the client had the same eight-player count. The two unused peers remained in their independent frontends. The new match displayed 0–0 and started round 1. At 14:30:03 UTC, all sixteen server/client PlayerState copies reported objective score 0 and match gold earned 36, approximately 36 seconds after round start. This supports fresh scoring/economy state in this run; no purchased upgrades or placed buildables were carried through this test, so their reset is unproven.

Evidence: `Saved/Logs/codex-results-return-second-session-verified.log`. This extends same-process PIE evidence only. Four separate processes, packaged travel, full second-match completion, and clean-machine LAN remain open. The results table visibly overlaps names and columns at the 859×520 client window size. Enter after Tab did not select a hero in fresh client selection; the CommonUI config change alone has not resolved verified keyboard acceptance. Play Again still uses local level reload/restart and needs a multiplayer implementation.

## Rematch implementation in progress (September 6, 14:35–14:48 UTC)

Play Again now submits intent through the owning PlayerController to a server PostMatch guard and non-seamless arena reload. Selection is rerun and PlayerStates/world actors are recreated. This replaces local `OpenLevel`/`restartlevel`. A server-side native probe rejected an early rematch. Reflective Python calls on client objects invoke implementations locally: they are **not RPC transport evidence**, including the attempted early-return probes in this run.

Two physical client Play Again clicks after natural one-point test matches reached server travel. They exposed missing listen mode in PIE's world URL, then a port reset from 17777 to 7777 during absolute travel. Retaining `World->URL.Port` did not fix it because that stored value was also 7777. The latest source reads the net driver's actual bound socket port and retains listen mode. Runtime verification of that final correction is pending; rematch has **not passed**. Logs: `codex-rematch-missing-listen-reproduced.log`, `codex-rematch-port-reset-reproduced.log`, and `codex-rematch-world-url-port-reproduced.log` under `Saved/Logs`. The final reproduction used temporary selection/scoring fixtures and a physical client button click; it is not natural full-match evidence. Packaged verification remains open.

## Rematch reconnection verified in PIE (September 6, 14:56–14:59 UTC)

The final implementation uses relative, explicitly non-seamless server travel. It removes inherited selection-skip/transition flags, retains listen mode, and normalizes the travel context's port to the net driver's bound port. Unreal rejects a port embedded in the public ServerTravel URL; relative travel retains it through the context instead. Build `Saved/Logs/codex-rematch-relative-build.log` passed (exit 0).

After a fresh editor restart, a natural goal ended a temporarily configured one-point match. A physical client **Play Again** click reached server travel at 14:58:34 UTC. The host reopened port 17777; all three clients reconnected. At 14:59:03, all four Dorado worlds had eight players and round number 0, with exactly one authority. All 32 PlayerState copies reported match gold earned 0. The client visibly displayed fresh hero selection with 4/8 ready. The host resolved the authored three-point rule again. PIE stopped afterward. Evidence: `Saved/Logs/codex-rematch-relative-verified.log`.

This supersedes the pending reconnection result above. It verifies the physical button, network request, four-peer reconnection, fresh selection, and earned-gold reset in same-process PIE. Previously purchased upgrades, placed buildables, full rematch completion, simultaneous conflicting results requests, separate-process and packaged behavior remain unverified. BF-086 records the travel and test-method pitfalls.

## Results-table layout verification (September 6)

The breakdown panel now uses viewport-relative bounds instead of a fixed 1200×720 canvas slot. Player columns are 180 units wide, with horizontal overflow support. Names use smaller, non-scrolling text with ellipses. The label column reserves the same header height as player columns and uses matching font metrics; an extra 32-unit spacer was removed from player columns. Both widget Blueprints compiled and were saved.

A fresh four-world PIE match, temporarily set to one point, reached results through a natural goal. Visual checks at client 859×520 and host 1286×760 confirmed all eight columns fit, names no longer overlap, stat values align with labels, and both footer controls remain visible. Evidence: tool screenshots in this task and `Saved/Logs/codex-results-layout-verified.log`. No native gameplay code changed. Packaged layout, full-name access for truncated labels, portrait data binding, and suppressing the underlying gameplay HUD remain open. BF-087 records the sizing, row-alignment, and CommonUI scrolling-style causes.

## Post-match HUD suppression (September 6, 15:18–15:20 UTC)

The local results orchestration widget now collapses `UI.Layer.Game` when results begin, remembers its prior visibility through a weak reference, and restores it on destruction. This suppresses gameplay presentation without changing replicated state or destroying HUD widgets. The editor build passed: `Saved/Logs/codex-results-hud-layer-build.log`, exit 0.

After a fresh restart, a four-world PIE match temporarily set to one point ended through a natural goal. All four peers logged local gameplay-layer suppression. The client results screenshot confirmed score/timer, relic labels, health bar, and ability bar were absent behind the table. Physical client **Play Again** returned to selection, then resumed gameplay with the HUD visible again. PIE stopped afterward. Evidence: `Saved/Logs/codex-results-hud-layer-verified.log` and task screenshots. This verifies the observed rematch lifecycle in PIE; packaged behavior and same-world manual results dismissal remain unverified. Debug screen messages are separate from the HUD and remain visible in development.

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
