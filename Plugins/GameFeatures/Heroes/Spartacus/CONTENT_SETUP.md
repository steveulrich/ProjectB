# Spartacus — Content Setup

C++ abilities exist. Complete these Editor assets for vertical slice.

## Hero Data Asset

**Path:** `/BreakawayCore/Characters/Heroes/DA_BW_Hero_Spartacus`

| Field | Value |
|-------|-------|
| DisplayName | Spartacus |
| HeroClass | Tank |
| HeroMesh | Spartacus skeletal mesh |
| AbilitySets | LAS with 4 Spartacus GAs + Slide |
| BuildableDataAssets | DA_FireCatapult, DA_DragonSpire |

## Ability Blueprints

Create `UGameplayAbility` BPs subclassing C++ types:

| C++ class | BP asset |
|-----------|----------|
| `UBwayGameplayAbility_ShieldBash` | `GA_Spartacus_ShieldBash` |
| `UBwayGameplayAbility_WarCry` | `GA_Spartacus_WarCry` |
| `UBwayGameplayAbility_DefensiveStance` | `GA_Spartacus_DefensiveStance` |
| `UBwayGameplayAbility_GladiatorsLeap` | `GA_Spartacus_GladiatorsLeap` |
| `UBwayGameplayAbility_Slide` | `GA_Slide` |

Wire cooldowns, GEs, input tags per [Spartacus_Implementation_Summary.md](../../BreakawayCore/Docs/Spartacus_Implementation_Summary.md).

## Buildables

| Buildable | Base class | Data asset |
|-----------|------------|------------|
| Fire Catapult | `ATurretBase` BP | `DA_BW_Buildable_FireCatapult` |
| Dragon Spire | `ATrapBase` BP | `DA_BW_Buildable_DragonSpire` |

Set `bPersistsBetweenRounds = true`.

## Game Feature Data

In this plugin's Content folder, create `GFD_Hero_Spartacus` (`UBwayGameFeatureData`) listing `DA_BW_Hero_Spartacus`.

Enable `Hero_Spartacus` in `B_BW_Experience_CaptureTheRelic`.
