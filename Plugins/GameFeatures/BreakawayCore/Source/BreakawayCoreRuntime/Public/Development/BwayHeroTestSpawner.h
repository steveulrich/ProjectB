// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BwayHeroTestSpawner.generated.h"

class UBwayHeroDataAsset;
class UBwayHeroSpawnerWidget;
class UCapsuleComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class APawn;
struct FHitResult;

/**
 * ABwayHeroTestSpawner
 * 
 * A placeable actor for testing hero switching. When a player walks over it,
 * their hero is changed to the configured hero and they respawn.
 * 
 * Features:
 * - Configurable hero per instance (EditInstanceOnly)
 * - Floating world-space widget showing hero name and portrait
 * - Collision-based trigger for hero change
 * 
 * Usage:
 * 1. Place in level
 * 2. Set HeroDefinition to desired hero data asset
 * 3. Walk over to switch to that hero
 */
UCLASS(Blueprintable, BlueprintType)
class BREAKAWAYCORERUNTIME_API ABwayHeroTestSpawner : public AActor
{
	GENERATED_BODY()

public:
	ABwayHeroTestSpawner();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	// ========================================
	// Configuration
	// ========================================

	/**
	 * The hero that players will switch to when they walk over this spawner.
	 * Set this per-instance in the level.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Hero Test Spawner")
	TObjectPtr<UBwayHeroDataAsset> HeroDefinition;

	/**
	 * Height offset for the floating info widget above the pad
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero Test Spawner|Display")
	float WidgetHeightOffset = 150.0f;

	/**
	 * Scale for the widget in world space
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero Test Spawner|Display")
	float WidgetDrawSize = 200.0f;

	// ========================================
	// Components
	// ========================================

	/** Collision volume for detecting player overlap */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> CollisionVolume;

	/** Visual mesh for the spawner pad */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	/** Widget component for displaying hero info in world space */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> InfoWidgetComponent;

protected:
	// ========================================
	// Internal Functions
	// ========================================

	/** Called when something overlaps the collision volume */
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, 
		const FHitResult& SweepHitResult);

	/**
	 * Triggers the hero change for the given pawn
	 * @param Pawn The pawn to change hero for
	 */
	void TriggerHeroChange(APawn* Pawn);

	/**
	 * Updates the display widget with current hero info
	 * Called from OnConstruction and BeginPlay
	 */
	void UpdateDisplayWidget();

	/** Widget class to use for the floating display */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Test Spawner|Display")
	TSubclassOf<UBwayHeroSpawnerWidget> InfoWidgetClass;

private:
	/** Cached reference to the widget instance */
	UPROPERTY()
	TObjectPtr<UBwayHeroSpawnerWidget> InfoWidget;
};

