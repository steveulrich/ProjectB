# DevMap playtest guide

End-to-end smoke test for Capture-the-Relic on **`L_BW_DevMap`**.

**Last reviewed:** August 19, 2026  
**Primary experience:** `B_BW_Experience_Dev` (core-loop harness). Playlist experience `B_BW_Experience_CaptureTheRelic` is the front-end tile path.

## Prerequisites

- C++ compiled (BreakawayCore changes)
- Editor assets per [Blueprint asset audit](./BLUEPRINT_ASSET_AUDIT.md) (audit is stale; confirm in Editor)
- For more information about URLs, see [Core loop implementation plan](../Plugins/GameFeatures/BreakawayCore/Docs/CoreLoop_Implementation_Plan.md)

## C++ enablers (no extra GameState components required)

`ABwayGameState` creates by default:

- RoundManagement, Scoring, RelicManager, TeamBridge, BuildableRegistry
- Hero selection + bot creation

Bots scale to **8 players** when `bScaleBotsToTargetPlayerCount` is true.

## Launch steps

1. PIE as **Listen Server**, 1 player, on `L_BW_DevMap`.
2. Humanoid regression: `?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&ForceHumanoid=1&NumBots=7`
3. Hero test: `?Experience=B_BW_Experience_Dev&Hero=Argus&NumBots=7`
4. Optional: travel from the Lyra front-end playlist (staging path).

## Cheat shortcuts

Console (authority):

- `ShowHeroSelect` — force hero UI
- `SelectHero Argus` / `LockHero` — pick by primary asset name (not DisplayName)
- See `UBwayCheatManager` for the full list

## Verify checklist

| Step | Expected |
|------|----------|
| Hero select | UI shows heroes from the registry (or `Hero=` skips UI) |
| Round start | Relic spawns at `SpawnPoint.Relic` |
| Goal score | Round ends, score increments |
| Round 2 | Gold unchanged on PlayerState; persist-flag buildables remain |
| Match end | Results screen; front-end travel is `11-8` (still deferred) |

## Gold persistence test

1. Note gold after Round 1.
2. Start Round 2 — gold should be **≥ Round 1** (not reset).
3. New match — gold resets (no SaveGame).

Gold is **not** spent on buildables in the slice.

## Buildable persistence test

1. Place the hero buildable (key **1**) in Round 1.
2. End round (score or timer).
3. Structure still in world at Round 2; second placement blocked until the next round.

## Failure triage

| Symptom | Check |
|---------|-------|
| No relic | Spawn points tagged; RelicManager RelicClass |
| No rounds | Phase `Playing` active; RoundManagement PlayingPhaseTag |
| No bots | BotCreationComponent TargetPlayerCount |
| No heroes | Hero data assets on the hero plugin mounts plus `GameFeaturesToEnable` |

## Related

- [Planning docs](./README.md)
- [Heroes README](../Plugins/GameFeatures/Heroes/README.md)
- [Relic system](../Plugins/GameFeatures/BreakawayCore/Docs/Relic_System.md)
