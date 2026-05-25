# Spartacus Hero Implementation Summary

> **See also:** [GAS_and_Abilities.md](./GAS_and_Abilities.md) · [Hero plugin setup](../../Heroes/Spartacus/CONTENT_SETUP.md)

## Relic Carrier Note

Combat abilities inherit `UBwayGameplayAbility_Base`, which blocks activation when ASC has `Gameplay.State.RelicCarrier`. Relic interaction abilities must use `ULyraGameplayAbility` (or a non-blocked base).

## Completed (C++ Code)

All C++ ability classes have been implemented:

1. **Shield Bash** (`BwayGameplayAbility_ShieldBash`)
   - Dash forward with sphere trace for enemy detection
   - Stuns first enemy hit
   - 8 second cooldown (configured in Blueprint)

2. **War Cry** (`BwayGameplayAbility_WarCry`)
   - AOE buff for nearby allies
   - Applies speed and damage buff
   - 12 second cooldown (configured in Blueprint)

3. **Defensive Stance** (`BwayGameplayAbility_DefensiveStance`)
   - Toggle ability for damage reduction and movement slow
   - Infinite duration until toggled off
   - No cooldown (toggle on/off)

4. **Gladiator's Leap** (`BwayGameplayAbility_GladiatorsLeap`)
   - Leap to target location
   - AOE damage on landing
   - 45 second cooldown (configured in Blueprint)

## Gameplay Tags Added

All required gameplay tags have been added to `BreakawayCore.ini`:
- `State.Stunned`
- `State.Buffed.WarCry`
- `State.DefensiveStance`
- `State.Dashing`
- `State.Leaping`
- `GameplayEffect.DamageType.Ability`
- `Ability.Spartacus.ShieldBash`
- `Ability.Spartacus.WarCry`
- `Ability.Spartacus.DefensiveStance`
- `Ability.Spartacus.GladiatorsLeap`

## Remaining Tasks (Unreal Editor)

### 1. Create Gameplay Effects (Blueprints)

Create the following Blueprint gameplay effects in `Content/GameplayEffects/`:

#### GE_Spartacus_ShieldBash_Stun
- **Parent Class:** `GameplayEffect`
- **Duration:** 1.5 seconds
- **Granted Tags:** `State.Stunned`
- **Block Abilities:** Add tag requirements to block movement/abilities

#### GE_Spartacus_WarCry_Buff
- **Parent Class:** `GameplayEffect`
- **Duration:** 5 seconds
- **Granted Tags:** `State.Buffed.WarCry`
- **Modifiers:**
  - Movement Speed: +20% (Multiplier)
  - Base Damage: +15% (Multiplier)

#### GE_Spartacus_DefensiveStance
- **Parent Class:** `GameplayEffect`
- **Duration:** Infinite
- **Granted Tags:** `State.DefensiveStance`
- **Modifiers:**
  - Incoming Damage: -40% (Multiplier)
  - Movement Speed: -30% (Multiplier)

#### GE_Spartacus_GladiatorsLeap_Damage
- **Parent Class:** `GameplayEffect`
- **Duration:** Instant
- **Damage Type Tag:** `GameplayEffect.DamageType.Ability`
- **Damage:** 100 (or use SetByCaller from LyraGameData)

### 2. Create Ability Blueprints

For each C++ ability class, create a Blueprint in `Content/Abilities/Spartacus/`:

#### BP_GA_Spartacus_ShieldBash
- **Parent Class:** `BwayGameplayAbility_ShieldBash`
- **Cooldown Gameplay Effect:** Create a cooldown effect (8 seconds)
- **Input Tag:** `InputTag.Ability.Ability1`
- **Stun Gameplay Effect Class:** Assign `GE_Spartacus_ShieldBash_Stun`

#### BP_GA_Spartacus_WarCry
- **Parent Class:** `BwayGameplayAbility_WarCry`
- **Cooldown Gameplay Effect:** Create a cooldown effect (12 seconds)
- **Input Tag:** `InputTag.Ability.Ability2`
- **Buff Gameplay Effect Class:** Assign `GE_Spartacus_WarCry_Buff`

#### BP_GA_Spartacus_DefensiveStance
- **Parent Class:** `BwayGameplayAbility_DefensiveStance`
- **Input Tag:** `InputTag.Ability.Ability3`
- **Stance Gameplay Effect Class:** Assign `GE_Spartacus_DefensiveStance`

#### BP_GA_Spartacus_GladiatorsLeap
- **Parent Class:** `BwayGameplayAbility_GladiatorsLeap`
- **Cooldown Gameplay Effect:** Create a cooldown effect (45 seconds)
- **Input Tag:** `InputTag.Ability.Defense`
- **Damage Gameplay Effect Class:** Assign `GE_Spartacus_GladiatorsLeap_Damage` (or use LyraGameData damage effect)

### 3. Create Ability Set

Create `DA_Spartacus_AbilitySet` in `Content/Characters/Heroes/Spartacus/`:
- **Parent Class:** `LyraAbilitySet`
- **Granted Gameplay Abilities:**
  1. `BP_GA_Spartacus_ShieldBash` - InputTag: `InputTag.Ability.Ability1`
  2. `BP_GA_Spartacus_WarCry` - InputTag: `InputTag.Ability.Ability2`
  3. `BP_GA_Spartacus_DefensiveStance` - InputTag: `InputTag.Ability.Ability3`
  4. `BP_GA_Spartacus_GladiatorsLeap` - InputTag: `InputTag.Ability.Defense`

### 4. Create Hero Data Asset

Create `DA_Spartacus_HeroData` in `Content/Characters/Heroes/Spartacus/`:
- **Parent Class:** `BwayHeroDataAsset`
- **Display Name:** "Spartacus"
- **Portrait:** Assign hero portrait texture
- **Hero Mesh:** Assign Spartacus skeletal mesh
- **Animation BP:** Assign Spartacus animation blueprint
- **Hero Stats:**
  - Max Health: 300
  - Base Damage: 25
  - Move Speed: 550
- **Ability Sets:** Add `DA_Spartacus_AbilitySet`
- **Attribute Set Class:** Use existing Lyra health/combat sets

### 5. Verify Input Bindings

Verify in `Content/Input/DA_BW_InputData_Humanoid`:
- Q key → `InputTag.Ability.Ability1` (Shield Bash)
- E key → `InputTag.Ability.Ability2` (War Cry)
- F key → `InputTag.Ability.Ability3` (Defensive Stance)
- R key → `InputTag.Ability.Defense` (Gladiator's Leap)

### 6. Testing Checklist

#### Singleplayer Testing
- [ ] All 4 abilities activate correctly
- [ ] Cooldowns work and display properly
- [ ] Shield Bash stuns enemies
- [ ] War Cry buffs nearby allies
- [ ] Defensive Stance toggles on/off correctly
- [ ] Gladiator's Leap damages enemies on landing
- [ ] Can pick up/throw relic while using abilities
- [ ] Abilities don't interfere with movement

#### Multiplayer Testing
- [ ] All abilities replicate correctly
- [ ] Cooldowns sync across clients
- [ ] Shield Bash stun replicates
- [ ] War Cry buff applies to all clients
- [ ] Defensive Stance state replicates
- [ ] Gladiator's Leap landing damage replicates
- [ ] No desync issues

## File Locations

### C++ Files
- Headers: `Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Abilities/`
- Implementation: `Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/Abilities/`

### Blueprint Assets (to be created)
- Abilities: `Content/Abilities/Spartacus/`
- Gameplay Effects: `Content/GameplayEffects/`
- Hero Data: `Content/Characters/Heroes/Spartacus/`

## Notes

- All abilities use `LocalPredicted` network execution policy for responsive gameplay
- Team checking uses `BwayGameState::AreOnSameTeam()` with fallback to `ULyraTeamSubsystem`
- Damage application uses Lyra's damage execution system via `ULyraGameData::DamageGameplayEffect_SetByCaller`
- Cooldowns are handled by Blueprint gameplay effects assigned in the ability Blueprints

