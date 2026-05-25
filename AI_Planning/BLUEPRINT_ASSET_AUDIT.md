# Breakaway Blueprint & Content Asset Audit

> **Purpose:** Track Editor-only assets required for the design-spec vertical slice. `.uasset` files are not in git; this document is filled from **ue-mcp live Editor reads**, config references, and C++ defaults.
>
> **Last audited (code/config):** 2026-05-25  
> **Last verified (Editor via ue-mcp):** 2026-05-25 — ProjectB loaded, bridge connected  
> **Raw MCP output:** `AI_Planning/ue_mcp_audit_raw.json`, `AI_Planning/ue_mcp_audit_extra.json`, `AI_Planning/ue_mcp_audit_summary.json`

---

## Audit Summary

| Category | Referenced in config/code | In git | Editor verified |
|----------|---------------------------|--------|-----------------|
| Maps | 4+ | No | **Yes** — DevMap, Dorado, FrontEnd, HeroSelect staging |
| Experiences | 2 + phases | No | **Yes** — both experiences + 5 phase abilities |
| GameMode/GameState BPs | Yes | No | **Partial** — BPs exist; CDO class overrides not readable via MCP |
| Hero data assets | 4 expected | No | **Partial** — 2 found (Argus, Alona); Morgan/Rawlins missing |
| UI widgets | 6+ | No | **Partial** — Core HUD, HeroSelect, Results; no Scoreboard/PauseMenu assets |
| Hero GF plugins | 4 expected | `.uplugin` templates only | **No content** — plugins in git, no `/BreakawayCore` hero GF content |

**C++ note:** `ABwayGameState` creates Round, Scoring, Relic, TeamBridge, and BuildableRegistry components in C++ — `BP_BW_GameState` should not duplicate them. ue-mcp `blueprint read` lists the four round/relic C++ subobjects on the BP instance tree; HeroSelection/Bot/Registry come from C++ unless explicitly overridden in BP.

**MCP limitation:** `asset read_properties` on Blueprint **CDO paths** (`*_C`) returns “Asset not found”. Experience/GameMode **class defaults** (GameFeaturesToEnable on the experience asset itself, GameStateClass override on `_Default`) were inferred from `blueprint get_dependencies` + child action sets, not direct CDO reads.

---

## 1. Experience & Game Mode

### `B_BW_Experience_CaptureTheRelic`
**Path:** `/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic`

| Field | Expected value | Verified |
|-------|----------------|----------|
| Exists | Yes | [x] Parent `LyraExperienceDefinition`, `BPTYPE_Const`, up to date |
| GameFeaturesToEnable | `BreakawayCore` (+ hero plugins when ready) | [~] `LAS_BW_SharedInput` enables `BreakawayCore`; experience CDO not directly readable |
| Default GameMode | `B_BWayGameMode_Default` or `ABreakawayGameMode` | [~] Experience depends on `BP_BW_GameState`, ball-mode scoring, bot spawner — wired for capture-the-relic; explicit GameMode class not readable |
| Default Pawn Data | `DA_BW_PawnData_Humanoid` → `B_BW_Character_Humanoid` | [x] Listed in experience dependency graph |
| Action Sets | Input (`LAS_BW_SharedInput`), HUD, ShooterCore | [x] Depends on `LAS_BW_SharedInput`, `EAS_BW_CaptureTheRelic`, `LAS_ShooterGame_StandardHUD`, `W_BW_CaptureTheRelic_ScoreWidget`, `W_BW_RelicStatusWidget` |
| GameState override | `BP_BW_GameState` | [x] Direct dependency |
| Phase abilities | See section 2 | [x] All five `BW_Phase_*` assets exist |

**Dependency highlights (ue-mcp):** `BP_BW_GameState`, `DA_BW_PawnData_Humanoid`, `LAS_BW_SharedInput`, `EAS_BW_CaptureTheRelic`, `B_BW_CaptureTheRelic_Scoring`, `B_BW_BotSpawner_BallMode`, `B_BW_TeamSetup_TwoTeams`, ShooterCore standard HUD/music action sets.

### `B_BW_Experience_Dev`
**Path:** `/BreakawayCore/Experiences/B_BW_Experience_Dev` — dev/test variant | [x] exists, parent `LyraExperienceDefinition` |

### GameMode BP: `B_BWayGameMode` / `B_BWayGameMode_Default`

| Setting | Expected | Verified |
|---------|----------|----------|
| `B_BWayGameMode` parent | `ABreakawayGameMode` | [x] Parent `BreakawayGameMode` |
| `B_BWayGameMode_Default` parent | `B_BWayGameMode` | [x] Parent `B_BWayGameMode_C` |
| GameState class | `BP_BW_GameState` or `ABwayGameState` | [~] C++ `BreakawayGameMode` defaults to `ABwayGameState`; `_Default` depends on `BP_BW_GameState` (override likely on BP, not confirmed via CDO) |
| PlayerState class | `BP_BW_PlayerState` or `ABwayPlayerState` | [~] C++ defaults `ABwayPlayerState`; `BP_BW_PlayerState` exists but parent is **`LyraPlayerState`** (not `ABwayPlayerState`) |
| PlayerController class | `BP_BW_PlayerController` or `ABwayPlayerController` | [~] C++ defaults `ABwayPlayerController`; **no** `BP_BW_PlayerController` asset found under `/BreakawayCore` |
| Team spawn tags | `Team1Spawn` / `Team2Spawn` (or spawn point actors) | [x] Maps use `LyraPlayerStart` ×12 + `BwaySpawnPoint` goal/relic markers (see maps) |

### GameState BP: `BP_BW_GameState`
**Path:** `/BreakawayCore/GameModes/BP_BW_GameState`

| Component | Required | C++ default subobject? | Verified |
|-----------|----------|----------------------|----------|
| `UBwayHeroSelectionManager` | Yes | Yes | [~] Not listed on BP component tree; expected from C++ `ABwayGameState` |
| `UBwayHeroSelectionPhaseComponent` | Yes | Yes | [x] BP uses `BP_BW_HeroSelectionPhase` component class override |
| `UBwayBotCreationComponent` | Yes | Yes | [~] C++ only (not on BP tree) |
| `UBwayRoundManagementComponent` | Yes | **Yes (C++)** | [x] On BP tree |
| `UBwayScoringComponent` | Yes | **Yes (C++)** | [x] On BP tree |
| `UBwayRelicManagerComponent` | Yes | **Yes (C++)** | [x] On BP tree |
| `UBwayTeamBridgeComponent` | Yes | **Yes (C++)** | [x] On BP tree |
| `UBwayBuildableRegistryComponent` | Recommended | **Yes (C++)** | [~] C++ only; recompile required for latest C++ |

**BP dependencies:** `WBP_BW_ResultsWidget`, `L_BW_FrontEnd`, `BP_BW_RelicActor`, `DA_BW_RelicSettings_Default`, `BW_Phase_PostGame`.

---

## 2. Game Phase Abilities

All five assets exist under `/BreakawayCore/Experiences/Phases/`, parent `LyraGamePhaseAbility`, compile status up to date.

| Asset | Tag (expected) | Verified |
|-------|----------------|----------|
| `BW_Phase_Warmup` | `Breakaway.GamePhase.Warmup` | [x] Asset exists; tag on CDO not exposed by MCP (registered in `BreakawayCore.ini`) |
| `BW_Phase_HeroSelection` | `Breakaway.GamePhase.HeroSelection` | [x] Asset exists |
| `BW_Phase_Playing` | `Breakaway.GamePhase.Playing` or `ShooterGame.GamePhase.Playing` | [x] Asset exists — **note:** `BwayRoundManagementComponent` still listens for `ShooterGame.GamePhase.Playing` in C++ |
| `BW_Phase_PostRound` | `Breakaway.GamePhase.PostRound` | [x] Asset exists |
| `BW_Phase_PostGame` | `Breakaway.GamePhase.PostGame` | [x] Asset exists |

**Code fallback:** `BwayHeroSelectionPhaseComponent` hardcodes `/BreakawayCore/Experiences/Phases/BW_Phase_Warmup` as next phase.

---

## 3. Maps

### `L_BW_DevMap` (integration / CI)
**Path:** `/BreakawayCore/Maps/L_BW_DevMap`

| Requirement | Verified |
|-------------|----------|
| Map exists | [x] 36 actors in outliner |
| Experience loads this map on travel | [ ] Not verified via MCP (check playlist / `DefaultGame.ini`) |
| Player spawns | [x] **12× `LyraPlayerStart`** (6 per side layout) |
| `ABwaySpawnPoint` — relic | [x] **1×** label `SP_Relic` |
| `ABwaySpawnPoint` — goals | [x] **2×** labels `SP_GoalTeam1`, `SP_GoalTeam2` (used instead of `ABwayGoalVolume`) |
| `ABwayGoalVolume` actors | [ ] **None** in outliner — scoring may use spawn-point tags / C++ lookup |
| Nav mesh | [x] `NavMeshBoundsVolume` + `RecastNavMesh` |

### `L_BW_Dorado` (demo)
**Path:** `/BreakawayCore/Maps/L_BW_Dorado`

| Requirement | Verified |
|-------------|----------|
| Map exists | [x] 78 actors (full art pass) |
| Player spawns | [x] **12× `LyraPlayerStart`** |
| Relic / goal spawn points | [x] Same **3× `BwaySpawnPoint`** labels as DevMap (`SP_Relic`, `SP_GoalTeam1`, `SP_GoalTeam2`) |
| Nav mesh | [x] `NavMeshBoundsVolume` + `RecastNavMesh` |

### Other maps (ue-mcp asset search)
- [x] `/BreakawayCore/Maps/L_BW_FrontEnd`
- [x] `/BreakawayCore/Maps/L_BW_HeroSelect_Staging`

### Parity checklist (DevMap ↔ Dorado)
- [x] Same `BwaySpawnPoint` count and labels (3 each)
- [x] Same `LyraPlayerStart` count (12 each)
- [ ] Goal **volume** parity — neither map has `ABwayGoalVolume`; both use labeled spawn points
- [ ] Same experience URL / playlist entry — not verified via MCP

---

## 4. Heroes (4 for vertical slice)

**Editor scan:** Only **2** `BwayHeroDataAsset` instances under `/BreakawayCore/Characters/Heroes/`.

| Hero (slice) | Data asset (expected) | Editor asset | Ability sets | Buildables | GF plugin |
|--------------|----------------------|--------------|--------------|------------|-----------|
| Spartacus | `DA_BW_Hero_Spartacus` | **`DA_BW_HeroData_Argus`** (display name “Argus”) | [x] `DA_BW_AbilitySet_Argus` + UI display infos | [ ] Legacy single `BuildableDataAsset`; `BuildableDataAssets[]` **empty**; buildable DA uses **pistol placeholder mesh** | Git template `Hero_Spartacus` — **no editor content** |
| Morgan Le Fay | `DA_BW_Hero_Morgan` | **Missing** (0 search hits) | [ ] | [ ] | Git template `Hero_Morgan` |
| Alona | `DA_BW_Hero_Alona` | [x] `DA_BW_HeroData_Alona` | [x] `DA_BW_AbilitySet_Alona` | [~] `DA_BW_BuildableData_Alona` exists but **`BuildableActorClass` = None**, mesh None; BP buildable actor exists | Git template `Hero_Alona` |
| Rawlins | `DA_BW_Hero_Rawlins` | **Missing** (0 search hits) | [ ] | [ ] | Git template `Hero_Rawlins` |

**Data issues (ue-mcp `read_properties`):**
- **Argus/Spartacus:** `HeroMesh` points to **`SKM_BW_alona`** (wrong skeleton); rename/migration to Spartacus naming still pending.
- **Argus/Spartacus:** `BuildableDataAssets` array empty — C++ `GetAllBuildableDataAssets()` will only see legacy single field until populated.
- **Alona:** `AbilityDisplayInfos` empty (hero select UI may look sparse).

**Buildable content scan:** Alona has `BP_BW_Alona_Buildable` + meshes; Argus has icons + `DA_BW_BuildableData_Argus` only (no dedicated buildable BP in hero folder search).

---

## 5. UI Widgets

| Widget | Path (actual) | Parent C++ class | Verified |
|--------|---------------|------------------|----------|
| `WBP_BW_HeroSelect` | `/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect` | `UBwayHeroSelectWidget` | [x] |
| `WBP_BW_CoreHUD` | **`/BreakawayCore/UI/Match/WBP_BW_CoreHUD`** (not `/UI/` root) | `UBwayCoreHUDWidget` | [x] |
| Score / team widget | **`/BreakawayCore/UI/Match/W_BW_CaptureTheRelic_ScoreWidget`** | (Lyra/Shooter widget) | [x] Experience dependency |
| Relic status | **`/BreakawayCore/UI/Match/W_BW_RelicStatusWidget`** | — | [x] Experience dependency |
| `WBP_BW_ResultsWidget` | `/BreakawayCore/UI/WBP_BW_ResultsWidget` | `UBwayResultsScreenWidget` | [x] *(audit template name `WBP_BW_ResultsScreen` is outdated)* |
| `WBP_BW_Scoreboard` | `/BreakawayCore/UI/` | `UBwayScoreboardWidget` | [ ] **Not found** — search 0 results |
| `WBP_BW_PauseMenu` | `/BreakawayCore/UI/` | `UBwayPauseMenuWidget` | [ ] **Not found** |
| Currency display | `/BreakawayCore/UI/HeroSelect/WBP_BW_CurrencyDisplayWidget` | — | [x] |

**Also present:** `WBP_BW_FrontEnd`, hero-select sub-widgets (timer, preview, ability display, player slot, etc.) — 21 widgets matching `WBP_BW` under `/BreakawayCore/UI`.

**Code reference:** `BwayCheatManager` loads `WBP_BW_HeroSelect` from `/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect`.

---

## 6. Relic & Buildables

| Asset | Path | Verified |
|-------|------|----------|
| `URelicSettings` data asset | `/BreakawayCore/GameModes/BallMode/Relic/DA_BW_RelicSettings_Default` | [x] |
| Relic mesh/material | `SM_BW_Relic`, `MT_BW_Relic` | [x] |
| Relic VFX (idle/carried/dropped/scoring) | In RelicSettings | [ ] All **`None`** |
| Relic audio (pickup/drop/scoring/reset) | In RelicSettings | [ ] All **`None`** |
| Relic carrier ability set | `AS_BW_BallAbilities` | [x] |
| Carrier / recent-carrier GEs | `GE_BW_RelicCarrier`, `GE_BW_Relic_RecentCarrier` | [x] |
| Buildable data assets (8 target) | Per hero `BuildableDataAssets[]` | [ ] **2** DAs only; both incomplete for slice |
| Placement ability GAs | Per hero ability sets | [~] Argus ability set exists; full placement loop not verified |

**Pawn defaults:** `DA_BW_PawnData_Humanoid` → `B_BW_Character_Humanoid`, `DA_BW_AbilitySet_Humanoid`, `DA_BW_InputData_Humanoid` — [x]

---

## 7. Lyra Base Assets Still Used

| Asset | Path | Role |
|-------|------|------|
| Front-end experience | `/Game/System/FrontEnd/B_LyraFrontEnd_Experience` | Main menu |
| Front-end map | `/Game/System/FrontEnd/Maps/L_LyraFrontEnd` | Default map |
| Global game mode | `/Game/B_LyraGameMode` | Engine default (overridden by experience) |
| UI policy | `/Game/UI/B_LyraUIPolicy` | CommonUI routing |
| ShooterCore HUD action set | `/ShooterCore/Experiences/LAS_ShooterGame_StandardHUD` | Referenced by capture experience |
| Mannequin anim BP | `/Game/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base` | Used on hero data assets |

---

## 8. Editor Verification Steps

1. Enable **Show Plugin Content** → `BreakawayCore`
2. Open Class Viewer — confirm all `Bway*` C++ classes compile (**recompile** if C++ audit changes from 2026-05-25 are not yet built)
3. Open `B_BW_Experience_CaptureTheRelic` — confirm GameMode, pawn data, action sets match section 1 (manual — MCP cannot read experience CDO fields)
4. PIE on `L_BW_DevMap` with `?Experience=B_BW_Experience_CaptureTheRelic`
5. Confirm hero select → round start → relic spawn → goal score → results → front-end travel
6. Fix hero data: rename Argus → Spartacus, add Morgan/Rawlins DAs, fill `BuildableDataAssets[]`, fix Argus mesh

**Automated re-run:** `node Scripts/audit-breakaway-assets.mjs` (+ `audit-breakaway-assets-extra.mjs`) with Editor open and ue-mcp bridge connected.

---

## 9. Known Gaps (ue-mcp + code review)

| Gap | Severity |
|-----|----------|
| Only **2/4** hero data assets in Editor; Argus used as Spartacus stand-in | **Blocker** for 4-hero slice |
| Morgan / Rawlins — zero assets under `/BreakawayCore` | **Blocker** |
| Hero GF plugins — git templates only, no loaded GF content | **High** |
| `BuildableDataAssets[]` empty on both heroes; buildable DAs incomplete | **High** |
| Argus hero mesh references Alona skeleton | **High** |
| No `WBP_BW_Scoreboard` / `WBP_BW_PauseMenu` | **Medium** — may use ShooterCore/Lyra widgets instead |
| `BP_BW_PlayerState` parent is `LyraPlayerState`, not `ABwayPlayerState` | **Medium** — verify gold/stats still work |
| No `BP_BW_PlayerController`; relies on C++ `ABwayPlayerController` | **Low** if C++ is sufficient |
| Relic VFX/audio unset in `DA_BW_RelicSettings_Default` | **Medium** polish |
| Maps use **`BwaySpawnPoint` goal labels**, not `ABwayGoalVolume` | **Low** if C++ supports spawn-point goals |
| `ShooterGame.GamePhase.Playing` vs `Breakaway.GamePhase.Playing` mismatch in round management | **Medium** — align phase ability tags or C++ listener |
| Experience / GameMode CDO fields not readable via current ue-mcp API | **Process** — use Editor or extend MCP |
| `UBwayAssetManager` referenced in old docs — **does not exist**; use `ULyraAssetManager` | Doc only |
| Duplicate team/score state on `ABwayGameState` vs components | Tech debt |

---

## Related Docs

- [BreakawayCore/Docs/SYSTEMS_INDEX.md](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md)
- [VERTICAL_SLICE_DEFINITION.md](./VERTICAL_SLICE_DEFINITION.md)
- [BLUEPRINT_INTEGRATION_GUIDE.md](./BLUEPRINT_INTEGRATION_GUIDE.md)
