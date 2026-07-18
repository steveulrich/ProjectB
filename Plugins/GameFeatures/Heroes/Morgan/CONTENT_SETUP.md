# Korryn (Hero_Morgan / Hexweaver) — Content Setup

**Codename folder:** `Hexweaver`. **DisplayName:** **Korryn** (official).  
Plugin name remains `Hero_Morgan`. See [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md).

## Assets

| Item | Path |
|------|------|
| Hero DA | `/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver` |
| Ability set | `/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_AbilitySet_Hexweaver` |
| Kit config | `/Hero_Morgan/Kit/DA_BW_KorrynKitConfig` |
| Buildable DA | `/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_BuildableData_Hexweaver` |
| Ward BP | `/Hero_Morgan/Characters/Heroes/Hexweaver/Buildable/BP_BW_Hexweaver_Buildable` |
| PlaceBuildable BP | `/Hero_Morgan/Characters/Heroes/Hexweaver/Abilities/GA_BW_Korryn_BuildablePlacement` |
| GFD | `/Hero_Morgan/Hero_Morgan` |

## Step 20a (functional)

Native kit lives in `HeroMorganRuntime`:

| Slot | Input | Class |
|------|-------|-------|
| LMB | Primary | `BwayGameplayAbility_KorrynPrimary` |
| F | Ability4 | `BwayGameplayAbility_KorrynFlock` |
| Q | Ability1 | `BwayGameplayAbility_KorrynBurdenOfSin` |
| E | Ability2 | `BwayGameplayAbility_KorrynCircleOfSpite` |
| R | Ability3 | `BwayGameplayAbility_KorrynAuraOfSilence` |

Automated wiring:

```
node Scripts/setup-korryn-20a-functional.mjs
node Scripts/setup-korryn-20a-functional.mjs --probe-only
```

Full checklist: [Korryn_20a_Editor_Setup.md](../../BreakawayCore/Docs/Korryn_20a_Editor_Setup.md).

## Step 20b–20c (Cursed Ward + parity)

| Item | Value |
|------|--------|
| Actor | `ABwayKorrynCursedWardBuildable` |
| HP / radius / slow | 600 / 600 uu (6 m) / ×0.5 |
| Placement | once/round free; persists between rounds |
| Kit authority | cooldowns, damage, durations, radii, Ward fields on `DA_BW_KorrynKitConfig` |

```
node Scripts/setup-korryn-20b-cursed-ward.mjs
node Scripts/setup-korryn-20b-cursed-ward.mjs --probe-only
```

Full checklist: [Korryn_20b_20c_Editor_Setup.md](../../BreakawayCore/Docs/Korryn_20b_20c_Editor_Setup.md).

## Ini scan

`DefaultGame.ini` already scans `/Hero_Morgan/Characters/Heroes` as `HeroDataAsset` with `AssetBaseClass=/Script/Engine.PrimaryDataAsset`.  
Kit configs scan `/Hero_Morgan/Kit` as `BwayKorrynKitConfig` (same Engine PrimaryDataAsset base).
