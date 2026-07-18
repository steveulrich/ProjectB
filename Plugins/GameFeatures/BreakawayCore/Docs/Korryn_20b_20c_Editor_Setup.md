# Step 20b–20c — Korryn Cursed Ward + Parity (Editor Setup)

**Status:** C++ landed — awaiting rebuild, asset wiring, PIE, and 3/3 cold starts.

C++ lives in `HeroMorganRuntime`:

- `ABwayKorrynCursedWardBuildable` — 600 HP, 50% slow (`MoveSpeedMultiplier *= 0.5`), 6 m (600 uu), persists between rounds; aura sourced from ward ASC
- Kit-driven cooldowns / magnitudes / durations via `UBwayKorrynKitConfig` + `ApplyCooldown` override on `UBwayGameplayAbility_KorrynBase`
- Infinite `UGE_Bway_KorrynCursedWardSlow` + SetByCaller magnitudes for Burden/Circle/Ward slows and Circle damage amp

Shared placement stack (from Argus 18b / Alona 19b) is unchanged:

- `UBwayGameplayAbility_PlaceBuildable` (per-hero key **1**)
- Confirm / Cancel on `DA_BW_AbilitySet_Humanoid`
- Once-per-round free placement via `ABwayPlayerState`

**Related:** [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) · [Buildable_System.md](./Buildable_System.md) · [Korryn_20a_Editor_Setup.md](./Korryn_20a_Editor_Setup.md) · [CONTENT_SETUP.md](../../Heroes/Morgan/CONTENT_SETUP.md) · [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)

## Ability ownership (canonical)

| Scope | Ability set | Grants |
|-------|-------------|--------|
| **Common** | `DA_BW_AbilitySet_Humanoid` | Slide · RelicRequest · **Confirm** · **Cancel** |
| **Per-hero** | `DA_BW_AbilitySet_Hexweaver` | Combat kit (20a) · **PlaceBuildable** (`InputTag.Ability.Buildable`) |
| **Hero DA** | `DA_BW_HeroData_Hexweaver` | `BuildableDataAsset` → Hexweaver / Cursed Ward DA |

**Do not** put PlaceBuildable on the humanoid set. **Do not** put Confirm/Cancel on the per-hero set. **Do not** wipe the five combat grants when adding PlaceBuildable.

## Prerequisites

- Step **20a** combat kit wired (`Scripts/setup-korryn-20a-functional.mjs`)
- `Hero_Morgan` enabled on `B_BW_Experience_Dev`
- Editor closed → compile so `ABwayKorrynCursedWardBuildable` is available → restart Editor

## Automated wiring (preferred)

```
node Scripts/setup-korryn-20b-cursed-ward.mjs
node Scripts/setup-korryn-20b-cursed-ward.mjs --probe-only
```

20a reruns remain safe after 20b: they **preserve** Hexweaver / Cursed Ward `BuildableDataAsset` and seed Ward kit fields.

Logs: `AI_Planning/setup_korryn_20b_probe.json` / `setup_korryn_20b_probe_after.json`.

### Probe expectations

| Check | Expected |
|-------|----------|
| **HealthSet / MeshComponent** | non-null on Ward BP CDO (corrupt reparent was quarantined) |
| Ward CDO | `MaxHealth=600`, `SlowRadius=600`, `SlowMultiplier=0.5`, `bPersistsBetweenRounds=true` |
| Buildable DA | `Cost=0`, `MaxActive=3`, actor → Hexweaver BP |
| Ability set | **6** grants (5 combat + Buildable) |
| Hero DA | `BuildableDataAsset` → `DA_BW_BuildableData_Hexweaver` |
| Kit | Ward 600/600/0.5; CDs 22/14/20/30; Burden 33/0.3/2s/0.5; Circle 0.85/1.35/700/5; Aura 13/0.5/800/5 |

## Manual fallback — assets

### 1. Buildable data asset

**Path:** `/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_BuildableData_Hexweaver`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayBuildableDataAsset` |
| **BuildableActorClass** | `BP_BW_Hexweaver_Buildable` |
| **Cost** | `0` |
| **MaxActiveBuildablesPerPlayer** | `3` |

### 2. Cursed Ward actor BP

**Path:** `/Hero_Morgan/Characters/Heroes/Hexweaver/Buildable/BP_BW_Hexweaver_Buildable`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayKorrynCursedWardBuildable` |
| **MaxHealth** | `600` |
| **SlowRadius** | `600` (6 m) |
| **SlowMultiplier** | `0.5` |
| **bPersistsBetweenRounds** | `true` |
| **BuildTime** | `0` |
| **KitConfig** | `DA_BW_KorrynKitConfig` |

### 3. Hero DA

| Field | Value |
|-------|--------|
| **BuildableDataAsset** | `DA_BW_BuildableData_Hexweaver` |
| **DisplayName** | `Korryn` |

### 4. PlaceBuildable BP + grant

**Path:** `/Hero_Morgan/Characters/Heroes/Hexweaver/Abilities/GA_BW_Korryn_BuildablePlacement`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayGameplayAbility_PlaceBuildable` |
| **DisplayData → AbilityName** | `Cursed Ward` |

Append to `DA_BW_AbilitySet_Hexweaver` only:

| Input tag | Ability | Key |
|-----------|---------|-----|
| `InputTag.Ability.Buildable` | `GA_BW_Korryn_BuildablePlacement` | **1** |

## Kit parity values (20c)

| System | Sheet value (authoritative on kit DA) |
|--------|----------------------------------------|
| Flock CD | 22 s |
| Burden | 33 base + 0.3× Atk · 2 s · 50% slow · CD 14 s |
| Circle | 15% slow (×0.85) · +35% incoming (1.35) · 7 m · 5 s · CD 20 s |
| Aura | 13 base + 0.5× Atk · 8 m · 5 s silence · CD 30 s |
| Cursed Ward | 600 HP · 50% slow · 6 m |

At Attack Str **40**: Burden raw **45** (`33 + 40×0.3`); Aura raw **33** (`13 + 40×0.5`).

## PIE test

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Korryn&NumBots=7&PointsToWin=3&PostRoundDuration=3
```

**Listen server**, 1 player.

### Pass checklist (20b)

- [ ] Press **1** → placement preview ghost (valid/invalid tint)
- [ ] **LMB** valid → Cursed Ward spawns (**600 HP**)
- [ ] **LMB** invalid (red) → stays in placement
- [ ] **RMB** → cancels; no spawn
- [ ] Enemy in **6 m** is slowed ~50%; leaves radius → speed restores
- [ ] Allies in radius are **not** slowed
- [ ] Ward destroyed at 0 HP; slow handles clear
- [ ] Once per round: second placement blocked; next round allows again
- [ ] Ward **persists** into next round
- [ ] Relic-carrier still blocks combat kit (regression)

### Pass checklist (20c spot-check)

- [ ] Burden ~45 raw at Atk 40 + 2 s 50% slow
- [ ] Circle 15% slow / 35% amp / 7 m / 5 s
- [ ] Aura ~33 raw / 8 m / 5 s silence
- [ ] Cooldowns **22 / 14 / 20 / 30** (Flock / Burden / Circle / Aura)
- [ ] **3/3 consecutive Editor cold starts** with no unexpected errors

Only after 3/3 succeeds, mark Step 20 (20a–20c) passed in `CoreLoop_Implementation_Plan.md` with `core-loop: step 20c passed (Korryn parity)`.
