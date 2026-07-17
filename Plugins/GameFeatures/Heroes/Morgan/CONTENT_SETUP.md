# Korryn (Hero_Morgan / Hexweaver) — Content Setup

**Codename folder:** `Hexweaver`. **DisplayName:** **Korryn** (official).  
Plugin name remains `Hero_Morgan`. See [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md).

## Assets

| Item | Path |
|------|------|
| Hero DA | `/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver` |
| Ability set | `/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_AbilitySet_Hexweaver` |
| Kit config | `/Hero_Morgan/Kit/DA_BW_KorrynKitConfig` |
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

**Do not** grant PlaceBuildable / Cursed Ward in 20a (that is **20b**).

Automated wiring:

```
node Scripts/setup-korryn-20a-functional.mjs
node Scripts/setup-korryn-20a-functional.mjs --probe-only
```

Full checklist: [Korryn_20a_Editor_Setup.md](../../BreakawayCore/Docs/Korryn_20a_Editor_Setup.md).

## Ini scan

`DefaultGame.ini` already scans `/Hero_Morgan/Characters/Heroes` as `HeroDataAsset` with `AssetBaseClass=/Script/Engine.PrimaryDataAsset`.  
Kit configs scan `/Hero_Morgan/Kit` as `BwayKorrynKitConfig` (same Engine PrimaryDataAsset base).

## Later steps

- **20b** — Cursed Ward buildable
- **20c** — sheet parity / feel audit
