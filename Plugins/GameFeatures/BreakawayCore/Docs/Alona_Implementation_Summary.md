# Alona — Ability Implementation Notes

> **Current work:** Core Loop **Step 19b C++ + scripts done** (Sun Shrine). Next: Editor wire + listen-server pass, then **19c** parity. DisplayName **Alona**; plugin **`Hero_Alona`**; runtime **`HeroAlonaRuntime`**.
> **Editor:** [Alona_19b_Editor_Setup.md](./Alona_19b_Editor_Setup.md) · [Alona_19a_Editor_Setup.md](./Alona_19a_Editor_Setup.md) · [CONTENT_SETUP.md](../../Heroes/Alona/CONTENT_SETUP.md)
> **Parity source:** [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)

## Relic Carrier Note

Combat abilities inherit `UBwayGameplayAbility_Base`, which blocks activation when ASC has `Gameplay.State.RelicCarrier` (LMB, F, Q, E, R). Relic interaction, movement slide, and Request Relic remain on the common humanoid set (`UBwayGameplayAbility` / Lyra).

## Ability ownership

| Scope | Set | Grants |
|-------|-----|--------|
| Common | `DA_BW_AbilitySet_Humanoid` | Left Shift slide · RMB Request Relic · Confirm · Cancel |
| Per-hero | `DA_BW_AbilitySet_Alona` | LMB / F / Q / E / R · **PlaceBuildable** (`InputTag.Ability.Buildable`) |
| Hero DA | `DA_BW_HeroData_Alona` | `BuildableDataAsset` → Sun Shrine DA |

## Input layout

| Key | Input tag | Ability |
|-----|-----------|---------|
| LMB | `InputTag.Ability.Primary` | `BwayGameplayAbility_AlonaPrimary` |
| RMB | `InputTag.Ability.RelicRequest` | Request Relic (common) |
| F | `InputTag.Ability.Ability4` | **Sun's Grace** |
| Q | `InputTag.Ability.Ability1` | **Radiance** |
| E | `InputTag.Ability.Ability2` | **Sun Burst** |
| R | `InputTag.Ability.Ability3` | **Blessing of the Sun** |
| Left Shift | `InputTag.Ability.Slide` | Common movement slide |
| **1** | `InputTag.Ability.Buildable` | PlaceBuildable → **Sun Shrine** |

Stats sheet **RMB** dodge column → in-game **F**.

## Functional kit (19a)

| Slot | Ability | CD | Notes |
|------|---------|-----|-------|
| LMB | Primary | — | Projectile; Base 28 / Scale 0.25 → 38 at Atk 40 |
| F | Sun's Grace | 22s | Invuln teleport (`Gameplay.DamageImmunity`) |
| Q | Radiance | 8s | Closest ally to reticule; 4s active; flat heal placeholder |
| E | Sun Burst | 20s | Radial knockback + sheet damage defaults |
| R | Blessing | 25s | Ground heal zone; initial 70 + 35/s defaults |

## C++ locations

- Abilities / cooldowns / projectile / zone: `Plugins/GameFeatures/Heroes/Alona/Source/HeroAlonaRuntime/`
- Shared heal helper: `UBwayGameplayAbility_Base::ApplyHealToAlly`
- Content target: `/Hero_Alona/Characters/Heroes/Alona/`

## Sun Shrine (19b)

| Piece | Class / asset |
|-------|----------------|
| Actor | `ABwayAlonaSunShrineBuildable` — 750 HP, 35 HP/s, 500 uu, non-stack, persists |
| Placement | Shared `UBwayGameplayAbility_PlaceBuildable` (once/round free) |
| Editor | [Alona_19b_Editor_Setup.md](./Alona_19b_Editor_Setup.md) · `Scripts/setup-alona-19b-sun-shrine.mjs` |

## Deferred

- **19c** Radiance STR/52 scaling + full sheet parity spot-check
