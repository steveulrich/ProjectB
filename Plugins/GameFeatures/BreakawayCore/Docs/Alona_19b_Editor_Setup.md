# Step 19b — Alona Sun Shrine (Editor Setup)

C++ landed in `HeroAlonaRuntime`:

- `ABwayAlonaSunShrineBuildable` — 750 HP, 35 HP/s ally heal, 5 m (500 uu) radius, non-stacking with other Sun Shrines, persists between rounds

Shared placement stack (from Argus 18b) is unchanged:

- `UBwayGameplayAbility_PlaceBuildable` (per-hero key **1**)
- Confirm / Cancel on `DA_BW_AbilitySet_Humanoid`
- Once-per-round free placement via `ABwayPlayerState`

Complete these Editor steps after recompiling. Full flow: [Buildable_System.md](./Buildable_System.md).

**Automated wiring (preferred when Editor + ue-mcp bridge are up):**

```
node Scripts/setup-alona-19b-sun-shrine.mjs
```

Probe only: `node Scripts/setup-alona-19b-sun-shrine.mjs --probe-only`  
Logs: `AI_Planning/setup_alona_19b_probe.json` / `setup_alona_19b_probe_after.json`.

## Ability ownership (canonical)

| Scope | Ability set | Grants |
|-------|-------------|--------|
| **Common** | `DA_BW_AbilitySet_Humanoid` | Slide · RelicRequest · **Confirm** · **Cancel** |
| **Per-hero** | `DA_BW_AbilitySet_Alona` | Combat kit (19a) · **PlaceBuildable** (`InputTag.Ability.Buildable`) |
| **Hero DA** | `DA_BW_HeroData_Alona` | `BuildableDataAsset` → Sun Shrine |

**Do not** put PlaceBuildable on the humanoid set. **Do not** put Confirm/Cancel on the per-hero set. **Do not** wipe 19a combat grants when adding PlaceBuildable.

## Prerequisites

- Step **19a** pass (Alona combat kit, relic carrier blocking, listen-server PIE)
- `Hero_Alona` enabled on `B_BW_Experience_Dev` (alongside `Hero_Spartacus` / BreakawayCore)
- Editor closed → compile so `ABwayAlonaSunShrineBuildable` is available → restart Editor

## 1. Create buildable data asset

**Path (recommended):** `/Hero_Alona/Characters/Heroes/Alona/Buildables/DA_BW_BuildableData_SunShrine`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayBuildableDataAsset` |
| **BuildableActorClass** | `BP_BW_Buildable_SunShrine` (step 2) |
| **Cost** | `0` (slice — no gold gate) |
| **MaxActiveBuildablesPerPlayer** | `3` (allows prior-round shrines if still alive) |
| **PrimaryBuildableMesh** | Shrine mesh for preview ghost (recommended) |

Legacy names (`DA_BW_Buildable_SunShrine`, `DA_BW_BuildableData_Alona`) may exist from older migrations — prefer the Argus-style path above and wire **singular** `BuildableDataAsset` only. Do **not** use Starlight / plural `BuildableDataAssets`.

## 2. Create Sun Shrine actor BP

**Path:** `/Hero_Alona/Characters/Heroes/Alona/Buildables/BP_BW_Buildable_SunShrine`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayAlonaSunShrineBuildable` |
| **MaxHealth** | `750` |
| **HealRadius** | `500` (5 m) |
| **HealPerSecond** | `35` |
| **bPersistsBetweenRounds** | `true` |
| **BuildTime** | `0` |
| **Mesh** | Shrine mesh + blocking collision for placement validation |

Legacy `Buildable/BP_BW_Alona_Buildable` can be reparented/duplicated into this path.

## 3. Wire `DA_BW_HeroData_Alona`

| Field | Value |
|-------|--------|
| **BuildableDataAsset** | `DA_BW_BuildableData_SunShrine` |
| **DisplayName** | `Alona` |

Without `BuildableDataAsset`, PlaceBuildable `CanActivateAbility` fails (no preview).

## 4. Per-hero PlaceBuildable

Add to `DA_BW_AbilitySet_Alona` (not humanoid):

| Input tag | Ability | Key |
|-----------|---------|-----|
| `InputTag.Ability.Buildable` | `GA_BW_Alona_BuildablePlacement` | **1** |

`GA_BW_Alona_BuildablePlacement` **setup:**

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayGameplayAbility_PlaceBuildable` |
| **DisplayData → AbilityName** | `Sun Shrine` |
| **DisplayData → Description** | Short heal-shrine description |
| **ValidPlacementMaterial** / **InvalidPlacementMaterial** | Translucent green/red (optional; drives ghost tint) |

**Do not strip C++ defaults in the BP:** keep `Ability.Buildable.PlacementSession`, `Ability.Buildable.PlacementExempt`, and empty `CancelAbilitiesWithTag` from the C++ parent.

**Do not put** `InputTag.*` **on AbilityTags** (Place/Confirm/Cancel). Input tags belong only on the ability-set **grant**.

## 5. Confirm / Cancel (humanoid)

Already wired in Step 18b — do not re-create. Dual-bind LMB Confirm / RMB Cancel remains on `DA_BW_AbilitySet_Humanoid`.

## 6. PIE test

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Alona&NumBots=7&PointsToWin=3&PostRoundDuration=3
```

**Listen server**, 1 player. Use a multi-round URL so you can test round rollover.

### Pass checklist (19b)

- [ ] Press **1** → placement preview ghost appears (valid/invalid tint)
- [ ] **LMB** on valid spot → Sun Shrine spawns (**750 HP**)
- [ ] **LMB** on invalid spot (red ghost) → stays in placement; can reposition
- [ ] **RMB** → cancels preview; no spawn
- [ ] Ally (or self) in **5 m** radius heals ~**35 HP/s** (damage first with `DamageSelf`, then stand in aura)
- [ ] Two overlapping shrines on same team → **one** 35/s heal, not 70/s
- [ ] **Once per round:** second placement in same round blocked
- [ ] **Round 2:** placement allowed again; surviving shrine **persists**
- [ ] Relic pickup / score regression still OK
- [ ] Argus regression: `Hero=Argus&NumBots=7` still places Siege Engine
- [ ] **3/3** cold-start listen-server PIE runs

### Failure modes (quick triage)

| Symptom | Likely cause |
|---------|--------------|
| **1** does nothing | Missing PlaceBuildable on Alona set; or empty `BuildableDataAsset`; class not compiled |
| Ghost never tints / missing mesh | Set `PrimaryBuildableMesh` or Valid/Invalid materials on PlaceBuildable BP |
| Spawns but never heals | Shrine parent not `BwayAlonaSunShrineBuildable`; or team ID unset; or still building |
| Double heal from two shrines | Non-stack logic not running — confirm C++ parent, not raw `ABuildableActor` |
| Round 2 place blocked by cap | Raise `MaxActiveBuildablesPerPlayer` to **3** |
| Confirm does nothing | Confirm BP stripped tags — restore from 18b |

## Deferred to 19c

| Item | Step |
|------|------|
| Radiance STR/52 heal scaling | 19c |
| Full sheet CD/damage/radius parity spot-check | 19c |
| Shrine VFX/SFX polish | optional |

## C++ class

| Piece | Class | Module |
|-------|-------|--------|
| Sun Shrine actor | `BwayAlonaSunShrineBuildable` | `HeroAlonaRuntime` |
| Placement ability | `BwayGameplayAbility_PlaceBuildable` (BP child) | `BreakawayCoreRuntime` |
