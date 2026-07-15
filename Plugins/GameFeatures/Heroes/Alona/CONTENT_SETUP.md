# Alona — Content Setup (Section 3 / Gate 2)

**DisplayName:** `Alona`. Plugin: `Hero_Alona`. See [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md).
**Section 3:** [Alona_19b_Editor_Setup.md](../../BreakawayCore/Docs/Alona_19b_Editor_Setup.md) · [Alona_19a_Editor_Setup.md](../../BreakawayCore/Docs/Alona_19a_Editor_Setup.md) · [CoreLoop_Implementation_Plan.md](../../BreakawayCore/Docs/CoreLoop_Implementation_Plan.md)

## Target layout

| Item | Path / notes |
|------|----------------|
| Hero DA | `/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona` |
| Ability set | `DA_BW_AbilitySet_Alona` — combat (19a) + PlaceBuildable (19b) |
| Buildable | **Sun Shrine** — `Buildables/DA_BW_BuildableData_SunShrine` + `BP_BW_Buildable_SunShrine` |
| Place GA | `Abilities/GA_BW_Alona_BuildablePlacement` → `UBwayGameplayAbility_PlaceBuildable` |
| GFD | `/Hero_Alona/Hero_Alona` (`UBwayGameFeatureData`) |
| Runtime module | `HeroAlonaRuntime` |

## Hero DA (19b)

| Field | Value |
|-------|--------|
| DisplayName | **Alona** |
| HeroClass | Support |
| HeroStats | HP 350, BaseDamage 40, Armor 0, MoveSpeed 10 |
| AttributeSetClass | `BwayHeroAttributeSet` |
| AbilitySets | `DA_BW_AbilitySet_Alona` |
| BuildableDataAsset | `DA_BW_BuildableData_SunShrine` |

## Ability set grants

| Input tag | Ability |
|-----------|---------|
| `InputTag.Ability.Primary` | `BwayGameplayAbility_AlonaPrimary` |
| `InputTag.Ability.Ability4` | `BwayGameplayAbility_AlonaSunsGrace` (F) |
| `InputTag.Ability.Ability1` | `BwayGameplayAbility_AlonaRadiance` |
| `InputTag.Ability.Ability2` | `BwayGameplayAbility_AlonaSunBurst` |
| `InputTag.Ability.Ability3` | `BwayGameplayAbility_AlonaBlessing` |
| `InputTag.Ability.Buildable` | `GA_BW_Alona_BuildablePlacement` (key **1**) |

Common humanoid set still owns Left Shift slide + RMB Request Relic + Confirm/Cancel.

## Gate / step done when

- [x] `Hero_Alona` enabled on `B_BW_Experience_Dev` (Argus/`Hero_Spartacus` kept)
- [x] Scan path `/Hero_Alona/Characters/Heroes` present in `DefaultGame.ini`
- [x] 19a combat kit fires on listen server; relic carrier gating correct
- [ ] 19b Sun Shrine (C++ + scripts done; Editor wire + PIE pass open)
- [ ] 19c sheet parity
- [x] **3/3** cold-start PIE (19a); re-run for 19b after Editor wire

## Related

- Stats authority: [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)
- Automated wiring: `Scripts/setup-alona-19b-sun-shrine.mjs` · `Scripts/setup-alona-19a-functional.mjs` (preserves buildable after 19b)
