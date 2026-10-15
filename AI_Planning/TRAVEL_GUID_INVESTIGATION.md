# Seamless Travel NetGUID & PlayerState Replication Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (`0` `LogRep: Warning: Attempt to reassign read-only NetGUID` warnings and `0` `StartReplicating: Invalid Net GUID` errors across 4-world PIE and 4-process packaged `L_BW_HeroSelect_Staging` -> `L_BW_Dorado` and rematch transitions at 200ms lag).
- **Frozen Candidate:** `LAN-20261014-02` (`Saved/StagedBuilds/LAN-20261014-02`, `LyraGame.exe` SHA-256 `996E8F7DEBC4B7D1D9F333E423EF5D9844F4FF60C27E2BF3A72CCFF2DB0C347E`).

## 2. Root Cause Identification (`BwayTravelGuidDiagnostics.cpp`)

Scoped `LogNetPackageMap` diagnostics around `L_BW_HeroSelect_Staging` -> `L_BW_Dorado` seamless travel identified two coupled mechanisms behind the historical `Attempt to reassign read-only NetGUID` warnings and `StartReplicating: Invalid Net GUID` subobject errors:

1. **Outgoing Staging `PlayerState` Subobject Replication Race (`ABreakawayGameMode::GetSeamlessTravelActorList`):**
   - During `FSeamlessTravelHandler::StartLoadingDestination`, the engine copies `PlayerState` properties into a newly spawned destination-world `ABwayPlayerState` (`HandleSeamlessTravelPlayer` / `CopyProperties`), while the outgoing staging `ABwayPlayerState` and its replicated subobjects (`AbilitySystemComponent`, `HealthSet`, `CombatSet`, `UpgradeComponent`, `CurrencySet`) briefly co-exist on the net driver before transition world teardown.
   - If the outgoing staging `ABwayPlayerState` remains active on replication channels after `GetSeamlessTravelActorList`, `FNetGUIDCache::CleanReferences` marks its evicted subobject entries with a non-zero `ReadOnlyTimestamp` while late channel updates attempt to reassign or start replicating those subobject NetGUIDs on clients.
2. **Transition GC Eviction of Static Class/CDO/Package NetGUIDs (`FNetGUIDCache::CleanReferences`):**
   - During transition-world garbage collection, unpinned `UClass`, `ClassDefaultObject` (`Default__BwayGameState`, `Default__BwayPlayerState`), and `UPackage` (`FWeakObjectPtr`) cache entries in `FNetGUIDCache::ObjectLookup` could be marked read-only (`ReadOnlyTimestamp != 0`) and trigger `Attempt to reassign read-only NetGUID` when the destination world re-registered the same static archetypes.

## 3. Production Fix & Scoped Diagnostics

- **Outgoing `PlayerState` Replication Quiescence (`ABreakawayGameMode::GetSeamlessTravelActorList`):**
  - When `bToTransition` is `true`, after `Super::GetSeamlessTravelActorList(bToTransition, ActorList)` adds the staging `PlayerState`s to the carry-over list, `ABreakawayGameMode` immediately calls `PS->SetNetDormancy(DORM_DormantAll)`, `PS->SetReplicates(false)`, and `PS->ForceNetUpdate()` on each outgoing `APlayerState` so only the freshly spawned destination-world `ABwayPlayerState` replicates to clients after `CopyProperties`.
- **Automatic Scoped Travel Diagnostics & Static Archetype Pinning (`BwayTravelGuidDiagnostics.cpp`):**
  - Automatically arms on `FWorldDelegates::OnSeamlessTravelStart` for `L_BW_HeroSelect_Staging` -> `L_BW_Dorado` transitions (`BwayTravelGuid: START reason=seamless-travel-start`), pins static `Class`/`CDO`/`UPackage` objects in `FNetGUIDCache::ObjectLookup` across transition GC (`pinned=489`), clears stale `ReadOnlyTimestamp` flags on valid static/live entries (`sanitized=104`), and disarms cleanly after transition settlement (`BwayTravelGuid: STOP`).

## 4. Verified Scope Summary Table

| Phase | Condition | Result & Evidence |
|---|---|---|
| 4-World PIE Staging -> Dorado Travel + Respawn | 4 worlds, `L_BW_HeroSelect_Staging` -> `L_BW_Dorado` | **PASS** — `readOnlyGuidWarnings: 0`, `invalidNetGuidErrors: 0`, `vsmOverflowWarnings: 0`, `travelGuidLineCount: 1005` (`Saved/Logs/pie-goal3/pie-goal3-result.json`) |
| 4-Process Packaged Connected Travel + Rematch (200ms) | `LAN-20261014-02` (`2 D3D12 + 2 NullRHI`, `-PktLag=200`) | **PASS** — 2 seamless travels per peer, `readOnlyGuidWarnings: 0`, `invalidNetGuidErrors: 0`, `vsmOverflowWarnings: 0`, `errorLines: 0` across all 4 logs (`Saved/Logs/goal3-packaged-20261014/packaged-LAN-20261014-02-200ms-result.json`) |
| Historical 4-World PIE Diagnostic Runs (01–03) | Default, 200ms lag, 200ms + 1 FPS | **PASS** — 0 GUID errors or read-only warnings (`Saved/Logs/buildable-authority-20260927/travel-guid01-summary.json`) |
| Historical Packaged 15 FPS Diagnostic (`LAN-20260929-47`) | 4 processes, 200ms lag, 15 FPS cap | **PASS** — 0 GUID errors or read-only warnings (`Saved/Logs/lan-travel-watch-20260930-48/travel15fps-result.json`) |

## 5. Dorado VSM Non-Nanite Queue Overflow Resolution (Goal 3 Scope 4)

- **Root Cause:** `Engine/Config/BaseScalability.ini` `[ShadowQuality@3]` set `r.Shadow.Virtual.ResolutionLodBiasDirectional=-1.5` ($8\times$ page area per non-Nanite instance), `r.Shadow.RadiusThreshold=0.01`, and `r.Shadow.Virtual.MarkCoarsePagesLocal=1` with `MarkCoarsePagesDirectional=2`, causing `UncachedPageRectBounds` on coarse clipmap levels in `L_BW_Dorado` to exceed the 8-page threshold (`bDoLargeJob = true`) and overflow the 128-slot `SharedMarkingJobs` queue.
- **Fix (`Config/DefaultEngine.ini` & `Config/DefaultScalability.ini`):** Configured `r.Shadow.Virtual.NonNanite.IncludeInCoarsePages=0`, `r.Shadow.Virtual.MarkCoarsePagesDirectional=0`, `r.Shadow.Virtual.MarkCoarsePagesLocal=0`, `r.Shadow.Virtual.ResolutionLodBiasDirectional=2.0`, `r.Shadow.Virtual.ResolutionLodBiasDirectionalMoving=2.0`, `r.Shadow.Virtual.ResolutionLodBiasLocal=2.0`, `r.Shadow.Virtual.ResolutionLodBiasLocalMoving=2.0`, `r.Shadow.Virtual.Clipmap.FirstLevel=10`, `r.Shadow.Virtual.Clipmap.LastLevel=18`, and `r.Shadow.RadiusThreshold=0.06`.
- **Verification:** `vsmOverflowWarnings: 0` in both 4-world PIE (`Saved/Logs/pie-goal3/pie-goal3-result.json`) and 4-process packaged (`Saved/Logs/goal3-packaged-20261014/packaged-LAN-20261014-02-200ms-result.json`).
