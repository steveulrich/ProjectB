# DevMap Playtest Guide (Phase 1)

End-to-end smoke test for Capture-the-Relic on **L_BW_DevMap**.

## Prerequisites

- C++ compiled (BreakawayCore changes)
- Editor assets per [BLUEPRINT_ASSET_AUDIT.md](./BLUEPRINT_ASSET_AUDIT.md)
- Experience: `B_BW_Experience_CaptureTheRelic`

## C++ Enablers (no BP GameState components required)

`ABwayGameState` now creates by default:

- RoundManagement, Scoring, RelicManager, TeamBridge, BuildableRegistry
- Hero selection + Bot creation

Bots scale to **8 players** when `bScaleBotsToTargetPlayerCount` is true.

## Launch Steps

1. PIE or listen server on `L_BW_DevMap`
2. URL option: `?Experience=B_BW_Experience_CaptureTheRelic`
3. Or travel from Lyra front-end playlist entry

## Cheat Shortcuts

Console (authority):

- `ShowHeroSelect` — force hero UI
- `SelectHero Spartacus` / `LockHero` — pick hero
- See `UBwayCheatManager` for full list

## Verify Checklist

| Step | Expected |
|------|----------|
| Hero select | UI shows heroes from registry |
| Round start | Relic spawns at `SpawnPoint.Relic` |
| Goal score | Round ends, score increments |
| Round 2 | Gold unchanged on PlayerState; buildables with persist flag remain |
| Match end | Results screen → front-end travel |

## Gold Persistence Test

1. Note gold after Round 1
2. Start Round 2 — gold should be **≥ Round 1** (not reset)
3. New match — gold resets (no SaveGame)

## Buildable Persistence Test

1. Place buildable with `bPersistsBetweenRounds=true`
2. End round (score or timer)
3. Structure still in world at Round 2

## Failure Triage

| Symptom | Check |
|---------|-------|
| No relic | Spawn points tagged; RelicManager RelicClass |
| No rounds | Phase `Playing` active; RoundManagement PlayingPhaseTag |
| No bots | BotCreationComponent TargetPlayerCount |
| No heroes | Hero data assets in `/BreakawayCore/Characters/Heroes/` |

## Next

- [CONTENT_SETUP Spartacus](../Plugins/GameFeatures/Heroes/Spartacus/CONTENT_SETUP.md)
- [Relic_System.md](../Plugins/GameFeatures/BreakawayCore/Docs/Relic_System.md)
