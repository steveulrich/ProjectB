# Seamless Travel NetGUID & PlayerState Replication Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Recorded candidate verdict:** **PASSED** (`0` `LogRep: Warning: Attempt to reassign read-only NetGUID` warnings, `0` `StartReplicating: Invalid Net GUID` errors, `0` pins/sanitizations outside the active observer window, scoped traveling-driver tracking, and verified module-hook cleanup across headless automation, 4-world PIE, and 4-process packaged `L_BW_HeroSelect_Staging` -> `L_BW_Dorado` + rematch transitions at 200ms lag with delayed client3 at 12 FPS).
- **Frozen Candidate:** `LAN-20261019-01` (`Saved/StagedBuilds/LAN-20261019-01`, `LyraGame.exe` SHA-256 `64FAB3BD1EC1F0C7FCA61CFFE0EC8F7DCC0D45F531F5F8AE2672E50E9064C338`; earlier `LAN-20261014-02` retained).

## 2. Root Cause Identification (`BwayTravelGuidDiagnostics.cpp`)

Scoped `LogNetPackageMap` diagnostics around `L_BW_HeroSelect_Staging` -> `L_BW_Dorado` seamless travel identified two coupled mechanisms behind the historical `Attempt to reassign read-only NetGUID` warnings and `StartReplicating: Invalid Net GUID` subobject errors:

1. **Outgoing Staging `PlayerState` Subobject Replication Race (`ABreakawayGameMode::GetSeamlessTravelActorList`):**
   - During `FSeamlessTravelHandler::StartLoadingDestination`, the engine copies `PlayerState` properties into a newly spawned destination-world `ABwayPlayerState` (`HandleSeamlessTravelPlayer` / `CopyProperties`), while the outgoing staging `ABwayPlayerState` and its replicated subobjects (`AbilitySystemComponent`, `HealthSet`, `CombatSet`, `UpgradeComponent`, `CurrencySet`) briefly co-exist on the net driver before transition world teardown.
   - If the outgoing staging `ABwayPlayerState` remains active on replication channels after `GetSeamlessTravelActorList`, `FNetGUIDCache::CleanReferences` marks its evicted subobject entries with a non-zero `ReadOnlyTimestamp` while late channel updates attempt to reassign or start replicating those subobject NetGUIDs on clients.
2. **Transition GC Eviction of Static Class/CDO/Package NetGUIDs (`FNetGUIDCache::CleanReferences`):**
   - During transition-world garbage collection, unpinned `UClass`, `ClassDefaultObject` (`Default__BwayGameState`, `Default__BwayPlayerState`), and `UPackage` (`FWeakObjectPtr`) cache entries in `FNetGUIDCache::ObjectLookup` could be marked read-only (`ReadOnlyTimestamp != 0`) and trigger `Attempt to reassign read-only NetGUID` when the destination world re-registered the same static archetypes.

## 3. Production Fix & Scoped Diagnostics

- **Outgoing `PlayerState` Replication Quiescence (`ABreakawayGameMode::GetSeamlessTravelActorList`):**
  - After `Super::GetSeamlessTravelActorList` preserves the outgoing `PlayerState`s for property copying, the override calls `PS->SetReplicates(false)` and `World->RemoveNetworkActor(PS)`. It applies on both transition legs. The automatic travel-start hook also quiesces authority `PlayerState`s before map loading. Fresh destination `PlayerState`s retain their replication defaults.
- **Automatic Scoped Travel Diagnostics & Static Archetype Pinning (`BwayTravelGuidDiagnostics.cpp` & `BreakawayCoreRuntimeModule.cpp`):**
  - Hooks are installed via `BwayTravelGuid::InstallHooks()` in `FBreakawayCoreRuntimeModule::StartupModule()` (deferred through `FCoreDelegates::GetOnPostEngineInit()` if `FCoreUObjectDelegates` is not yet ready) and removed via `BwayTravelGuid::UninstallHooks()` in `FBreakawayCoreRuntimeModule::ShutdownModule()`.
  - `FObserver` tracks only the `UNetDriver`s of worlds actively undergoing seamless travel (`TrackTravelingDriver`, `IsTrackedDriver`, `IsTrackedWorld`), ignoring unrelated editor/preview worlds and non-traveling drivers.
  - Active observation pins valid static GUID objects and their packages (`PinStaticCacheObjectsForDriver`), excluding map packages, and only when `bActive && IsTrackedDriver(Driver)` holds. Multi-world PIE and delayed-client destination arrivals (`ArrivedWorlds`) shorten retention only after all tracked traveling worlds arrive on `L_BW_Dorado`. `Stop` removes the ticker and GC callback, clears deadlines, and releases all acquired pins.
  - `ShouldClearReadOnlyTimestamp` preserves invalid-object, non-reciprocal, retired-alias, and dead dynamic/unloaded static GUID retirement timestamps so Unreal's 90-second retirement window remains intact.
- **Legacy `ABreakawayGameMode` Debt Cleanup Audit:**
  - Duplicate round/scoring/relic state on `ABreakawayGameMode` remains removed while required delegation to `UBwayRelicManagerComponent` (`RelicMgr->SpawnRelic()`) and `UBwayRoundManagementComponent` is preserved and verified against serialized Blueprints (`B_BWayGameMode`, `B_BWayGameMode_Default`) in `Breakaway.Respawn.EscalatingProgression`.

## 4. Verified Scope Summary Table

| Phase | Condition | Result & Evidence |
|---|---|---|
| Headless GUID Retirement, Hook Lifecycle & Legacy Audit | `Breakaway.Travel.GuidRetirement+Breakaway.Travel.HookLifecycle+Breakaway.Respawn.EscalatingProgression` | **PASS** — 4/4 Goal 3 headless suites pass (`0` errors/warnings; `Saved/Logs/goal3-respawn-travel-20261019/headless-goal3-2.json`) |
| 4-World PIE Staging -> Dorado Travel + Respawn (200ms) | 4 worlds, `L_BW_HeroSelect_Staging` -> `L_BW_Dorado`, `-PktLag=200` | **PASS** — `readOnlyGuidWarnings: 0`, `invalidNetGuidErrors: 0`, `vsmOverflowLines: 0`, `pinsOutsideObserverWindow: 0`, `sanitizeOutsideObserverWindow: 0`, `trackedDrivers: 4`, `hooksInstalled: 1`, `hooksRemoved: 1`, 8 PlayerStates & 4 locked humans/world (`Saved/Logs/goal3-respawn-travel-20261019/pie-goal3-summary-2.json`, `pie-goal3-guid-brief-2.json`) |
| 4-Process Packaged Connected Travel + Rematch (200ms + Delayed Client3) | `LAN-20261019-01` (`2 D3D12 + 2 NullRHI`, `client3` at `12 FPS`, `-PktLag=200`) | **PASS** — 2 seamless travels per peer (`8` total), `readOnlyGuidWarnings: 0`, `invalidNetGuidErrors: 0`, `vsmOverflowLines: 0`, `pinsOutsideObserverWindow: 0`, `sanitizeOutsideObserverWindow: 0`, `errorLineCount: 0` across all 4 logs (`Saved/Logs/goal3-respawn-travel-20261019/packaged/packaged-result.json`, `packaged/guid-brief.json`) |
| Historical 4-World PIE Diagnostic Runs (01–03) | Default, 200ms lag, 200ms + 1 FPS | **PASS** — 0 GUID errors or read-only warnings (`Saved/Logs/buildable-authority-20260927/travel-guid01-summary.json`) |
| Historical Packaged 15 FPS Diagnostic (`LAN-20260929-47`) | 4 processes, 200ms lag, 15 FPS cap | **PASS** — 0 GUID errors or read-only warnings (`Saved/Logs/lan-travel-watch-20260930-48/travel15fps-result.json`) |

## 5. Dorado VSM Non-Nanite Queue Overflow Resolution (Goal 3 Scope 4)

- **Root Cause (`Saved/Logs/goal3-respawn-travel-20261019/vsm/vsm-D-diag-epic.log`):** On cold-cache frame ~5 when `L_BW_Dorado` first renders under Epic VSM settings (`r.Shadow.Virtual.ResolutionLodBiasDirectional=-1.5`, `Clipmap.FirstLevel=6`, `LastLevel=22`, `RadiusThreshold=0.01`), 16 large arena floor/wall static meshes (`el_dorado_fbx_Scene_*`, 147–473 VSM page overlaps each) were routed through the non-Nanite VSM marking path and overflowed the 128-slot `SharedMarkingJobs` queue because those 16 `UStaticMesh` assets had `Nanite=False` and their shared Masked material `/Game/Characters/Cosmetics/M_TeamColorBasic` had `bUsedWithNanite=False`.
- **Fix (Asset-Level Nanite Enablement + Reverted Config Degradations):**
  - Enabled Nanite on all 16 project-owned `el_dorado_fbx_Scene_*` static meshes under `Plugins/GameFeatures/BreakawayCore/Content/Environments/Levels/BW_Dorado/` (`dorado-mesh-audit-applied.json`) and enabled `bUsedWithNanite=True` (`MATUSAGE_Nanite`) on `/Game/Characters/Cosmetics/M_TeamColorBasic` (`dorado-material-nanite-usage.json`).
  - Reverted `Config/DefaultEngine.ini` and `Config/DefaultScalability.ini` back to `ed6008b9^` so Epic shadow presentation (`ResolutionLodBiasDirectional=-1.5`, `Clipmap.FirstLevel=6`, `LastLevel=22`, `RadiusThreshold=0.01`, coarse-page defaults) is fully preserved.
- **Verification (`Saved/Logs/goal3-respawn-travel-20261019/vsm/vsm-summary.json`):**
  - Per-frame `VSM/OVERFLOW_FLAGS` CSV profiling (`2,400` frames per run at `1920x1080` in `LAN-20261019-01`, profiles `E-cooked-nanite-epic` and `E2-cooked-nanite-epic-arena`) recorded **0** overflow frames (`markingJobQueue=0`, `pagePool=0`, `oppMaxLights=0`, `visibleInstances=0`) and **0** `[VSM]` warning log lines while reducing `VSM/NonNanitePreCullInstanceCount` from `78.5–100.0` to `16.5–25.8` and preserving full Epic shadow page density (`310.0` mean requested VSM pages/frame vs `9.9` under the reverted `ed6008b9` degradation) and `11.67ms` mean frame time.
  - In-engine `HighResShot` captures (`vsm-E2-cooked-nanite-epic-arena-highres-0.png`, `vsm-E2-cooked-nanite-epic-arena-highres-1.png`) confirm crisp character contact shadows and distant architectural shadow edges across the Dorado arena.
