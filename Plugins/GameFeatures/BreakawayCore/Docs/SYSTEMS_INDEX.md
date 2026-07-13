# BreakawayCore — Systems Documentation Index

Developer reference for the BreakawayCore Game Feature plugin. Slice planning and schedules live in [`AI_Planning/`](../../../AI_Planning/).

## Architecture Overview

BreakawayCore extends Lyra via C++ subclasses and `UGameStateComponent`s. Match flow is experience-driven; domain logic lives on `ABwayGameState` components.

```
Lyra FrontEnd → Experience (B_BW_Experience_Dev / CaptureTheRelic) → ABwayGameState + components → match loop
```

**Living plan:** [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) (Section 3 = heroes + buildables).

## System Documents

| System | Document |
|--------|----------|
| Lyra integration | [Lyra_Integration.md](./Lyra_Integration.md) |
| Match flow & phases | [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md) |
| Core loop plan | [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) |
| Relic | [Relic_System.md](./Relic_System.md) |
| Buildables | [Buildable_System.md](./Buildable_System.md) |
| Economy / gold | [Economy_Gold.md](./Economy_Gold.md) |
| GAS & abilities | [GAS_and_Abilities.md](./GAS_and_Abilities.md) |
| UI | [UI_System.md](./UI_System.md) |
| Networking | [Networking.md](./Networking.md) |
| Spawn system | [SpawnSystem_README.md](./SpawnSystem_README.md) |
| Hero selection | [CharacterSelect_Integration.md](./CharacterSelect_Integration.md) |
| Hero select staging | [HeroSelect_Staging_Setup.md](./HeroSelect_Staging_Setup.md) |
| Argus / Spartacus notes | [Spartacus_Implementation_Summary.md](./Spartacus_Implementation_Summary.md) |
| Argus 18a editor | [Argus_18a_Editor_Setup.md](./Argus_18a_Editor_Setup.md) |
| Argus 18b editor (placement) | [Argus_18b_Editor_Setup.md](./Argus_18b_Editor_Setup.md) |
| Agent testing | [Agent_Testing_Ladder.md](./Agent_Testing_Ladder.md) |

## Core C++ Entry Points

| Class | Role |
|-------|------|
| `ABreakawayGameMode` | Player lifecycle, team assign, hero apply, initial spawns |
| `ABwayGameState` | Teams, match flow delegates, hosts all match components |
| `ABwayPlayerState` | Hero selection, relic flags, K/D/A, gold ASC, once-per-round buildable flag |
| `ABwayPlayerController` | Hero/results UI RPCs, cheat manager |
| `ABwayCharacterWithAbilities` | In-match pawn, hero init, relic pickup |

## GameState Components (C++ default subobjects)

| Component | Role |
|-----------|------|
| `UBwayRoundManagementComponent` | Rounds, win conditions, gold awards |
| `UBwayScoringComponent` | Team scores |
| `UBwayRelicManagerComponent` | Relic spawn/reset/carrier |
| `UBwayTeamBridgeComponent` | Sync to `ULyraTeamSubsystem` |
| `UBwayBuildableRegistryComponent` | Match-wide buildable tracking |
| `UBwayHeroSelectionManager` | Pick/lock rules |
| `UBwayHeroSelectionPhaseComponent` | Phase + UI orchestration |
| `UBwayBotCreationComponent` | Bot backfill to 8 players |
| `UBwaySpawnPointManagerComponent` | Added at runtime by GameMode |

## Config & Tags

- [`Config/Tags/BreakawayCore.ini`](../Config/Tags/BreakawayCore.ini)
- [`Config/DefaultGame.ini`](../../../Config/DefaultGame.ini) — AssetManager scans for `/BreakawayCore/` and hero plugin paths

## Asset Audit

Editor checklist: [`AI_Planning/BLUEPRINT_ASSET_AUDIT.md`](../../../AI_Planning/BLUEPRINT_ASSET_AUDIT.md)

## Known Tech Debt

- Duplicate team/score paths on `ABwayGameState` vs components (consolidate over time)
- `PlayingPhaseTag` defaults to `ShooterGame.GamePhase.Playing`
- Asset management uses `ULyraAssetManager` (not a custom Bway asset manager)
