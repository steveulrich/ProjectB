# Hero Game Feature Plugins

Per-hero content lives in optional Game Feature plugins under `Plugins/GameFeatures/Heroes/`.

## Why Separate Plugins

- Hot-reload hero content without touching BreakawayCore
- Experience `GameFeaturesToEnable` loads only roster heroes needed for a mode
- `UBwayGameFeatureData::HeroDataAssets` registers heroes with the asset system

## Template Structure

```
Heroes/
  Spartacus/
    Hero_Spartacus.uplugin
    CONTENT_SETUP.md
    Content/          (create in Editor)
      Characters/
      Abilities/
      Buildables/
  Morgan/
  Alona/
  Rawlins/
```

## Creating a Hero Plugin

1. Copy an existing `Hero_*.uplugin` folder
2. Rename FriendlyName / plugin Name
3. In Editor: add `UBwayGameFeatureData` asset pointing to `UBwayHeroDataAsset`
4. Add plugin to experience `GameFeaturesToEnable`
5. Register hero data under `/BreakawayCore/Characters/Heroes/` OR plugin content path (update AssetManager scan if needed)

## Vertical Slice Roster

Codename paths: [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md).

| Plugin | DisplayName | Codename folder | Gate | Buildables |
|--------|-------------|-----------------|------|------------|
| Hero_Spartacus | Spartacus | `Argus` | 1 | Fire Catapult, Dragon Spire |
| Hero_Alona | Alona | `Alona` | 2 | Sun Shrine, Starlight |
| Hero_Morgan | Morgan Le Fay | `Hexweaver` | 3 | Elder Stone, Tome of Frailty |
| Hero_Rawlins | Rawlins | `Gunslinger` | 4 | Cage, Boom Box |

## Loader

`UBwayHeroPluginLoader::LoadAllHeroPlugins` scans `/GameFeatures/Heroes/*` — ensure plugins are Registered/Loaded before hero select.

See [GAS_and_Abilities.md](../BreakawayCore/Docs/GAS_and_Abilities.md).
