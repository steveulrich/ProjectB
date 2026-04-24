// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "GameplayTagContainer.h"
#include "BwayRelicManagerComponent.generated.h"

class ARelicActor;
class ABwayCharacterWithAbilities;
class UBwaySpawnPointManagerComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRelicCarrierChanged, ARelicActor*, Relic, ABwayCharacterWithAbilities*, NewCarrier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRelicReset, ARelicActor*, Relic);

/**
 * Component responsible for managing the Relic lifecycle in a Breakaway match.
 * Handles relic spawning, reset, carrier tracking, and possession state.
 * Lives on the GameState for replication.
 *
 * Extracted from ABreakawayGameMode to follow Lyra's modular GameStateComponent pattern.
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayRelicManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayRelicManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ========================================
	// Relic Lifecycle
	// ========================================

	/** Spawn the relic at a designated spawn point */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Relic")
	ARelicActor* SpawnRelic();

	/** Reset the relic to its spawn location */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Relic")
	void ResetRelic();

	/** Called when relic carrier changes */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Relic")
	void OnRelicCarrierChanged(ABwayCharacterWithAbilities* NewCarrier);

	// ========================================
	// Accessors
	// ========================================

	/** Get the active relic actor */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Relic")
	ARelicActor* GetRelicActor() const { return ActiveRelic; }

	/** Get which team currently possesses the relic (-1 if neutral) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Relic")
	int32 GetRelicPossessingTeam() const { return RelicPossessingTeam; }

	/** Set the active relic actor (e.g., when spawned by GameMode) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Relic")
	void SetActiveRelic(ARelicActor* NewRelic);

	// ========================================
	// Configuration
	// ========================================

	/** Class to use for the relic actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakaway|Config")
	TSubclassOf<ARelicActor> RelicClass;

	/** Gameplay tag for relic spawn points */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FGameplayTag RelicSpawnTag;

	/** Relic configuration data asset — provides physics, gameplay, visual, and audio settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakaway|Config")
	TSoftObjectPtr<class URelicSettings> RelicSettingsAsset;

	// ========================================
	// Events
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRelicCarrierChanged OnRelicCarrierChangedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRelicReset OnRelicResetEvent;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Current active relic in the match */
	UPROPERTY(Replicated)
	TObjectPtr<ARelicActor> ActiveRelic;

	/** Which team currently possesses the relic (-1 = neutral/dropped) */
	UPROPERTY(ReplicatedUsing = OnRep_RelicPossessingTeam)
	int32 RelicPossessingTeam = -1;

	UFUNCTION()
	void OnRep_RelicPossessingTeam();

	/** Cached reference to the spawn point manager */
	UPROPERTY()
	TObjectPtr<UBwaySpawnPointManagerComponent> CachedSpawnPointManager;
};
