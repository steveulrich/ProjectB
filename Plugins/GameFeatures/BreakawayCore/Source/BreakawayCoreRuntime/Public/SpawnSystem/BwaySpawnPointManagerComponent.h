// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BwaySpawnPointManagerComponent.generated.h"

class ABwaySpawnPoint;
class UBwaySpawnPointData;

/**
 * Component that manages all spawn points in the level
 * Provides centralized access to spawn points for the game mode and other systems
 * Automatically discovers and registers spawn points at game start
 * 
 * Add this to your GameState in the Experience Definition or GameMode
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwaySpawnPointManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBwaySpawnPointManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	// ========================================
	// Spawn Point Discovery
	// ========================================

	/**
	 * Discover all spawn points in the level
	 * Called automatically during BeginPlay
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point Manager")
	void DiscoverSpawnPoints();

	/**
	 * Register a spawn point with the manager
	 * Can be called manually to add spawn points dynamically
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point Manager")
	void RegisterSpawnPoint(ABwaySpawnPoint* SpawnPoint);

	/**
	 * Unregister a spawn point from the manager
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point Manager")
	void UnregisterSpawnPoint(ABwaySpawnPoint* SpawnPoint);

	// ========================================
	// Query Functions
	// ========================================

	/**
	 * Get all spawn points matching a specific gameplay tag
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point Manager")
	TArray<ABwaySpawnPoint*> GetSpawnPointsByTag(FGameplayTag Tag) const;

	/**
	 * Get all spawn points matching any of the provided tags
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point Manager")
	TArray<ABwaySpawnPoint*> GetSpawnPointsByTags(const FGameplayTagContainer& GameplayTags) const;

	/**
	 * Get all spawn points for a specific team
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point Manager")
	TArray<ABwaySpawnPoint*> GetSpawnPointsByTeam(int32 TeamIndex) const;

	/**
	 * Get a random spawn point matching a tag
	 * Returns nullptr if no matching spawn points found
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point Manager")
	ABwaySpawnPoint* GetRandomSpawnPoint(FGameplayTag Tag) const;

	/**
	 * Get the closest spawn point to a location
	 * Optionally filter by tag
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point Manager")
	ABwaySpawnPoint* GetClosestSpawnPoint(FVector Location, FGameplayTag OptionalTag = FGameplayTag()) const;

	/**
	 * Get all registered spawn points
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point Manager")
	TArray<ABwaySpawnPoint*> GetAllSpawnPoints() const;

	/**
	 * Get count of spawn points matching a tag
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point Manager")
	int32 GetSpawnPointCount(FGameplayTag Tag = FGameplayTag()) const;

	// ========================================
	// Spawn Management
	// ========================================

	/**
	 * Spawn objects at all spawn points matching a tag
	 * Returns array of spawned actors
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point Manager")
	TArray<AActor*> SpawnObjectsAtPoints(FGameplayTag Tag);

	/**
	 * Despawn objects at all spawn points matching a tag
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point Manager")
	void DespawnObjectsAtPoints(FGameplayTag Tag);

	/**
	 * Reset all spawn points (despawn and respawn)
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point Manager")
	void ResetAllSpawnPoints(FGameplayTag OptionalTag = FGameplayTag());

	// ========================================
	// Configuration
	// ========================================

	/**
	 * Whether to automatically discover spawn points on BeginPlay
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point Manager")
	bool bAutoDiscoverSpawnPoints = true;

	/**
	 * Whether to automatically spawn objects at spawn points on discovery
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point Manager")
	bool bAutoSpawnOnDiscovery = false;

	/**
	 * Tags to filter auto-spawning (if empty, spawns at all points)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point Manager", meta = (EditCondition = "bAutoSpawnOnDiscovery", Categories = "SpawnPoint"))
	FGameplayTagContainer AutoSpawnTags;

protected:
	/**
	 * All registered spawn points (stored as raw pointers for TMap compatibility)
	 */
	UPROPERTY()
	TArray<TObjectPtr<ABwaySpawnPoint>> RegisteredSpawnPoints;

	/**
	 * Spawn points organized by tag for faster lookups
	 */
	TMap<FGameplayTag, TArray<ABwaySpawnPoint*>> SpawnPointsByTag;

	/**
	 * Spawn points organized by team for faster lookups
	 */
	TMap<int32, TArray<ABwaySpawnPoint*>> SpawnPointsByTeam;

	/**
	 * Rebuild lookup tables after spawn point registration changes
	 */
	void RebuildLookupTables();
};
