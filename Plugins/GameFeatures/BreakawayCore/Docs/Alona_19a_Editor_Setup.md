# Step 19a — Alona Functional (Editor Setup)

**Status:** Passed (listen-server PIE; abilities fire; relic carrier gating; Argus regression).

C++ landed in `HeroAlonaRuntime` (Alona combat kit). Complete these Editor steps after recompiling with the Editor **closed**, then restart the Editor.

**Related:** [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) · [CONTENT_SETUP.md](../../Heroes/Alona/CONTENT_SETUP.md) · [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)

## Ability ownership (canonical)

| Scope | Ability set | Grants |
|-------|-------------|--------|
| **Common (all heroes)** | `DA_BW_AbilitySet_Humanoid` | Left Shift movement slide · RMB Request Relic · Confirm/Cancel placement |
| **Per-hero** | `DA_BW_AbilitySet_Alona` | LMB Primary · F Sun's Grace · Q Radiance · E Sun Burst · R Blessing |
| **Hero DA** | `DA_BW_HeroData_Alona` | Leave `BuildableDataAsset` empty until **19b** (Sun Shrine). After 19b, re-running this script **preserves** buildable wiring |

**Do not** put Request Relic or movement slide on the per-hero set. **Do not** grant PlaceBuildable in 19a.

## Input layout

| Key | Input tag | Ability |
|-----|-----------|---------|
| **LMB** | `InputTag.Ability.Primary` | Primary Attack (projectile) |
| **RMB** | `InputTag.Ability.RelicRequest` | Request Relic (**common**) |
| **F** | `InputTag.Ability.Ability4` | **Sun's Grace** (sheet "RMB" dodge column → in-game **F**) |
| **Q** | `InputTag.Ability.Ability1` | Radiance |
| **E** | `InputTag.Ability.Ability2` | Sun Burst |
| **R** | `InputTag.Ability.Ability3` | Blessing of the Sun |
| **Left Shift** | `InputTag.Ability.Slide` | Common movement slide |

## 1. Recompile + enable plugin

1. Close Unreal Editor.
2. Compile `LyraEditor` / ProjectB so `HeroAlonaRuntime` builds.
3. Restart Editor.
4. Confirm **Edit → Plugins → Hero Alona** is available (project already lists it enabled).

## 2. Automated wiring (preferred)

With Editor + MCP bridge running:

```
node Scripts/setup-alona-19a-functional.mjs
```

Probe only:

```
node Scripts/setup-alona-19a-functional.mjs --probe-only
```

The script:

- Ensures `/Hero_Alona/.../DA_BW_HeroData_Alona` and `DA_BW_AbilitySet_Alona`
- Sets DisplayName **Alona**, stats HP **350** / Armor **0** / Atk **40** / Speed **10**
- Grants native C++ abilities on Primary / Ability4 / Ability1 / Ability2 / Ability3
- Wires `Hero_Alona` GFD
- Adds **`Hero_Alona`** to `B_BW_Experience_Dev` **GameFeaturesToEnable** while keeping **`Hero_Spartacus`**

## 3. Manual fallback — `DA_BW_HeroData_Alona`

| Field | Value |
|-------|--------|
| **DisplayName** | `Alona` |
| **HeroClass** | Support |
| **HeroStats → MaxHealth** | `350` |
| **HeroStats → BaseDamage** | `40` |
| **HeroStats → Armor** | `0` |
| **HeroStats → MoveSpeed** | `10` |
| **AttributeSetClass** | `BwayHeroAttributeSet` |
| **AbilitySets** | `[DA_BW_AbilitySet_Alona]` |
| **BuildableDataAsset** | empty (19b) |

## 4. Manual fallback — `DA_BW_AbilitySet_Alona`

| Slot | Input tag | Native class |
|------|-----------|--------------|
| LMB | `InputTag.Ability.Primary` | `BwayGameplayAbility_AlonaPrimary` |
| F | `InputTag.Ability.Ability4` | `BwayGameplayAbility_AlonaSunsGrace` |
| Q | `InputTag.Ability.Ability1` | `BwayGameplayAbility_AlonaRadiance` |
| E | `InputTag.Ability.Ability2` | `BwayGameplayAbility_AlonaSunBurst` |
| R | `InputTag.Ability.Ability3` | `BwayGameplayAbility_AlonaBlessing` |

## 5. Experience + AssetManager

1. `B_BW_Experience_Dev` → **GameFeaturesToEnable** includes `BreakawayCore`, `Hero_Spartacus`, **`Hero_Alona`**.
2. Confirm `Config/DefaultGame.ini` already scans `/Hero_Alona/Characters/Heroes` under `HeroDataAsset` (do **not** duplicate the scan entry).

## 6. PIE test

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Alona&NumBots=7
```

**Listen server**, 1 player.

### Pass checklist (19a)

- [x] Log: `BwayHeroStatsLibrary: Applied stats … HP 350 …`
- [x] LMB fires projectile and damages enemies (`ScaledDamage: base=28.0 atk=40.0 scale=0.25 -> 38.0`)
- [x] **F** Sun's Grace teleports + invuln window; **RMB** still Request Relic
- [x] **Q** Radiance heals reticule ally (or self fallback) for ~4s
- [x] **E** Sun Burst knocks nearby enemies
- [x] **R** Blessing spawns ground heal circle
- [x] **Left Shift** movement slide works
- [x] Relic carrier blocks LMB / F / Q / E / R; Shift + RMB Request Relic still work
- [x] Argus regression: `Hero=Argus&NumBots=7` still works
- [x] **3/3** cold-start PIE runs

## Deferred

| Item | Step |
|------|------|
| Sun Shrine buildable | 19b — [Alona_19b_Editor_Setup.md](./Alona_19b_Editor_Setup.md) · `Scripts/setup-alona-19b-sun-shrine.mjs` |
| Radiance STR/52 heal scaling + final parity spot-check | 19c |
| In-match ability bar polish | 22 |

## C++ classes

| Ability | Class | Cooldown GE |
|---------|-------|-------------|
| Primary | `BwayGameplayAbility_AlonaPrimary` | — |
| Sun's Grace | `BwayGameplayAbility_AlonaSunsGrace` | `UGE_Bway_Cooldown_AlonaSunsGrace` (22s) |
| Radiance | `BwayGameplayAbility_AlonaRadiance` | `UGE_Bway_Cooldown_AlonaRadiance` (8s) |
| Sun Burst | `BwayGameplayAbility_AlonaSunBurst` | `UGE_Bway_Cooldown_AlonaSunBurst` (20s) |
| Blessing | `BwayGameplayAbility_AlonaBlessing` | `UGE_Bway_Cooldown_AlonaBlessing` (25s) |

Supporting actors: `ABwayAlonaPrimaryProjectile`, `ABwayAlonaBlessingZone`.
