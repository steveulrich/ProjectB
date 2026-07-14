# Step 18c — Argus Parity (Editor Setup)

C++ landed in `BreakawayCoreRuntime`:

| Slot | C++ class | CD GE | Sheet |
|------|-----------|-------|-------|
| LMB | `BwayGameplayAbility_MeleePrimary` | — | Base 10, Scale 0.4 |
| F | `BwayGameplayAbility_ArgusSlide` | `GE_Bway_Cooldown_ArgusSlide` (18s) | Invuln dash |
| Q | `BwayGameplayAbility_NoRetreat` | `GE_Bway_Cooldown_NoRetreat` (12s) | Base 2, Scale 0.4 |
| E | `BwayGameplayAbility_ForGlory` | `GE_Bway_Cooldown_ForGlory` (25s) | Base 2, Scale 0.4 |
| R | `BwayGameplayAbility_Retribution` | `GE_Bway_Cooldown_Retribution` (30s) | Base 10+20, Scale 0.5 |
| 1 | Siege Engine (18b) | — | HP 250, 200 dps, 10s |

**Damage formula:** `Final = AbilityBaseDamage + AttackStrength * Scaling` (Attack Str 50 → Primary 30, No Retreat/For Glory 22, Retribution 35+45).

Complete these Editor steps after recompiling. Source of truth: [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md).

**Automated wiring (when Editor + ue-mcp bridge are up):**

```
node Scripts/setup-argus-18c-parity.mjs
```

Probe only: `node Scripts/setup-argus-18c-parity.mjs --probe-only`

## 1. Reparent ability BPs

Under `/Hero_Spartacus/Characters/Heroes/Argus/Abilities/`:

| Existing BP (or create new) | New parent class | DisplayData AbilityName |
|-----------------------------|------------------|-------------------------|
| `GA_BW_Spartacus_PrimaryAttack` | `BwayGameplayAbility_MeleePrimary` | Primary Attack |
| `GA_BW_Spartacus_GladiatorsLeap` → prefer rename `GA_BW_Argus_Slide` | `BwayGameplayAbility_ArgusSlide` | Slide |
| `GA_BW_Spartacus_ShieldBash` → prefer rename `GA_BW_Argus_NoRetreat` | `BwayGameplayAbility_NoRetreat` | No Retreat |
| `GA_BW_Spartacus_WarCry` → prefer rename `GA_BW_Argus_ForGlory` | `BwayGameplayAbility_ForGlory` | For Glory |
| `GA_BW_Spartacus_DefensiveStance` → prefer rename `GA_BW_Argus_Retribution` | `BwayGameplayAbility_Retribution` | Retribution |

**Reparent:** open BP → File → Reparent Blueprint → pick C++ class → Compile/Save.

**Do not** assign a Blueprint Cooldown GE that overrides the C++ `CooldownGameplayEffectClass` unless you intentionally change sheet CDs. Clear any stale Spartacus stun/buff GE refs on the BP defaults.

**Do not** put `InputTag.*` on AbilityTags (grants carry input tags via the ability set).

## 2. Wire `DA_BW_AbilitySet_Argus`

| Input tag | Ability BP | Key |
|-----------|------------|-----|
| `InputTag.Ability.Primary` | PrimaryAttack | LMB |
| `InputTag.Ability.Ability4` | Argus Slide | F |
| `InputTag.Ability.Ability1` | No Retreat | Q |
| `InputTag.Ability.Ability2` | For Glory | E |
| `InputTag.Ability.Ability3` | Retribution | R |
| `InputTag.Ability.Buildable` | PlaceBuildable (unchanged) | 1 |

## 3. Siege Engine spot-check

On `BP_BW_Buildable_SiegeEngine` (`BwaySiegeEngineBuildable`):

| Field | Value |
|-------|-------|
| MaxHealth | 250 |
| BuildableDamagePerSecond | 200 |
| MaxRollDuration | 10 |

## 4. Hero stats spot-check

`DA_BW_HeroData_Argus` → HeroStats: HP 500, BaseDamage (Attack Str) 50, Armor 3, MoveSpeed 10.

## 5. PIE test

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Argus&NumBots=7
```

Listen server, 1 player.

### Pass checklist (18c)

- [ ] Log on spawn: `BwayHeroStatsLibrary: Applied stats … HP 500 … Atk 50`
- [ ] LMB hit log: `ScaledDamage: base=10.0 atk=50.0 scale=0.40 -> 30.0`
- [ ] F Slide: dash + invuln window; cannot re-cast for **18s** (`Cooldown.Argus.Slide`)
- [ ] Q No Retreat: charge; hit log `base=2.0 … -> 22.0`; CD **12s**
- [ ] E For Glory: kick knockback; hit log `base=2.0 … -> 22.0`; CD **25s**
- [ ] R Retribution: two hits `10→35` then `20→45`; CD **30s**
- [ ] Siege Engine still places/rolls with 250 HP / 200 dps / 10s
- [ ] Relic carrier still blocks LMB/F/Q/E/R; Shift + Request Relic + PlaceBuildable OK
- [ ] Relic pickup/score regression OK
- [ ] **3/3** cold-start PIE runs

### Expected damage at Attack Str 50

| Ability | Formula | Expected |
|---------|---------|----------|
| Primary | 10 + 50×0.4 | **30** |
| No Retreat | 2 + 50×0.4 | **22** |
| For Glory | 2 + 50×0.4 | **22** |
| Retribution hit 1 | 10 + 50×0.5 | **35** |
| Retribution hit 2 | 20 + 50×0.5 | **45** |
