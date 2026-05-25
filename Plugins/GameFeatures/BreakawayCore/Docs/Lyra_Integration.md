# Lyra Integration

How BreakawayCore plugs into the Lyra Starter Project without replacing Lyra's foundation.

## Principle

**Lyra owns:** boot, sessions, experiences, GAS infrastructure, phases, teams subsystem, CommonUI, asset manager.

**BreakawayCore owns:** Capture-the-Relic rules, heroes, relic, buildables, round/score components.

## Inheritance Map

| Breakaway | Lyra base |
|-----------|-----------|
| `ABreakawayGameMode` | `ALyraGameMode` |
| `ABwayGameState` | `ALyraGameState` |
| `ABwayPlayerState` | `ALyraPlayerState` |
| `ABwayPlayerController` | `ALyraPlayerController` |
| `ABwayCharacterWithAbilities` | `ALyraCharacter` |
| `UBwayCharacterMovementComponent` | `ULyraCharacterMovementComponent` |
| `UBwayGameplayAbility_Base` | `ULyraGameplayAbility` |
| `UBwayGoldAttributeSet` | `ULyraAttributeSet` |
| `UBwayCheatManager` | `ULyraCheatManager` |

## Experience Pipeline

1. Player selects mode from Lyra front-end (`B_LyraFrontEnd_Experience`).
2. Travel URL includes `?Experience=B_BW_Experience_CaptureTheRelic`.
3. `ULyraExperienceManagerComponent` loads `ULyraExperienceDefinition`.
4. Experience enables Game Feature plugins and runs ActionSets (input, widgets, abilities).
5. `ABreakawayGameMode` receives players; `ABwayGameState` components drive match.

**Primary experience asset:** `/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic`

## Game Feature Plugin Loading

- BreakawayCore is **not** in `ProjectB.uproject`; it loads via Game Features (`ExplicitlyLoaded`, default Loaded).
- Depends on Lyra sample features: ShooterCore, ShooterCTF, ShooterExplorer (phase/spawn patterns).

## Team Bridge

`UBwayTeamBridgeComponent` maps `ABwayGameState::FTeamInfo` (indices 0/1) to `ULyraTeamSubsystem` (IDs 1/2) so Lyra HUD, cues, and team queries work.

## Game Phases

Lyra `ULyraGamePhaseSubsystem` drives input/ability gating via phase abilities (`ULyraGamePhaseAbility`).

| Phase asset (expected) | Tag |
|------------------------|-----|
| `BW_Phase_Warmup` | `Breakaway.GamePhase.Warmup` |
| `BW_Phase_HeroSelection` | `Breakaway.GamePhase.HeroSelection` |
| `BW_Phase_Playing` | `Breakaway.GamePhase.Playing` / `ShooterGame.GamePhase.Playing` |
| `BW_Phase_PostRound` | `Breakaway.GamePhase.PostRound` |
| `BW_Phase_PostGame` | `Breakaway.GamePhase.PostGame` |

`UBwayHeroSelectionPhaseComponent` blocks pawn spawn until hero selection completes.

## Asset Manager

Uses **`ULyraAssetManager`** with Breakaway paths registered in `Config/DefaultGame.ini`:

- Maps: `/BreakawayCore/Maps/`
- Experiences: `/BreakawayCore/Experiences/`
- Heroes: `/BreakawayCore/Characters/Heroes/` (`HeroDataAsset` primary type)

There is **no** `UBwayAssetManager` class.

## Config Cross-Reference

| File | Purpose |
|------|---------|
| `Config/DefaultEngine.ini` | Default map, game instance, global game mode |
| `Config/DefaultGame.ini` | GAS globals, asset scans, LyraGameFeaturePolicy |
| `Config/DefaultGameplayTags.ini` | Shared gameplay tags |
| `BreakawayCore/Config/Tags/BreakawayCore.ini` | Breakaway-specific tags |

## Blueprint Wiring Checklist

See [BLUEPRINT_ASSET_AUDIT.md](../../../AI_Planning/BLUEPRINT_ASSET_AUDIT.md).
