# Spartacus / Argus — Ability Implementation Notes

> **Current work:** Core Loop **Step 18c** (Argus parity). DisplayName **Argus**; plugin **`Hero_Spartacus`**; asset folder **`Argus`**.  
> **Editor:** [Argus_18a_Editor_Setup.md](./Argus_18a_Editor_Setup.md) · [Argus_18b_Editor_Setup.md](./Argus_18b_Editor_Setup.md) · [Argus_18c_Editor_Setup.md](./Argus_18c_Editor_Setup.md) · [CONTENT_SETUP.md](../../Heroes/Spartacus/CONTENT_SETUP.md)  
> **Parity source:** [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)

## Relic Carrier Note

Combat abilities inherit `UBwayGameplayAbility_Base`, which blocks activation when ASC has `Gameplay.State.RelicCarrier` (LMB, F, Q, E, R). Relic interaction, movement slide, Request Relic, and buildable placement must use `UBwayGameplayAbility` / `ULyraGameplayAbility` (not `Base`).

## Ability ownership (current)

| Scope | Set | Grants |
|-------|-----|--------|
| Common | `DA_BW_AbilitySet_Humanoid` | Left Shift slide · RMB Request Relic · Confirm · Cancel |
| Per-hero | `DA_BW_AbilitySet_Argus` | LMB Primary · F/Q/E/R · key **1** PlaceBuildable |
| Hero DA | `DA_BW_HeroData_Argus` | `BuildableDataAsset` → Siege Engine (18b) |

## Input layout

| Key | Input tag | Ability |
|-----|-----------|---------|
| LMB | `InputTag.Ability.Primary` | `BwayGameplayAbility_MeleePrimary` |
| LMB (while placing) | `InputTag.Ability.Buildable.Confirm` | Humanoid Confirm forwarder |
| RMB | `InputTag.Ability.RelicRequest` | Request Relic (common) |
| RMB (while placing) | `InputTag.Ability.Buildable.Cancel` | Humanoid Cancel forwarder |
| F | `InputTag.Ability.Ability4` | **Slide** (`BwayGameplayAbility_ArgusSlide`) |
| Q | `InputTag.Ability.Ability1` | **No Retreat** (`BwayGameplayAbility_NoRetreat`) |
| E | `InputTag.Ability.Ability2` | **For Glory** (`BwayGameplayAbility_ForGlory`) |
| R | `InputTag.Ability.Ability3` | **Retribution** (`BwayGameplayAbility_Retribution`) |
| Left Shift | `InputTag.Ability.Slide` | Common movement slide |
| 1 | `InputTag.Ability.Buildable` | Per-hero PlaceBuildable |

**Do not** use `InputTag.Ability.Defense`. Stats sheet **RMB** column = design dodge slot → in-game **F**.

---

## Sheet-accurate kit (18c)

Damage formula: **`Final = AbilityBaseDamage + AttackStrength × Scaling`** (Attack Str from hero DA → `ULyraCombatSet::BaseDamage`).

| Slot | Ability | CD | Base / Scale | At Atk 50 |
|------|---------|-----|--------------|-----------|
| LMB | Primary Attack | — | 10 / 0.4 | 30 |
| F | Slide | 18s | — (invuln dash) | — |
| Q | No Retreat | 12s | 2 / 0.4 | 22 |
| E | For Glory | 25s | 2 / 0.4 | 22 |
| R | Retribution | 30s | 10+20 / 0.5 | 35 + 45 |
| 1 | Siege Engine | — | HP 250; 200 dps; 10s roll | — |

### C++ classes

| Ability | Class | Cooldown GE |
|---------|-------|-------------|
| Primary | `BwayGameplayAbility_MeleePrimary` | — |
| Slide | `BwayGameplayAbility_ArgusSlide` | `UGE_Bway_Cooldown_ArgusSlide` |
| No Retreat | `BwayGameplayAbility_NoRetreat` | `UGE_Bway_Cooldown_NoRetreat` |
| For Glory | `BwayGameplayAbility_ForGlory` | `UGE_Bway_Cooldown_ForGlory` |
| Retribution | `BwayGameplayAbility_Retribution` | `UGE_Bway_Cooldown_Retribution` |

### Gameplay tags

- Abilities: `Ability.Argus.PrimaryAttack`, `.Slide`, `.NoRetreat`, `.ForGlory`, `.Retribution`
- Cooldowns: `Cooldown.Argus.Slide` / `.NoRetreat` / `.ForGlory` / `.Retribution`
- Slide owns `Gameplay.DamageImmunity` while active; For Glory owns `State.Unstoppable` while active

### Legacy Spartacus stand-ins (superseded by 18c)

Kept in tree for reference / other heroes; **do not grant** on Argus after 18c reparent:

1. `BwayGameplayAbility_ShieldBash`
2. `BwayGameplayAbility_WarCry`
3. `BwayGameplayAbility_DefensiveStance`
4. `BwayGameplayAbility_GladiatorsLeap`

---

## Testing

See [Argus_18c_Editor_Setup.md](./Argus_18c_Editor_Setup.md). Multiplayer: listen server; abilities `LocalPredicted`.

## File Locations

- C++: `Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public|Private/Abilities/`
- Content target: `/Hero_Spartacus/Characters/Heroes/Argus/`
