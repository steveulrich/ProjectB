# Step 21b — Rawlins Jail (Editor Setup)

**Status:** **Passed** (with 21a/21c).

C++ lives in `HeroRawlinsRuntime`:

- `ABwayRawlinsJailBuildable` (`ATrapBase`) — invulnerable one-shot floor trap; persists until first live enemy trigger; then captures nearby enemies and destroys itself
- `ABwayRawlinsJailCage` (`ABuildableActor`) — per-victim **450 HP** cage; attaches to victim; **non-persistent**; not PC-owned (registry once/round count unchanged)
- `UGE_Bway_RawlinsJailed` — infinite GE: `Gameplay.MovementStopped` + `State.Status.Jailed` (abilities remain usable)
- Kit fields on `UBwayRawlinsKitConfig`: `JailMaxHealth`, `JailTriggerRadius`, `JailCaptureRadius`, `JailCageRadius`
- `ABwayRawlinsBulletProjectile` damages enemy cages (team-filtered via cage `TeamId`)

Shared placement stack (Argus/Alona/Korryn) is unchanged:

- `UBwayGameplayAbility_PlaceBuildable` (per-hero key **1**)
- Confirm / Cancel on `DA_BW_AbilitySet_Humanoid`
- Once-per-round free placement via `ABwayPlayerState`

**Related:** [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) · [Buildable_System.md](./Buildable_System.md) · [Rawlins_21a_Editor_Setup.md](./Rawlins_21a_Editor_Setup.md) · [CONTENT_SETUP.md](../../Heroes/Rawlins/CONTENT_SETUP.md) · [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)

## Ability ownership (canonical)

| Scope | Ability set | Grants |
|-------|-------------|--------|
| **Common** | `DA_BW_AbilitySet_Humanoid` | Slide · RelicRequest · **Confirm** · **Cancel** |
| **Per-hero** | `DA_BW_AbilitySet_Gunslinger` | Combat kit (21a) · **PlaceBuildable** (`InputTag.Ability.Buildable`) |
| **Hero DA** | `DA_BW_HeroData_Gunslinger` | `BuildableDataAsset` → Gunslinger / Jail DA |

**Do not** put PlaceBuildable on the humanoid set. **Do not** put Confirm/Cancel on the per-hero set. **Do not** wipe the five combat grants when adding PlaceBuildable.

## Prerequisites

- Step **21a** combat kit wired (`Scripts/setup-rawlins-21a-functional.mjs`)
- `Hero_Rawlins` enabled on `B_BW_Experience_Dev`
- Editor **closed** → compile so `ABwayRawlinsJailBuildable` / `ABwayRawlinsJailCage` are available → cold restart Editor

## Automated wiring (preferred)

```
node Scripts/setup-rawlins-21b-jail.mjs
node Scripts/setup-rawlins-21b-jail.mjs --probe-only
```

21a reruns remain safe after 21b: they **preserve** Gunslinger / Jail `BuildableDataAsset` and seed Jail kit fields.

Logs: `AI_Planning/setup_rawlins_21b_probe.json` / `setup_rawlins_21b_probe_after.json`.

### Probe expectations

| Check | Expected |
|-------|----------|
| Native classes | `BwayRawlinsJailBuildable`, `BwayRawlinsJailCage` load |
| **HealthSet / MeshComponent** | non-null on trap + cage BP CDOs (corrupt legacy BP quarantined if needed) |
| Trap CDO | `TriggerRadius=300`, `CaptureRadius=450`, `bPersistsBetweenRounds=true`, `JailCageClass` → cage BP |
| Cage CDO | `MaxHealth=450`, `CageRadius=120`, `bPersistsBetweenRounds=false` |
| Buildable DA | `Cost=0`, `MaxActive=3`, actor → floor-trap BP |
| Ability set | **6** grants (5 combat + Buildable) |
| Hero DA | `BuildableDataAsset` → `DA_BW_BuildableData_Gunslinger` |
| Kit | Jail 450 / 300 / 450 / 120 |

## Manual fallback — assets

### 1. Buildable data asset

**Path:** `/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_BuildableData_Gunslinger`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayBuildableDataAsset` |
| **BuildableActorClass** | `BP_BW_Gunslinger_Buildable` (floor trap) |
| **Cost** | `0` |
| **MaxActiveBuildablesPerPlayer** | `3` |

### 2. Floor trap actor BP

**Path:** `/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/BP_BW_Gunslinger_Buildable`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayRawlinsJailBuildable` |
| **TriggerRadius** | `300` |
| **CaptureRadius** | `450` |
| **bPersistsBetweenRounds** | `true` |
| **BuildTime** | `0` |
| **JailCageClass** | `BP_BW_Gunslinger_JailCage` |
| **KitConfig** | `DA_BW_RawlinsKitConfig` |

### 3. Cage actor BP

**Path:** `/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/BP_BW_Gunslinger_JailCage`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayRawlinsJailCage` |
| **MaxHealth** | `450` |
| **CageRadius** | `120` |
| **bPersistsBetweenRounds** | `false` |
| **BuildTime** | `0` |
| **KitConfig** | `DA_BW_RawlinsKitConfig` |

### 4. Hero DA

| Field | Value |
|-------|--------|
| **BuildableDataAsset** | `DA_BW_BuildableData_Gunslinger` |
| **DisplayName** | `Rawlins` |

### 5. PlaceBuildable BP + grant

**Path:** `/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_BuildablePlacement`

| Field | Value |
|-------|--------|
| **Parent Class** | `BwayGameplayAbility_PlaceBuildable` |
| **DisplayData → AbilityName** | `Jail` |

Append to `DA_BW_AbilitySet_Gunslinger` only:

| InputTag | Ability |
|----------|---------|
| `InputTag.Ability.Buildable` | `GA_BW_Rawlins_BuildablePlacement` |

## PIE gate (listen server)

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Rawlins&NumBots=7
```

- [ ] Key **1** preview; valid **LMB** confirm; invalid confirm keeps preview; **RMB** cancel
- [ ] Free / once-per-round; round-2 placement reset
- [ ] First enemy consumes floor trap; cages spawn for every nearby enemy; allies do not trigger
- [ ] Each cage **450 HP**; prisoners cannot move/turn but can fire abilities
- [ ] Destroying one cage frees only its victim; victim death removes its cage
- [ ] Untriggered traps persist across rounds; triggered cages cleanly release on round reset
- [ ] Re-run `setup-rawlins-21a-functional.mjs --probe-only` — Jail wiring + six grants preserved
- [ ] Combat / relic smoke still OK
- [ ] **3/3** cold starts — no Asset Manager / subobject / delegate / GE leaks

## Deferred to 21c

Formal sheet parity (CDs, damage/scaling, Jail HP) is enforced by [Rawlins_21c_Editor_Setup.md](./Rawlins_21c_Editor_Setup.md) / `Scripts/setup-rawlins-21c-parity.mjs`.

Jail radii (`300 / 450 / 120`) and combat feel knobs remain **implementation defaults** — the stats sheet does not define replacements. Do not retune them in 21b.

Only after the checklist passes, mark Step 21b complete in `CoreLoop_Implementation_Plan.md`.
