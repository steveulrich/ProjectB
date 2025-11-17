// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BwaySpawnPointData.generated.h"

class ABwaySpawnPoint;

/**
 * Data asset defining what a spawn point should spawn
 * Allows designers to configure spawn behavior without code changes
 * 
 * Examples:
 * - RelicSpawnData: Spawns a relic actor with specific settings
 * - GoalSpawnData: Spawns a goal volume for a specific team
 * - PowerupSpawnData: Spawns a powerup with respawn behavior
 */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UBwaySpawnPointData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ========================================
	// Configuration
	// ========================================

	/**
	 * The actor class to spawn
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn Data")
	TSoftClassPtr<AActor> ActorClass;

	/**
	 * Gameplay tag identifying what type of spawn this is
	 * Should match the SpawnPointTag on spawn points using this data
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn Data", meta = (Categories = "SpawnPoint"))
	FGameplayTag SpawnType;

	/**
	 * Optional: Override spawn transform (relative to spawn point)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn Data")
	FTransform SpawnTransformOffset = FTransform::Identity;

	/**
	 * Whether spawned actors should use the spawn point's team index
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn Data")
	bool bUseSpawnPointTeam = true;

	/**
	 * Optional: Override respawn behavior
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn Data|Respawn")
	bool bOverrideRespawnSettings = false;

	/**
	 * Enable respawning for spawned objects
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn Data|Respawn", meta = (EditCondition = "bOverrideRespawnSettings"))
	bool bEnableRespawning = false;

	/**
	 * Respawn delay in seconds
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn Data|Respawn", meta = (EditCondition = "bOverrideRespawnSettings && bEnableRespawning"))
	float RespawnDelay = 30.0f;

	// ========================================
	// Functions
	// ========================================

	/**
	 * Get the actor class to spawn (handles soft class loading)
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Data")
	TSubclassOf<AActor> GetActorClass() const;

	/**
	 * Initialize a spawned actor with this data
	 * Override in subclasses for custom initialization
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawn Data")
	void InitializeSpawnedActor(AActor* SpawnedActor, ABwaySpawnPoint* SpawnPoint);
	virtual void InitializeSpawnedActor_Implementation(AActor* SpawnedActor, ABwaySpawnPoint* SpawnPoint);

	/**
	 * Check if this spawn data matches a gameplay tag
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Data")
	bool MatchesSpawnType(FGameplayTag Tag) const;

#if WITH_EDITOR
	//~UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End of UObject interface
#endif
};

/**
 * Specialized spawn data for relic spawning
 * Includes relic-specific configuration
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayRelicSpawnData : public UBwaySpawnPointData
{
	GENERATED_BODY()

public:
	UBwayRelicSpawnData();

	/**
	 * Reference to relic settings data asset
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic")
	TSoftObjectPtr<class URelicSettings> RelicSettings;

	//~UBwaySpawnPointData interface
	virtual void InitializeSpawnedActor_Implementation(AActor* SpawnedActor, ABwaySpawnPoint* SpawnPoint) override;
	//~End of UBwaySpawnPointData interface
};

/**
 * Specialized spawn data for goal spawning
 * Includes team and goal-specific configuration
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGoalSpawnData : public UBwaySpawnPointData
{
	GENERATED_BODY()

public:
	UBwayGoalSpawnData();

	/**
	 * Goal extent (size of the goal trigger box)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goal")
	FVector GoalExtent = FVector(100.0f, 200.0f, 200.0f);

	/**
	 * Whether to show debug visualization for goal
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Goal")
	bool bShowDebugSphere = true;

	//~UBwaySpawnPointData interface
	virtual void InitializeSpawnedActor_Implementation(AActor* SpawnedActor, ABwaySpawnPoint* SpawnPoint) override;
	//~End of UBwaySpawnPointData interface
};
