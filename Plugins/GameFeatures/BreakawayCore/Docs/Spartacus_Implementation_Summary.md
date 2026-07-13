# Spartacus / Argus — Ability Implementation Notes

> **Current work:** Core Loop **Step 18** (Argus). DisplayName **Argus**; plugin **`Hero_Spartacus`**; asset folder **`Argus`**.  
> **Editor:** [Argus_18a_Editor_Setup.md](./Argus_18a_Editor_Setup.md) · [Argus_18b_Editor_Setup.md](./Argus_18b_Editor_Setup.md) · [CONTENT_SETUP.md](../../Heroes/Spartacus/CONTENT_SETUP.md)  
> **Parity source:** [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)

This doc retains the **legacy Spartacus C++ kit** (Shield Bash / War Cry / Defensive Stance / Gladiator's Leap) used as **18a stand-ins** for F/Q/E/R until **18c** replaces them with sheet-accurate Argus abilities.

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
| LMB | `InputTag.Ability.Primary` | Per-hero primary (`BwayGameplayAbility_MeleePrimary`) |
| LMB (while placing) | `InputTag.Ability.Buildable.Confirm` | Humanoid Confirm forwarder |
| RMB | `InputTag.Ability.RelicRequest` | Request Relic (common) |
| RMB (while placing) | `InputTag.Ability.Buildable.Cancel` | Humanoid Cancel forwarder |
| F | `InputTag.Ability.Ability4` | Defense dodge (sheet name "Slide"; 18a stand-in Gladiator's Leap) |
| Q | `InputTag.Ability.Ability1` | No Retreat (18a stand-in Shield Bash) |
| E | `InputTag.Ability.Ability2` | For Glory (18a stand-in War Cry) |
| R | `InputTag.Ability.Ability3` | Retribution (18a stand-in Defensive Stance) |
| Left Shift | `InputTag.Ability.Slide` | Common movement slide |
| 1 | `InputTag.Ability.Buildable` | Per-hero PlaceBuildable |

**Do not** use `InputTag.Ability.Defense`. Stats sheet **RMB** column = design dodge slot → in-game **F**.

---

## Legacy C++ kit (18a stand-ins)

All C++ classes live under `BreakawayCoreRuntime` `Abilities/`.

1. **Shield Bash** (`BwayGameplayAbility_ShieldBash`) — dash + stun; 18a stand-in for **No Retreat**
2. **War Cry** (`BwayGameplayAbility_WarCry`) — AOE ally buff; 18a stand-in for **For Glory**
3. **Defensive Stance** (`BwayGameplayAbility_DefensiveStance`) — toggle DR; 18a stand-in for **Retribution**
4. **Gladiator's Leap** (`BwayGameplayAbility_GladiatorsLeap`) — leap AOE; 18a stand-in for Argus defense **Slide**

### Gameplay tags (legacy)

- `State.Stunned`, `State.Buffed.WarCry`, `State.DefensiveStance`, `State.Dashing`, `State.Leaping`
- `Ability.Spartacus.ShieldBash`, `WarCry`, `DefensiveStance`, `GladiatorsLeap`
- `GameplayEffect.DamageType.Ability`

### Editor content (if still using stand-ins)

Create GEs / BP children as needed under hero plugin content; wire cooldowns on each BP. Prefer Argus sheet names in DisplayData even when parent is a Spartacus stand-in class.

---

## Target sheet kit (18c)

| Slot | Sheet ability | CD | Notes |
|------|---------------|-----|-------|
| LMB | Primary Attack | — | `BwayGameplayAbility_MeleePrimary` (done in 18a) |
| F | Slide (dodge) | 18s | Replace Gladiator's Leap stand-in |
| Q | No Retreat | 12s | Replace Shield Bash |
| E | For Glory | 25s | Replace War Cry |
| R | Retribution | 30s | Replace Defensive Stance |
| 1 | Siege Engine | — | 18b PlaceBuildable + `ABwaySiegeEngineBuildable` |

## Testing

See 18a / 18b pass checklists. Multiplayer: listen server; abilities `LocalPredicted` where applicable.

## File Locations

- C++: `Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public|Private/Abilities/`
- Content target: `/Hero_Spartacus/Characters/Heroes/Argus/`
