# Step 18b — Argus Siege Engine (Editor Setup)

C++ landed in `BreakawayCoreRuntime`:

- `ABwaySiegeEngineBuildable`
- `UBwayGameplayAbility_PlaceBuildable` (per-hero key **1**)
- `UBwayGameplayAbility_ConfirmBuildablePlacement` / `CancelBuildablePlacement` (humanoid)
- `UBwayGameplayAbility_RelicRequest` (blocked during placement)
- Preview ghost via `ABwayTargetActor_ActorPlacementFace` + `ABwayWorldReticle_ActorVisualization`

Complete these Editor steps after recompiling. Full flow: [Buildable_System.md](./Buildable_System.md).

**Automated wiring (preferred when Editor + ue-mcp bridge are up):**

```
node Scripts/setup-argus-18b-placement-confirm-cancel.mjs
```

Probe only: `node Scripts/setup-argus-18b-placement-confirm-cancel.mjs --probe-only`  
Logs: `AI_Planning/setup_argus_18b_probe.json` / `setup_argus_18b_probe_after.json`.

## Ability ownership (canonical)

| Scope | Ability set | Grants |
|-------|-------------|--------|
| **Common** | `DA_BW_AbilitySet_Humanoid` | Slide · RelicRequest · **Confirm** · **Cancel** |
| **Per-hero** | `DA_BW_AbilitySet_Argus` | Combat kit · **PlaceBuildable** (`InputTag.Ability.Buildable`) |
| **Hero DA** | `DA_BW_HeroData_Argus` | **`BuildableDataAsset`** → Siege Engine |

**Do not** put PlaceBuildable on the humanoid set. **Do not** put Confirm/Cancel on the per-hero set.

Unused tag (ignore): `InputTag.Ability.Buildable.Start` — not wired; Confirm/Cancel use `.Confirm` / `.Cancel`.

## Prerequisites

- Step **18a** pass (Argus combat kit, relic carrier blocking, listen-server PIE)
- **`Hero_Spartacus`** enabled on `B_BW_Experience_Dev`

## 1. Create buildable data asset

**Path (recommended):** `/Hero_Spartacus/Characters/Heroes/Argus/Buildables/DA_BW_BuildableData_SiegeEngine`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayBuildableDataAsset` |
| **BuildableActorClass** | `BP_BW_Buildable_SiegeEngine` (step 2) |
| **Cost** | `0` (slice — no gold gate) |
| **MaxActiveBuildablesPerPlayer** | `3` (allows prior-round barrels if still alive) |
| **PrimaryBuildableMesh** | Barrel mesh for preview ghost (recommended) |

## 2. Create Siege Engine actor BP

**Path:** `/Hero_Spartacus/Characters/Heroes/Argus/Buildables/BP_BW_Buildable_SiegeEngine`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwaySiegeEngineBuildable` |
| **MaxHealth** | `250` |
| **BuildableDamagePerSecond** | `200` |
| **MaxRollDuration** | `10` |
| **RollSpeed** | `900` (tune feel) |
| **DamageRadius** | `150` |
| **bPersistsBetweenRounds** | `true` |
| **BuildTime** | `0` |
| **Mesh** | Barrel mesh + blocking collision (roll sweep stops on walls) |

## 3. Wire `DA_BW_HeroData_Argus`

| Field | Value |
|-------|--------|
| **BuildableDataAsset** | `DA_BW_BuildableData_SiegeEngine` |
| **DisplayName** | `Argus` |

Without `BuildableDataAsset`, PlaceBuildable `CanActivateAbility` fails (no preview).

## 4. Per-hero PlaceBuildable

Add to **`DA_BW_AbilitySet_Argus`** (not humanoid):

| Input tag | Ability | Key |
|-----------|---------|-----|
| `InputTag.Ability.Buildable` | `GA_BW_Spartacus_BuildablePlacement` | **1** |

**`GA_BW_Spartacus_BuildablePlacement` setup:**

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayGameplayAbility_PlaceBuildable` |
| **DisplayData → AbilityName** | `Siege Engine` |
| **DisplayData → Description** | Short buildable description |
| **DisplayData → Icon** | Buildable ability icon |
| **ValidPlacementMaterial** / **InvalidPlacementMaterial** | Translucent green/red (on ability CDO — drives ghost tint) |

Remove `InputTag.Ability.Buildable` from **`DA_BW_AbilitySet_Humanoid`** if still present (one grant per tag).

**Do not strip C++ defaults in the BP:** keep `Ability.Buildable.PlacementSession`, `Ability.Buildable.PlacementExempt`, and empty `CancelAbilitiesWithTag` from the C++ parent.

**Do not put `InputTag.*` on AbilityTags** (Place/Confirm/Cancel). Input tags belong only on the ability-set **grant** (`DynamicSpecSourceTags`). Putting `InputTag.Ability.Buildable` on Place AbilityTags hierarchically blocks Confirm/Cancel via `AreAbilityTagsBlocked` (BF-009).

Preview resolves `PlacedActorClass` from the locked hero's `BuildableDataAsset`.

## 5. Confirm / Cancel (humanoid)

C++ forwarders call `ASC->LocalInputConfirm()` / `LocalInputCancel()` — **no spawn logic** here. Spawn is authority-only inside PlaceBuildable `OnTargetDataValid`.

### Input — dual-bind (add, do not replace)

On **`DA_BW_InputData_Humanoid`** / AbilityInputConfig / IMC:

| Key | Keep | **Add** |
|-----|------|---------|
| **LMB** | `InputTag.Ability.Primary` | `InputTag.Ability.Buildable.Confirm` |
| **RMB** | `InputTag.Ability.RelicRequest` | `InputTag.Ability.Buildable.Cancel` |

Same physical key fires both tags. During placement, Primary and RelicRequest are blocked (`ActivationBlockedTags` → `State.BuildablePlacement`); Confirm/Cancel require that state and activate.

### Ability set (`DA_BW_AbilitySet_Humanoid`)

| Input tag | Ability | Parent class |
|-----------|---------|--------------|
| `InputTag.Ability.Buildable.Confirm` | `GA_BW_BuildablePlacement_Confirm` | `BwayGameplayAbility_ConfirmBuildablePlacement` |
| `InputTag.Ability.Buildable.Cancel` | `GA_BW_BuildablePlacement_Cancel` | `BwayGameplayAbility_CancelBuildablePlacement` |

**Do not strip C++ defaults:** Confirm/Cancel must keep `ActivationRequiredTags = State.BuildablePlacement` and `Ability.Buildable.PlacementExempt`. EventGraphs can stay empty.

Reparent **`GA_BW_Relic_Request`** to **`BwayGameplayAbility_RelicRequest`** so Request Relic is blocked while the preview is active.

Ensure Primary BP parent is **`BwayGameplayAbility_MeleePrimary`** (or otherwise has `ActivationBlockedTags` → `State.BuildablePlacement` and does **not** cancel the placement session).

### Runtime behavior (expected)

| Input | Result |
|-------|--------|
| **1** | PlaceBuildable activates; owns `State.BuildablePlacement`; ghost appears |
| **LMB** valid | Confirm → `LocalInputConfirm` → authority spawn → session ends |
| **LMB** invalid (red ghost) | Confirm runs but target actor does **not** broadcast → preview stays open |
| **RMB** | Cancel → `LocalInputCancel` → session ends; no spawn |
| **F / Q / E / R / Shift** (other `UBwayGameplayAbility`) | Cancels `Ability.Buildable.PlacementSession`, then runs |
| Confirm/Cancel outside placement | No-op (`ActivationRequiredTags` fail) |

**Net note (listen server):** Confirm is local; spawn runs on authority in `OnTargetDataValid`. On a pure client, ending the local ability without seeing a spawn until replication is normal — use listen-server PIE for 18b pass.

## 6. PIE test

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Argus&NumBots=7
```

**Listen server**, 1 player. Use a long round or dev URL so you can test round rollover.

### Pass checklist (18b)

- [ ] Press **1** → placement preview ghost appears (valid/invalid tint)
- [ ] **LMB** on valid spot → Siege Engine spawns and **rolls forward**
- [ ] **LMB** on invalid spot (red ghost) → stays in placement; can reposition
- [ ] **RMB** → cancels preview; no spawn
- [ ] During placement: **LMB does not Primary**; **RMB does not Request Relic**
- [ ] During placement: **F** (or other hero ability) **cancels preview** and runs that ability
- [ ] After placement ends: Primary and Request Relic work normally
- [ ] Rolling barrel damages **enemy buildables** (~200 DPS) for up to **10s**, then destroys itself
- [ ] Siege Engine has **250 HP** if attacked before/during roll
- [ ] **Once per round:** second placement in same round blocked
- [ ] **Round 2:** placement allowed again; surviving barrels **persist**
- [ ] Relic pickup / score regression still OK
- [ ] **3/3** cold-start PIE runs

### Failure modes (quick triage)

| Symptom | Likely cause |
|---------|----------------|
| **1** does nothing | Missing PlaceBuildable on Argus set; or empty `BuildableDataAsset`; or still granted on humanoid only |
| Ghost never tints / missing mesh | Set `PrimaryBuildableMesh` or Valid/Invalid materials on PlaceBuildable BP |
| LMB still Primary during preview | Primary missing `ActivationBlockedTags`; or Confirm not granted / not dual-bound |
| RMB still Request Relic during preview | RelicRequest not reparented to `BwayGameplayAbility_RelicRequest`; or Cancel not dual-bound |
| Confirm does nothing | Confirm BP stripped `ActivationRequiredTags`; or PlaceBuildable never applied `State.BuildablePlacement` |
| Spawns on invalid spot | Unexpected — C++ should withhold target data; check custom target-actor BP overrides |

## Deferred to 18c

| Item | Step |
|------|------|
| Sheet-accurate ability kits (No Retreat, For Glory, Retribution, Argus defense Slide) | 18c |
| Cooldown/damage parity | 18c |
| Siege Engine VFX/SFX polish | 18c |
