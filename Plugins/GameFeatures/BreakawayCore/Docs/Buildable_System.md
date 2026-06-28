# Buildable System

Persistent arena structures purchased with gold.

## Core Types

| Type | Role |
|------|------|
| `ABuildableActor` | Base — build time, team, persistence flag |
| `ATurretBase` | AI perception auto-target |
| `ATrapBase` | Overlap trigger trap |
| `UBwayBuildableDataAsset` | Class, cost, mesh, per-player cap |
| `UBwayBuildablePlacementLibrary` | Validate, spawn, gold spend |
| `UBwayBuildableRegistryComponent` | Server registry for match-wide tracking |

## Persistence

`ABuildableActor::bPersistsBetweenRounds` defaults **true**.

On `UBwayRoundManagementComponent::ResetRoundState`:

- Non-persisting buildables destroyed
- Persisting buildables remain in world

## Placement Flow

1. Player activates placement ability (GAS targeting — `ABwayTargetActor_ActorPlacementFace`)
2. `CanPlayerPlaceBuildable` — gold check, `MaxActiveBuildablesPerPlayer` cap
3. `SpawnBuildableForPlayer` — spawn, `InitializeBuildable`, deduct gold
4. `InitializeBuildable` → registers with `UBwayBuildableRegistryComponent`

## Hero → Buildable Data

`UBwayHeroDataAsset`:

- `BuildableDataAssets[]` — up to 2 per hero (design spec)
- Legacy `BuildableDataAsset` still supported via `GetAllBuildableDataAssets()`

## 2-per-Hero Matrix (Vertical Slice)

| Hero | Buildable 1 | Buildable 2 |
|------|-------------|-------------|
| Spartacus | Fire Catapult (turret) | Dragon Spire (trap) |
| Morgan | Elder Stone | Tome of Frailty |
| Alona | Sun Shrine | Starlight |
| Rawlins | Cage | Boom Box |

Content: BP subclasses of `ATurretBase` / `ATrapBase` + data assets.

## Registry API

```cpp
RegisterBuildable / UnregisterBuildable  // server
GetAllBuildables / GetBuildablesForTeam
GetBuildableCountForPlayer               // placement cap
```

Replicated `RegisteredBuildables` array — late joiners receive existing replicated buildable actors in world; registry supports queries.

## Between-Round Spend

Listen to `UBwayRoundManagementComponent::OnPostRoundSummaryStarted` for between-round UI (Step 15). Legacy `OnBetweenRoundPlanningStarted` is deprecated.

## Networking

- Buildables `bReplicates = true`
- Team ID replicated via `ABwayActorWithAbilities`
- Registry is server-authoritative list

## TODO

- Dedicated planning widget (BP)
- Destroyed buildable cleanup in registry on death
- Buildable save across match only (no SaveGame)
