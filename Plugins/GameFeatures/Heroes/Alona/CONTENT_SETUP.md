# Alona — Content Setup (Section 3 / Gate 2)

**DisplayName:** `Alona`. Plugin: `Hero_Alona`. See [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md).
**Section 3:** [Alona_19a_Editor_Setup.md](../../BreakawayCore/Docs/Alona_19a_Editor_Setup.md) · [CoreLoop_Implementation_Plan.md](../../BreakawayCore/Docs/CoreLoop_Implementation_Plan.md)

## Target layout

| Item | Path / notes |
|------|----------------|
| Hero DA | `/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona` |
| Ability set | `DA_BW_AbilitySet_Alona` — native C++ grants (19a) |
| Buildable | **Sun Shrine** only (Section 3) — wire in **19b**; leave empty in 19a |
| GFD | `/Hero_Alona/Hero_Alona` (`UBwayGameFeatureData`) |
| Runtime module | `HeroAlonaRuntime` |

## Hero DA (19a)

| Field | Value |
|-------|--------|
| DisplayName | **Alona** |
| HeroClass | Support |
| HeroStats | HP 350, BaseDamage 40, Armor 0, MoveSpeed 10 |
| AttributeSetClass | `BwayHeroAttributeSet` |
| AbilitySets | `DA_BW_AbilitySet_Alona` |
| BuildableDataAsset | empty until 19b |

## Ability set grants (19a)

| Input tag | Ability |
|-----------|---------|
| `InputTag.Ability.Primary` | `BwayGameplayAbility_AlonaPrimary` |
| `InputTag.Ability.Ability4` | `BwayGameplayAbility_AlonaSunsGrace` (F) |
| `InputTag.Ability.Ability1` | `BwayGameplayAbility_AlonaRadiance` |
| `InputTag.Ability.Ability2` | `BwayGameplayAbility_AlonaSunBurst` |
| `InputTag.Ability.Ability3` | `BwayGameplayAbility_AlonaBlessing` |

Common humanoid set still owns Left Shift slide + RMB Request Relic.

## Gate / step done when

- [x] `Hero_Alona` enabled on `B_BW_Experience_Dev` (Argus/`Hero_Spartacus` kept)
- [x] Scan path `/Hero_Alona/Characters/Heroes` present in `DefaultGame.ini`
- [x] 19a combat kit fires on listen server; relic carrier gating correct
- [ ] 19b Sun Shrine
- [ ] 19c sheet parity
- [x] **3/3** cold-start PIE

## Related

- Stats authority: [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)
- Automated wiring: `Scripts/setup-alona-19a-functional.mjs`
