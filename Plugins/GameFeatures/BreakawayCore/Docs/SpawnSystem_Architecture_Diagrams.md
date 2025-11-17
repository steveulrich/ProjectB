# Breakaway Spawn System - Visual Architecture

## System Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                      LEVEL (World)                               │
│                                                                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ BwaySpawnPt  │  │ BwaySpawnPt  │  │ BwaySpawnPt  │          │
│  │              │  │              │  │              │          │
│  │ Tag: Relic   │  │ Tag: Goal.T1 │  │ Tag: Goal.T2 │          │
│  │ Team: -1     │  │ Team: 0      │  │ Team: 1      │          │
│  │ Data: ──┐    │  │ Data: ──┐    │  │ Data: ──┐    │          │
│  └──────────┼────┘  └──────────┼────┘  └──────────┼────┘          │
│             │                  │                  │               │
└─────────────┼──────────────────┼──────────────────┼───────────────┘
              │                  │                  │
              ▼                  ▼                  ▼
       ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
       │ RelicSpawn   │  │ GoalSpawnT1  │  │ GoalSpawnT2  │
       │ DataAsset    │  │ DataAsset    │  │ DataAsset    │
       │              │  │              │  │              │
       │ ActorClass ─────►BP_Relic     │  │ ActorClass ─────►BP_Goal
       │ Settings ────────►DA_Settings  │  │ GoalExtent   │
       └──────────────┘  └──────────────┘  └──────────────┘

                    ┌─────────────────────────┐
                    │   SERVER (Authority)    │
                    └─────────────────────────┘
                               │
                               ▼
                    ┌─────────────────────────┐
                    │  BreakawayGameMode      │
                    │                         │
                    │  InitGameState()        │
                    │    └─► Creates Manager  │
                    │                         │
                    │  BeginPlay()            │
                    │    ├─► Init Tags        │
                    │    └─► Spawn Objects    │
                    └───────────┬─────────────┘
                                │
                                ▼
                    ┌─────────────────────────┐
                    │    BwayGameState        │
                    │                         │
                    │  Components:            │
                    │  └─► SpawnPointManager ◄┼─────┐
                    │                         │     │
                    │  References:            │     │
                    │  ├─► RelicActor         │     │
                    │  ├─► Team Info          │     │
                    │  └─► Scores             │     │
                    └─────────────────────────┘     │
                                                    │
                ┌───────────────────────────────────┘
                │
                ▼
    ┌────────────────────────────────────────┐
    │  BwaySpawnPointManagerComponent        │
    │                                        │
    │  ┌──────────────────────────────────┐ │
    │  │ Discovery Phase (BeginPlay)      │ │
    │  │ ├─► Find all BwaySpawnPoint      │ │
    │  │ ├─► Register each point          │ │
    │  │ └─► Build lookup tables          │ │
    │  └──────────────────────────────────┘ │
    │                                        │
    │  ┌──────────────────────────────────┐ │
    │  │ Cached Data Structures           │ │
    │  │ ├─► RegisteredSpawnPoints[]      │ │
    │  │ ├─► SpawnPointsByTag{}           │ │
    │  │ └─► SpawnPointsByTeam{}          │ │
    │  └──────────────────────────────────┘ │
    │                                        │
    │  ┌──────────────────────────────────┐ │
    │  │ API Functions                    │ │
    │  │ ├─► GetSpawnPointsByTag()        │ │
    │  │ ├─► GetRandomSpawnPoint()        │ │
    │  │ ├─► SpawnObjectsAtPoints()       │ │
    │  │ └─► ResetAllSpawnPoints()        │ │
    │  └──────────────────────────────────┘ │
    └────────────────────────────────────────┘
                    │
                    │ Spawns
                    ▼
        ┌───────────────────────┐
        │   Game Objects        │
        │                       │
        │  ┌────────────────┐   │
        │  │  RelicActor    │   │
        │  └────────────────┘   │
        │  ┌────────────────┐   │
        │  │  GoalVolume    │   │
        │  │  (Team 1)      │   │
        │  └────────────────┘   │
        │  ┌────────────────┐   │
        │  │  GoalVolume    │   │
        │  │  (Team 2)      │   │
        │  └────────────────┘   │
        └───────────────────────┘
```

## Spawn Point Lifecycle

```
┌──────────────────────────────────────────────────────────────────┐
│                    SPAWN POINT LIFECYCLE                          │
└──────────────────────────────────────────────────────────────────┘

1. LEVEL LOAD
   │
   ├─► Spawn points placed in level editor
   │   └─► Properties configured (Tag, Team, Data Asset)
   │
   ▼

2. GAME START (Server Only)
   │
   ├─► GameMode::InitGameState()
   │   └─► Create SpawnPointManagerComponent on GameState
   │
   ├─► GameMode::BeginPlay()
   │   ├─► InitializeSpawnPointTags()
   │   └─► SpawnInitialGameObjects()
   │       └─► Manager::SpawnObjectsAtPoints(Tag)
   │
   ▼

3. DISCOVERY
   │
   ├─► Manager::DiscoverSpawnPoints()
   │   ├─► Iterate all BwaySpawnPoint actors in world
   │   ├─► RegisterSpawnPoint() for each
   │   └─► RebuildLookupTables()
   │
   ▼

4. AUTO-SPAWNING (if enabled)
   │
   ├─► For each spawn point:
   │   ├─► Check bAutoSpawnOnBeginPlay
   │   ├─► Call GetClassToSpawn()
   │   │   └─► Returns ActorClass from SpawnData
   │   ├─► SpawnActor() at spawn transform
   │   ├─► Initialize spawned actor
   │   │   └─► SpawnData::InitializeSpawnedActor()
   │   └─► Store reference
   │
   ▼

5. RUNTIME
   │
   ├─► Queries (anytime)
   │   ├─► GetSpawnPointsByTag()
   │   ├─► GetRandomSpawnPoint()
   │   └─► GetClosestSpawnPoint()
   │
   ├─► Manual Spawning (game logic)
   │   └─► SpawnPoint::SpawnObject()
   │
   ├─► Despawning (when object destroyed/consumed)
   │   └─► SpawnPoint::OnSpawnedObjectDestroyed()
   │       └─► Schedule respawn if enabled
   │
   ▼

6. ROUND RESET
   │
   ├─► GameMode::ResetRoundState()
   │   └─► Manager::ResetAllSpawnPoints(Tag)
   │       ├─► Despawn existing objects
   │       └─► Spawn new objects
   │
   ▼

7. CLEANUP
   │
   └─► Manager::EndPlay()
       ├─► Clear all references
       └─► Allow garbage collection
```

## Tag Hierarchy

```
SpawnPoint (Base)
│
├─► SpawnPoint.Relic
│   └─► Neutral spawn for relic/ball
│
├─► SpawnPoint.Goal
│   ├─► SpawnPoint.Goal.Team1 (Blue)
│   └─► SpawnPoint.Goal.Team2 (Red)
│
├─► SpawnPoint.Powerup
│   ├─► SpawnPoint.Powerup.Health
│   ├─► SpawnPoint.Powerup.Speed
│   ├─► SpawnPoint.Powerup.Damage
│   └─► SpawnPoint.Powerup.Shield
│
├─► SpawnPoint.Buildable
│   ├─► SpawnPoint.Buildable.Turret
│   ├─► SpawnPoint.Buildable.Shrine
│   └─► SpawnPoint.Buildable.Wall
│
├─► SpawnPoint.Player
│   ├─► SpawnPoint.Player.Team1
│   └─► SpawnPoint.Player.Team2
│
└─► SpawnPoint.Bot
    ├─► SpawnPoint.Bot.Team1
    └─► SpawnPoint.Bot.Team2
```

## Class Hierarchy

```
UObject
│
├─► AActor
│   ├─► ABwaySpawnPoint
│   │   └─► Properties: Tag, Team, Data, AutoSpawn
│   │
│   ├─► ARelicActor (spawned by system)
│   │
│   └─► ABwayGoalVolume (spawned by system)
│
└─► UActorComponent
    └─► UGameStateComponent
        └─► UBwaySpawnPointManagerComponent
            └─► Manages all spawn points

UDataAsset
│
└─► UPrimaryDataAsset
    └─► UBwaySpawnPointData
        ├─► UBwayRelicSpawnData
        └─► UBwayGoalSpawnData
```

## Data Flow: Spawning a Relic

```
1. GameMode calls SpawnInitialGameObjects()
                │
                ▼
2. Manager->SpawnObjectsAtPoints(RelicTag)
                │
                ├─► Query: GetSpawnPointsByTag(RelicTag)
                │         └─► Returns array of matching spawn points
                │
                ▼
3. For each spawn point:
   │
   ├─► SpawnPoint->SpawnObject()
   │         │
   │         ├─► GetClassToSpawn() 
   │         │     └─► SpawnData->GetActorClass()
   │         │           └─► Returns BP_RelicActor
   │         │
   │         ├─► World->SpawnActor<AActor>(
   │         │       BP_RelicActor,
   │         │       SpawnPoint->GetSpawnTransform()
   │         │     )
   │         │
   │         ├─► Store spawned actor reference
   │         │
   │         └─► SpawnData->InitializeSpawnedActor()
   │                 └─► Cast to ARelicActor
   │                 └─► RelicActor->InitializeRelicData(Settings)
   │
   └─► Returns SpawnedActor
                │
                ▼
4. GameState->SetRelicActor(SpawnedRelic)
                │
                ▼
5. Relic is now active in the game!
```

## Team-Based Spawning

```
Team 1 (Index 0)                    Team 2 (Index 1)
     │                                    │
     │                                    │
     ├─► SpawnPoint.Player.Team1          ├─► SpawnPoint.Player.Team2
     │   └─► Player spawns here           │   └─► Player spawns here
     │                                    │
     ├─► SpawnPoint.Goal.Team1            ├─► SpawnPoint.Goal.Team2
     │   └─► Opponent scores here         │   └─► Opponent scores here
     │       (Team 2 attacks this)        │       (Team 1 attacks this)
     │                                    │
     └─► SpawnPoint.Bot.Team1             └─► SpawnPoint.Bot.Team2
         └─► Bot spawns here                  └─► Bot spawns here

         SpawnPoint.Relic (Neutral)
                │
                └─► Team: -1
                    └─► Available to both teams
```

## Query Performance

```
GetSpawnPointsByTag():
    Best Case:  O(1) - Direct map lookup
    Worst Case: O(n) - Linear search if not cached
    Memory:     O(k) - k = unique tags

GetSpawnPointsByTeam():
    Best Case:  O(1) - Direct map lookup
    Worst Case: O(n) - Linear search if not cached
    Memory:     O(t) - t = number of teams

GetRandomSpawnPoint():
    Time:       O(1) after filtering
    Uses:       GetSpawnPointsByTag() + random index

GetClosestSpawnPoint():
    Time:       O(n) - Must check all matching points
    Space:      O(1) - No extra allocation
```

---

This visual documentation should help you understand how all the pieces fit together!
