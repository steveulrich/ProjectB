# Breakaway Spawn Point System

## Overview

The Breakaway Spawn Point System is a flexible, data-driven approach to spawning game objects (relics, goals, powerups, etc.) in your levels. It follows Lyra's architecture patterns using gameplay tags, data assets, and component-based design.

## Architecture

### Core Components

1. **ABwaySpawnPoint** - Tagged actor placed in levels to mark spawn locations
2. **UBwaySpawnPointData** - Data asset defining what to spawn and how
3. **UBwaySpawnPointManagerComponent** - Game state component managing all spawn points
4. **Integration with BreakawayGameMode** - Seamless integration with existing game logic

### Key Features

- ✅ **Gameplay Tag-Based**: Uses gameplay tags for flexible filtering and querying
- ✅ **Data-Driven**: Configuration via data assets for designer-friendly workflows
- ✅ **Team Support**: Built-in team affiliation for team-based spawning
- ✅ **Respawn System**: Optional auto-respawn for powerups and collectibles
- ✅ **Debug Visualization**: Editor and runtime visualization for easy level design
- ✅ **Networked**: Designed for server-authoritative spawning with replication

## Quick Start Guide

### 1. Add Gameplay Tags

Add these gameplay tags to your project (via `Config/Tags/BreakawayCore.ini`):

```ini
[/Script/GameplayTags.GameplayTagsList]
+GameplayTagList=(Tag="SpawnPoint.Relic",DevComment="Relic spawn point")
+GameplayTagList=(Tag="SpawnPoint.Goal",DevComment="Goal spawn point (parent)")
+GameplayTagList=(Tag="SpawnPoint.Goal.Team1",DevComment="Team 1 goal spawn")
+GameplayTagList=(Tag="SpawnPoint.Goal.Team2",DevComment="Team 2 goal spawn")
+GameplayTagList=(Tag="SpawnPoint.Powerup",DevComment="Powerup spawn (parent)")
+GameplayTagList=(Tag="SpawnPoint.Powerup.Health",DevComment="Health powerup spawn")
+GameplayTagList=(Tag="SpawnPoint.Powerup.Speed",DevComment="Speed powerup spawn")
```

### 2. Create Spawn Data Assets

#### Relic Spawn Data

1. In Content Browser, right-click → Miscellaneous → Data Asset
2. Choose `BwayRelicSpawnData`
3. Name it `DA_RelicSpawn`
4. Configure:
   - **Actor Class**: Your relic blueprint (e.g., `BP_BW_RelicActor`)
   - **Spawn Type**: `SpawnPoint.Relic`
   - **Relic Settings**: Reference to your `URelicSettings` asset

#### Goal Spawn Data

1. Create data asset of type `BwayGoalSpawnData`
2. Name it `DA_GoalSpawn_Team1` and `DA_GoalSpawn_Team2`
3. Configure:
   - **Actor Class**: `ABwayGoalVolume` (or your goal blueprint)
   - **Spawn Type**: `SpawnPoint.Goal.Team1` (or Team2)
   - **Goal Extent**: Size of the goal trigger box (e.g., 100, 200, 200)
   - **Use Spawn Point Team**: Checked

### 3. Place Spawn Points in Level

1. In level editor, search for `BwaySpawnPoint` in Place Actors panel
2. Drag spawn points into your level
3. Select spawn point and configure in Details panel:
   - **Spawn Point Tag**: Choose appropriate tag (e.g., `SpawnPoint.Relic`)
   - **Spawn Data**: Select your data asset (e.g., `DA_RelicSpawn`)
   - **Team Index**: Set team for team-specific spawns (-1 for neutral)
   - **Auto Spawn On Begin Play**: Usually checked
   - **Show Debug Visualization**: Checked during development

#### Example Setup

**Relic Spawn:**
- Tag: `SpawnPoint.Relic`
- Data: `DA_RelicSpawn`
- Team: -1 (Neutral)
- Auto Spawn: ✓

**Team 1 Goal:**
- Tag: `SpawnPoint.Goal.Team1`
- Data: `DA_GoalSpawn_Team1`
- Team: 0
- Auto Spawn: ✓

**Team 2 Goal:**
- Tag: `SpawnPoint.Goal.Team2`
- Data: `DA_GoalSpawn_Team2`
- Team: 1
- Auto Spawn: ✓

### 4. GameMode Integration

The `ABreakawayGameMode` automatically:
- Creates a `UBwaySpawnPointManagerComponent` on the Game State
- Discovers all spawn points in the level
- Spawns initial game objects (relic, goals)
- Handles reset/respawn logic during rounds

No additional setup required - it works automatically!

## API Reference

### ABwaySpawnPoint

Main spawn point actor that can be placed in levels.

```cpp
// Spawn an object at this spawn point
AActor* SpawnedActor = SpawnPoint->SpawnObject();

// Check if spawn point has an active object
if (SpawnPoint->HasSpawnedObject())
{
    // Despawn the current object
    SpawnPoint->DespawnObject();
}

// Query spawn point properties
FGameplayTag Tag = SpawnPoint->SpawnPointTag;
int32 Team = SpawnPoint->TeamIndex;
FTransform SpawnTransform = SpawnPoint->GetSpawnTransform();
```

### UBwaySpawnPointManagerComponent

Centralized manager for all spawn points in the level.

```cpp
// Get manager from game state
ABwayGameState* GameState = GetWorld()->GetGameState<ABwayGameState>();
UBwaySpawnPointManagerComponent* Manager = GameState->FindComponentByClass<UBwaySpawnPointManagerComponent>();

// Query spawn points by tag
FGameplayTag RelicTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Relic"));
TArray<ABwaySpawnPoint*> RelicSpawns = Manager->GetSpawnPointsByTag(RelicTag);

// Get random spawn point
ABwaySpawnPoint* RandomSpawn = Manager->GetRandomSpawnPoint(RelicTag);

// Get closest spawn point to a location
ABwaySpawnPoint* ClosestSpawn = Manager->GetClosestSpawnPoint(PlayerLocation, RelicTag);

// Spawn objects at all matching spawn points
TArray<AActor*> SpawnedActors = Manager->SpawnObjectsAtPoints(RelicTag);

// Reset all spawn points (despawn and respawn)
Manager->ResetAllSpawnPoints(RelicTag);
```

### UBwaySpawnPointData

Data asset base class for spawn configuration.

```cpp
// Get the class to spawn
TSubclassOf<AActor> ClassToSpawn = SpawnData->GetActorClass();

// Initialize spawned actor (called automatically)
SpawnData->InitializeSpawnedActor(SpawnedActor, SpawnPoint);
```

## Advanced Usage

### Creating Custom Spawn Data

You can create specialized spawn data types for different game objects:

```cpp
UCLASS()
class UBwayPowerupSpawnData : public UBwaySpawnPointData
{
    GENERATED_BODY()

public:
    UBwayPowerupSpawnData()
    {
        SpawnType = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Powerup"));
        bEnableRespawning = true;
        RespawnDelay = 30.0f;
    }

    UPROPERTY(EditDefaultsOnly, Category = "Powerup")
    float PowerupDuration = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Powerup")
    float PowerupStrength = 1.5f;

    virtual void InitializeSpawnedActor_Implementation(AActor* SpawnedActor, ABwaySpawnPoint* SpawnPoint) override
    {
        Super::InitializeSpawnedActor_Implementation(SpawnedActor, SpawnPoint);

        // Custom initialization for powerup
        if (APowerupActor* Powerup = Cast<APowerupActor>(SpawnedActor))
        {
            Powerup->SetDuration(PowerupDuration);
            Powerup->SetStrength(PowerupStrength);
        }
    }
};
```

### Runtime Spawn Point Management

```cpp
// Manually register a spawn point created at runtime
ABwaySpawnPoint* NewSpawnPoint = GetWorld()->SpawnActor<ABwaySpawnPoint>(Location, Rotation);
NewSpawnPoint->SpawnPointTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Powerup.Health"));
Manager->RegisterSpawnPoint(NewSpawnPoint);

// Unregister when no longer needed
Manager->UnregisterSpawnPoint(NewSpawnPoint);
```

### Blueprint Usage

All spawn point functionality is exposed to Blueprint:

- **Spawn Point**: All properties are Blueprint editable
- **Manager Component**: All query functions are Blueprint callable
- **Spawn Data**: Can override `InitializeSpawnedActor` in Blueprint

## Best Practices

### Level Design

1. **Use Descriptive Names**: Name spawn points clearly (e.g., "SP_Relic_Center", "SP_Goal_Team1_Left")
2. **Visualize in Editor**: Keep debug visualization enabled during level design
3. **Test Spawning**: Use PIE to verify all spawn points work correctly
4. **Balance Placement**: Ensure fair placement for competitive gameplay

### Data Asset Organization

1. **Folder Structure**: Organize by spawn type
   ```
   Content/BreakawayCore/
   ├── SpawnData/
   │   ├── Relics/
   │   │   └── DA_RelicSpawn.uasset
   │   ├── Goals/
   │   │   ├── DA_GoalSpawn_Team1.uasset
   │   │   └── DA_GoalSpawn_Team2.uasset
   │   └── Powerups/
   │       ├── DA_PowerupSpawn_Health.uasset
   │       └── DA_PowerupSpawn_Speed.uasset
   ```

2. **Naming Convention**: Use prefix `DA_` for Data Assets

### Performance

1. **Spawn on Server Only**: All spawning is server-authoritative
2. **Lazy Loading**: Use soft references in spawn data for async loading
3. **Caching**: Manager component caches spawn points by tag for fast lookups
4. **Minimal Tick**: Spawn points don't tick unless debug visualization is needed

## Troubleshooting

### Common Issues

**Issue**: Spawn points not being discovered
- **Solution**: Ensure `bAutoDiscoverSpawnPoints` is true on manager component
- **Solution**: Check spawn points are placed in the persistent level, not sub-levels

**Issue**: Objects not spawning
- **Solution**: Verify spawn data asset is assigned to spawn point
- **Solution**: Check actor class is set in spawn data
- **Solution**: Ensure spawning is on server (HasAuthority())

**Issue**: Wrong team spawning
- **Solution**: Verify `TeamIndex` is set correctly on spawn point
- **Solution**: Check `bUseSpawnPointTeam` is enabled in spawn data

**Issue**: Respawn not working
- **Solution**: Enable `bEnableRespawning` on spawn point or data asset
- **Solution**: Verify `RespawnDelay` is > 0

### Debug Commands

Add these console commands for debugging (implement in your game mode):

```cpp
// List all spawn points
ListSpawnPoints

// Spawn at specific tag
SpawnAtTag SpawnPoint.Relic

// Reset all spawns
ResetSpawns

// Toggle debug visualization
ShowSpawnPoints 1
```

## Future Enhancements

Potential improvements for the spawn system:

1. **Spawn Waves**: Sequential spawning with delays
2. **Spawn Pools**: Pre-spawn and activate from pool for performance
3. **Conditional Spawning**: Spawn based on game state conditions
4. **Spawn Effects**: VFX/SFX on spawn/despawn
5. **Heat Maps**: Track spawn point usage for balancing
6. **A/B Testing**: Multiple spawn configurations for testing

## Additional Resources

- **Lyra Documentation**: [Lyra Sample Game](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)
- **Gameplay Tags**: [Gameplay Tags Guide](https://docs.unrealengine.com/5.5/en-US/using-gameplay-tags-in-unreal-engine/)
- **Data Assets**: [Data Assets Overview](https://docs.unrealengine.com/5.5/en-US/data-assets-in-unreal-engine/)

## License

Copyright Epic Games, Inc. All Rights Reserved.
