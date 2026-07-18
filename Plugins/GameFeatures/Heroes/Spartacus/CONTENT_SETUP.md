# Spartacus / Argus — Content Setup (Section 3 Step 18)

**Plugin:** `Hero_Spartacus`  
**Asset folder (codename):** `Argus`  
**DisplayName (UI):** **Argus** — Section 3 / stats sheet authority  
**Parity:** [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)

C++ lives in `BreakawayCoreRuntime`. Content targets this plugin mount (`/Hero_Spartacus/...`). Legacy `/BreakawayCore/Characters/Heroes/Argus/` may still hold assets until migration cutover.

**Editor checklists:** [Argus_18a_Editor_Setup.md](../../BreakawayCore/Docs/Argus_18a_Editor_Setup.md) · [Argus_18b_Editor_Setup.md](../../BreakawayCore/Docs/Argus_18b_Editor_Setup.md) · [Argus_18c_Editor_Setup.md](../../BreakawayCore/Docs/Argus_18c_Editor_Setup.md)  
**Ability notes:** [Spartacus_Implementation_Summary.md](../../BreakawayCore/Docs/Spartacus_Implementation_Summary.md)

## Ability ownership (canonical)

| Scope | Ability set | Grants |
|-------|-------------|--------|
| Common | `DA_BW_AbilitySet_Humanoid` | Left Shift slide · RMB Request Relic · Confirm · Cancel |
| Per-hero | `DA_BW_AbilitySet_Argus` | LMB Primary · F/Q/E/R · key **1** PlaceBuildable |
| Hero DA | `DA_BW_HeroData_Argus` | Single `BuildableDataAsset` → Siege Engine |

## Target layout

| Item | Path |
|------|------|
| Hero DA | `/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus` |
| Ability set | `…/DA_BW_AbilitySet_Argus` |
| PlaceBuildable BP | `…/Abilities/GA_BW_Spartacus_BuildablePlacement` |
| Siege Engine DA | `…/Buildables/DA_BW_BuildableData_SiegeEngine` |
| Siege Engine actor | `…/Buildables/BP_BW_Buildable_SiegeEngine` |
| Kit config (20.5) | `/Hero_Spartacus/Kit/DA_BW_ArgusKitConfig` (`UBwayArgusKitConfig`) |
| GFD | `GFD_Hero_Spartacus` (`UBwayGameFeatureData`) |

### Hero DA fields (18a + 18b)

| Field | Value |
|-------|--------|
| DisplayName | **Argus** |
| HeroStats | HP 500, BaseDamage 50, Armor 3, MoveSpeed 10 |
| AttributeSetClass | `BwayHeroAttributeSet` |
| AbilitySets | `DA_BW_AbilitySet_Argus` |
| BuildableDataAsset | `DA_BW_BuildableData_SiegeEngine` (18b) |
| HeroMesh / AnimBP | Argus mesh (not Alona placeholder) |

### Ability set grants

| Input tag | Ability | Notes |
|-----------|---------|-------|
| `Primary` | MeleePrimary BP | 18a/18c — damage 10/0.4 |
| `Ability4` | Argus Slide BP (`BwayGameplayAbility_ArgusSlide`) | F key — 18c |
| `Ability1` | No Retreat BP (`BwayGameplayAbility_NoRetreat`) | Q — 18c |
| `Ability2` | For Glory BP (`BwayGameplayAbility_ForGlory`) | E — 18c |
| `Ability3` | Retribution BP (`BwayGameplayAbility_Retribution`) | R — 18c |
| `Buildable` | `GA_BW_Spartacus_BuildablePlacement` | 18b — **not** on humanoid |

### Buildable (one — not two)

| Slice name | Data | Actor | Behavior |
|------------|------|-------|----------|
| **Siege Engine** | `DA_BW_BuildableData_SiegeEngine` | `BP_BW_Buildable_SiegeEngine` (`BwaySiegeEngineBuildable`) | 250 HP; 200 dmg/s vs buildables; 10s roll; once/round free; persists |

**Deprecated (do not use for Section 3):** Fire Catapult / Dragon Spire two-buildable Gate 1 targets.

## Migration / enable steps

1. **Enable plugin** — Edit → Plugins → `Hero Spartacus` → Enabled.
2. **Content** — Create or move Argus hero DA, ability set, abilities, Siege Engine under `/Hero_Spartacus/Characters/Heroes/Argus/`.
3. **GFD** — `GFD_Hero_Spartacus` → `HeroDataAssets` includes hero DA.
4. **Config** — `DefaultGame.ini` `HeroDataAsset` scan includes `(Path="/Hero_Spartacus/Characters/Heroes")`.
5. **Experience** — `B_BW_Experience_Dev` → **GameFeaturesToEnable** → `Hero_Spartacus`.
6. **18a** — Combat kit + stats per Argus_18a checklist.
7. **18b** — Siege Engine + PlaceBuildable + Confirm/Cancel dual-bind per Argus_18b checklist.
8. **Cutover** — After verify, remove legacy `/BreakawayCore/Characters/Heroes/Argus/` if duplicated.

## Gate done when (Step 18)

- [ ] `Hero_Spartacus` on `B_BW_Experience_Dev`
- [ ] Hero DA loads; DisplayName **Argus**
- [ ] 18a combat + relic carrier pass
- [ ] 18b Siege Engine place / confirm / cancel / once-per-round / persist pass
- [ ] 18c sheet parity (CDs + damage) — [Argus_18c_Editor_Setup.md](../../BreakawayCore/Docs/Argus_18c_Editor_Setup.md)
- [ ] **3/3** cold-start PIE listen server

Then proceed to **Alona** ([CONTENT_SETUP](../Alona/CONTENT_SETUP.md)) only after 18c is green.

## Related

- [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md) — plugin/folder codenames; DisplayName follows Section 3
- [CoreLoop_Implementation_Plan.md](../../BreakawayCore/Docs/CoreLoop_Implementation_Plan.md) — Step 18
