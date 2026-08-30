# BreakawayCore systems documentation index

Developer reference for the BreakawayCore Game Feature plugin. Slice planning lives in [`AI_Planning/`](../../../AI_Planning/) — start with [the planning hub](../../../AI_Planning/README.md).

**Last reviewed:** August 27, 2026

## Architecture overview

BreakawayCore extends Lyra via C++ subclasses and `UGameStateComponent`s. Match flow is experience-driven; domain logic lives on `ABwayGameState` components.

```
Lyra FrontEnd → Experience (B_BW_Experience_Dev / CaptureTheRelic) → ABwayGameState + components → match loop
```

**Living implementation plan:** [Core loop implementation plan](./CoreLoop_Implementation_Plan.md) (Section 3 = heroes + buildables). **Current next gate:** Step 22 editor PIE (C++ fumble landed).

Four slice heroes (Argus, Alona, Korryn, Rawlins) ship as Game Feature plugins under `Plugins/GameFeatures/Heroes/`.

## System documents

| System | Document |
|--------|----------|
| Lyra integration | [Lyra_Integration.md](./Lyra_Integration.md) |
| Match flow and phases | [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md) |
| Core loop plan | [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) |
| Relic | [Relic_System.md](./Relic_System.md) |
| Buildables | [Buildable_System.md](./Buildable_System.md) |
| Economy / gold | [Economy_Gold.md](./Economy_Gold.md) |
| GAS and abilities | [GAS_and_Abilities.md](./GAS_and_Abilities.md) |
| UI | [UI_System.md](./UI_System.md) |
| Networking | [Networking.md](./Networking.md) |
| Spawn system | [SpawnSystem_README.md](./SpawnSystem_README.md) |
| Hero selection | [CharacterSelect_Integration.md](./CharacterSelect_Integration.md) |
| Hero select staging | [HeroSelect_Staging_Setup.md](./HeroSelect_Staging_Setup.md) |
| Argus / Spartacus notes | [Spartacus_Implementation_Summary.md](./Spartacus_Implementation_Summary.md) |
| Argus 18a / 18b / 18c editor | [Argus_18a](./Argus_18a_Editor_Setup.md) · [18b](./Argus_18b_Editor_Setup.md) · [18c](./Argus_18c_Editor_Setup.md) |
| Alona | [Alona_Implementation_Summary.md](./Alona_Implementation_Summary.md) · [19a](./Alona_19a_Editor_Setup.md) · [19b](./Alona_19b_Editor_Setup.md) |
| Korryn | [20a](./Korryn_20a_Editor_Setup.md) · [20b/20c](./Korryn_20b_20c_Editor_Setup.md) |
| Rawlins | [21a](./Rawlins_21a_Editor_Setup.md) · [21b](./Rawlins_21b_Editor_Setup.md) · [21c](./Rawlins_21c_Editor_Setup.md) |
| Kit configs (Step 20.5) | [Step20_5_KitConfig_Editor_Setup.md](./Step20_5_KitConfig_Editor_Setup.md) |
| Match HUD layout | [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md) · [MatchUI_Setup.md](./MatchUI_Setup.md) |
| Agent testing | [Agent_Testing_Ladder.md](./Agent_Testing_Ladder.md) |

## Core C++ entry points

| Class | Role |
|-------|------|
| `ABreakawayGameMode` | Player lifecycle, team assign, hero apply, initial spawns |
| `ABwayGameState` | Teams, match flow delegates, hosts all match components |
| `ABwayPlayerState` | Hero selection, relic flags, K/D/A, gold ASC, once-per-round buildable flag |
| `ABwayPlayerController` | Hero/results UI RPCs, cheat manager |
| `ABwayCharacterWithAbilities` | In-match pawn, hero init, relic pickup |

## GameState components (C++ default subobjects)

| Component | Role |
|-----------|------|
| `UBwayRoundManagementComponent` | Rounds, win conditions, gold awards |
| `UBwayScoringComponent` | Team scores |
| `UBwayRelicManagerComponent` | Relic spawn/reset/carrier, fumble-on-damage |
| `UBwayTeamBridgeComponent` | Sync to `ULyraTeamSubsystem` |
| `UBwayBuildableRegistryComponent` | Match-wide buildable tracking |
| `UBwayHeroSelectionManager` | Pick/lock rules |
| `UBwayHeroSelectionPhaseComponent` | Phase + UI orchestration |
| `UBwayBotCreationComponent` | Bot backfill to 8 players |
| `UBwaySpawnPointManagerComponent` | Added at runtime by GameMode |

## Config and tags

- [`Config/Tags/BreakawayCore.ini`](../Config/Tags/BreakawayCore.ini)
- [`Config/DefaultGame.ini`](../../../Config/DefaultGame.ini) — AssetManager scans for `/BreakawayCore/` and hero plugin paths

## Asset audit

Editor checklist: [`AI_Planning/BLUEPRINT_ASSET_AUDIT.md`](../../../AI_Planning/BLUEPRINT_ASSET_AUDIT.md) (last Editor-verified May 25, 2026 — re-run after Step 22).

## Known tech debt

- Duplicate team/score paths on `ABwayGameState` vs components (consolidate over time)
- `PlayingPhaseTag` defaults to `ShooterGame.GamePhase.Playing`
- Asset management uses `ULyraAssetManager` (not a custom Bway asset manager)
