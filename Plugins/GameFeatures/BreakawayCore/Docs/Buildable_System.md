# Buildable System

Persistent arena structures. **Vertical slice:** one buildable per hero, **once per round**, **free** (no gold). Gold spend is deferred to post-slice stat enhancers/items — see [Economy_Gold.md](./Economy_Gold.md).

**Editor wiring (Argus):** [Argus_18b_Editor_Setup.md](./Argus_18b_Editor_Setup.md)  
**Editor wiring (Alona):** [Alona_19b_Editor_Setup.md](./Alona_19b_Editor_Setup.md)

## Core Types

| Type | Role |
|------|------|
| `ABuildableActor` | Base — build time, team, persistence flag |
| `ABwaySiegeEngineBuildable` | Argus rolling barrel — buildable-only DPS, 10s roll |
| `ABwayAlonaSunShrineBuildable` | Alona heal shrine — 750 HP, 35 HP/s ally aura, 5 m, non-stack |
| `ATurretBase` | AI perception auto-target |
| `ATrapBase` | Overlap trigger trap |
| `UBwayBuildableDataAsset` | Class, cost (unused for slice), mesh, per-player cap |
| `UBwayBuildablePlacementLibrary` | Validate, spawn, resolve hero buildable |
| `UBwayGameplayAbility_PlaceBuildable` | Per-hero key-1 placement start + preview ghost |
| `UBwayGameplayAbility_ConfirmBuildablePlacement` | LMB confirm forwarder (humanoid set) |
| `UBwayGameplayAbility_CancelBuildablePlacement` | RMB cancel forwarder (humanoid set) |
| `UBwayGameplayAbility_RelicRequest` | Request Relic — blocked during placement |
| `Ability.Buildable.PlacementSession` | Asset tag on PlaceBuildable — cancel target for other abilities |
| `Ability.Buildable.PlacementExempt` | Confirm / Cancel / start — must not cancel session on activate |
| `State.BuildablePlacement` | Owned while preview active; blocks Primary + RelicRequest |
| `ABwayTargetActor_ActorPlacementFace` | Ground trace + placement validation + ghost |
| `UBwayBuildableRegistryComponent` | Server registry for match-wide tracking |

## Ability ownership

| Scope | Set | Grants |
|-------|-----|--------|
| Common | `DA_BW_AbilitySet_Humanoid` | Confirm · Cancel · (Slide · RelicRequest) |
| Per-hero | Hero ability set | PlaceBuildable (`InputTag.Ability.Buildable`) |
| Hero DA | `UBwayHeroDataAsset::BuildableDataAsset` | Which actor/data spawns |

## Persistence

`ABuildableActor::bPersistsBetweenRounds` defaults **true**.

On `UBwayRoundManagementComponent::ResetRoundState`:

- Non-persisting buildables destroyed
- Persisting buildables remain in world
- Per-player once-per-round placement flag resets on round start

## Placement Flow

1. Player presses **1** → per-hero `UBwayGameplayAbility_PlaceBuildable` (`InputTag.Ability.Buildable` on hero ability set)
2. Ability applies `State.BuildablePlacement` and shows preview ghost via `WaitTargetData` (`UserConfirmed`)
3. While placing, **Primary** and **Request Relic** are blocked (`ActivationBlockedTags` → `State.BuildablePlacement`). **LMB/RMB** dual-bind to Confirm/Cancel. Any other `UBwayGameplayAbility` cancels the session via default `CancelAbilitiesWithTag` → `Ability.Buildable.PlacementSession`
4. **LMB** → Confirm → `LocalInputConfirm` → authority spawn if valid
5. **RMB** → Cancel → `LocalInputCancel` → exit preview, no spawn
6. `CanPlayerPlaceBuildable` — once-per-round check, `MaxActiveBuildablesPerPlayer` cap (**no gold gate** for slice)
7. `SpawnBuildableForPlayer` — spawn, `InitializeBuildable` → registers with `UBwayBuildableRegistryComponent`

Invalid confirm (red ghost) keeps preview active; player can reposition and retry.

## Hero → Buildable Data (Section 3 slice)

`UBwayHeroDataAsset::BuildableDataAsset` — **one** buildable per hero.

| Hero (DisplayName) | Plugin / folder | Buildable |
|--------------------|-----------------|-----------|
| Argus | `Hero_Spartacus` / `Argus` | **Siege Engine** — 250 HP; 200 dmg/s vs buildables; 10s roll · [18b](./Argus_18b_Editor_Setup.md) |
| Alona | `Hero_Alona` | **Sun Shrine** — 750 HP, 35 HP/s, 5m, non-stack · [19b](./Alona_19b_Editor_Setup.md) |
| Korryn | `Hero_Morgan` / `Hexweaver` | **Cursed Ward** — 600 HP, 50% slow, 6m · [20b/20c](./Korryn_20b_20c_Editor_Setup.md) |
| Rawlins | `Hero_Rawlins` / `Gunslinger` | **Jail** — 450 HP, cage trap |

Authority: [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md).

## Registry API

```cpp
RegisterBuildable / UnregisterBuildable  // server
GetAllBuildables / GetBuildablesForTeam
GetBuildableCountForPlayer               // placement cap
```

Replicated `RegisteredBuildables` array — late joiners receive existing replicated buildable actors; registry supports queries.

## Between-Round Spend

Listen to `UBwayRoundManagementComponent::OnPostRoundSummaryStarted` for between-round UI (Step 15). Legacy `OnBetweenRoundPlanningStarted` is deprecated. Slice does **not** spend gold on buildables.

## Networking

- Buildables `bReplicates = true`
- Team ID replicated via `ABwayActorWithAbilities`
- Registry is server-authoritative
- Confirm is local; spawn is authority-only in PlaceBuildable

## TODO

- Dedicated planning widget (BP)
- Destroyed buildable cleanup in registry on death
- Post-slice: gold spend / multi-buildable heroes if design returns
