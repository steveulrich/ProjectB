// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "BwaySpawnPoint.generated.h"

class UBwaySpawnPointData;
class UBillboardComponent;
class UArrowComponent;

/**
 * Spawn point actor that can be placed in the level to spawn various game objects
 * Uses gameplay tags to identify spawn point types and data assets to define what spawns
 * 
 * Examples:
 * - Relic spawn points (SpawnPoint.Relic)
 * - Goal spawn points (SpawnPoint.Goal.Team1 / SpawnPoint.Goal.Team2)
 * - Powerup spawn points (SpawnPoint.Powerup.Health / SpawnPoint.Powerup.Speed)
 * - Player spawn points (handled by existing PlayerStart actors with tags)
 */
UCLASS(Blueprintable)
class BREAKAWAYCORERUNTIME_API ABwaySpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ABwaySpawnPoint();

	//~AActor interface
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End of AActor interface

	// ========================================
	// Configuration
	// ========================================

	/** 
	 * Gameplay tag identifying what this spawn point is for
	 * Examples:
	 * - SpawnPoint.Relic
	 * - SpawnPoint.Goal.Team1
	 * - SpawnPoint.Powerup.Health
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Point", meta = (Categories = "SpawnPoint"))
	FGameplayTag SpawnPointTag;

	/**
	 * Optional: Data asset defining what to spawn at this point
	 * If null, uses default spawning rules from the game mode
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Point")
	TObjectPtr<UBwaySpawnPointData> SpawnData;

	/**
	 * Whether this spawn point should spawn its object automatically at level start
	 * If false, requires manual spawning via SpawnObject()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	bool bAutoSpawnOnBeginPlay = true;

	/**
	 * Whether this spawn point can be used for respawning
	 * Relevant for powerups that respawn periodically
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	bool bEnableRespawning = false;

	/**
	 * Delay before respawning after the spawned object is destroyed/consumed
	 * Only used if bEnableRespawning is true
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point", meta = (EditCondition = "bEnableRespawning"))
	float RespawnDelay = 30.0f;

	/**
	 * Optional: Team affiliation for this spawn point
	 * -1 = Neutral, 0 = Team 1, 1 = Team 2, etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	int32 TeamIndex = -1;

	/**
	 * Whether to show debug visualization in editor and during gameplay
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point|Debug")
	bool bShowDebugVisualization = true;

	/**
	 * Color for debug visualization (overrides default based on spawn type)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point|Debug", meta = (EditCondition = "bShowDebugVisualization"))
	FLinearColor DebugColor = FLinearColor::White;

	// ========================================
	// Spawning
	// ========================================

	/**
	 * Spawn the object defined by this spawn point
	 * Returns the spawned actor, or nullptr if spawning failed
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	AActor* SpawnObject();

	/**
	 * Despawn the currently spawned object (if any)
	 */
	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void DespawnObject();

	/**
	 * Returns true if this spawn point currently has a spawned object
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	bool HasSpawnedObject() const { return SpawnedObject != nullptr; }

	/**
	 * Get the currently spawned object (may be null)
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	AActor* GetSpawnedObject() const { return SpawnedObject; }

	/**
	 * Called when the spawned object is destroyed (for respawning logic)
	 */
	UFUNCTION()
	void OnSpawnedObjectDestroyed(AActor* DestroyedActor);

	// ========================================
	// Query Functions
	// ========================================

	/**
	 * Check if this spawn point matches a specific gameplay tag
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	bool MatchesTag(FGameplayTag Tag) const;

	/**
	 * Check if this spawn point matches any of the provided tags
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	bool MatchesAnyTags(const FGameplayTagContainer& GameplayTags) const;

	/**
	 * Get the spawn transform (location + rotation + scale)
	 */
	UFUNCTION(BlueprintPure, Category = "Spawn Point")
	FTransform GetSpawnTransform() const;

protected:
	/**
	 * Reference to the currently spawned object
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Spawn Point")
	TObjectPtr<AActor> SpawnedObject;

	/**
	 * Timer handle for respawn delay
	 */
	FTimerHandle RespawnTimerHandle;

	/**
	 * Internal function to determine what class to spawn
	 * Can be overridden in Blueprint for custom logic
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawn Point")
	TSubclassOf<AActor> GetClassToSpawn() const;
	virtual TSubclassOf<AActor> GetClassToSpawn_Implementation() const;

	/**
	 * Internal function called after successful spawning
	 * Can be overridden for custom post-spawn logic
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Spawn Point")
	void OnObjectSpawned(AActor* NewObject);
	virtual void OnObjectSpawned_Implementation(AActor* NewObject);

	/**
	 * Update debug visualization
	 */
	void UpdateDebugVisualization();

	/**
	 * Get debug color based on spawn point type
	 */
	FLinearColor GetDebugColorForType() const;

	/**
	 * Timer callback for respawning (void return type required for timers)
	 */
	void HandleRespawnTimer();

private:
	/**
	 * Scene component for visualization
	 */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

#if WITH_EDITORONLY_DATA
	/**
	 * Billboard component for editor visualization
	 */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBillboardComponent> EditorBillboard;

	/**
	 * Arrow component showing spawn direction
	 */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UArrowComponent> DirectionArrow;
#endif
};
