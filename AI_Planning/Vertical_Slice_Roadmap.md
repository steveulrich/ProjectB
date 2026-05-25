# Breakaway: Reborn - Vertical Slice Roadmap

> **Scope:** [VERTICAL_SLICE_DEFINITION.md](./VERTICAL_SLICE_DEFINITION.md)  
> **Systems detail:** [BreakawayCore/Docs/SYSTEMS_INDEX.md](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md)

## Current State (2026-05-25)

- **C++ match loop:** Round/score/relic/teams on GameState components (default subobjects)
- **Buildables:** Persistence flag + `UBwayBuildableRegistryComponent`
- **4v4 bots:** Auto-scale to 8 players
- **Relic carrier:** Combat abilities blocked via `Gameplay.State.RelicCarrier`
- **Docs:** Dual-location dev + planning docs synced
- **Content gap:** Editor assets not in git — use [BLUEPRINT_ASSET_AUDIT.md](./BLUEPRINT_ASSET_AUDIT.md)

## Priority Order (Design-Spec Slice)

### 1. Playable Loop (DevMap)
- [x] C++ GameState components wired
- [ ] Experience + map assets verified in Editor
- [ ] End-to-end PIE — [DEVMAP_PLAYTEST_GUIDE.md](./DEVMAP_PLAYTEST_GUIDE.md)

### 2. Spartacus Content
- [x] C++ abilities
- [x] Relic carrier restrictions
- [ ] BP ability assets, mesh, 2 buildables — [Heroes/Spartacus/CONTENT_SETUP.md](../Plugins/GameFeatures/Heroes/Spartacus/CONTENT_SETUP.md)

### 3. Heroes 2–4
- [x] GF plugin templates (Morgan, Alona, Rawlins)
- [ ] Ability + buildable content per hero CONTENT_SETUP.md

### 4. Relic & Net Polish
- [x] Scoring cooldown, LastPossessingTeam, movement replication component
- [ ] 200ms latency playtest
- [ ] Fumble-on-damage (design spec)

### 5. Buildables & Between-Round UI
- [x] Registry, planning delegate `OnBetweenRoundPlanningStarted`
- [ ] BP planning/shop widget
- [ ] 8 buildable data assets

### 6. Dorado Parity + Polish
- [ ] Mirror DevMap spawn/goals on L_BW_Dorado
- [ ] HUD/scoreboard BP polish
- [ ] Slide juice (FOV/VFX/audio) — post-slice gate optional

## Explicitly Deferred

- Ascension Rites campaign
- SaveGame / meta gold
- Production art pass

## Technical Debt

- Consolidate duplicate GameState vs component state
- Migrate off ShooterCTF spawn tag coupling
- Slide net steering under latency
