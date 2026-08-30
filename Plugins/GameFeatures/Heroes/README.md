# Hero Game Feature plugins

Per-hero content lives in optional Game Feature plugins under `Plugins/GameFeatures/Heroes/`.

**Last reviewed:** August 19, 2026  
**Status and next work:** [Project roadmap](../../../AI_Planning/PROJECT_ROADMAP.md)  
**Codenames:** [Hero codename map](../../../AI_Planning/HERO_CODENAME_MAP.md)

## Why separate plugins

- Hot-reload hero content without touching BreakawayCore
- Experience `GameFeaturesToEnable` loads only roster heroes needed for a mode
- `UBwayGameFeatureData::HeroDataAssets` registers heroes with the asset system

## Template Structure

```
Heroes/
  Spartacus/          # Argus (DisplayName); folder codename Argus
    Hero_Spartacus.uplugin
    CONTENT_SETUP.md
    Content/
      Characters/Heroes/Argus/
  Alona/
  Morgan/             # Korryn; folder Hexweaver
  Rawlins/            # folder Gunslinger
```

## Creating a Hero Plugin

1. Copy an existing `Hero_*.uplugin` folder
2. Rename FriendlyName / plugin Name
3. In Editor: add `UBwayGameFeatureData` asset pointing to `UBwayHeroDataAsset`
4. Add plugin to experience `GameFeaturesToEnable` (one hero at a time during Section 3)
5. Register hero scan path in `Config/DefaultGame.ini` (`HeroDataAsset` directories)

## Vertical Slice Roster (Section 3)

**Authority:** [CoreLoop_Implementation_Plan.md](../BreakawayCore/Docs/CoreLoop_Implementation_Plan.md) + [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md).  
Codenames / paths: [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md).

| Plugin | DisplayName (UI) | Codename folder | Buildable (one) |
|--------|------------------|-----------------|-----------------|
| Hero_Spartacus | **Argus** | `Argus` | Siege Engine |
| Hero_Alona | Alona | `Alona` | Sun Shrine |
| Hero_Morgan | **Korryn** | `Hexweaver` | Cursed Ward |
| Hero_Rawlins | Rawlins | `Gunslinger` | Jail |

**Rules:** one buildable per hero; once per round; free; persists between rounds. PlaceBuildable is **per-hero**; Confirm/Cancel are **humanoid**.

## Current focus

**Step 22 capstone** — fumble-on-damage C++ landed. Enable all four plugins with `Scripts/setup-step22-capstone.mjs`, then PIE staging E2E / four-hero match. See [Project roadmap](../../../AI_Planning/PROJECT_ROADMAP.md).

## Loader

Ensure plugins are Registered/Enabled before hero select. Prefer experience `GameFeaturesToEnable` for Section 3.

See [GAS_and_Abilities.md](../BreakawayCore/Docs/GAS_and_Abilities.md).
