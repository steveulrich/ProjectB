// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "BwayMidfieldDividerComponent.generated.h"

class ABwayMidfieldDividerActor;

/**
 * Spawns and drives the midfield sudden-death divider at world origin.
 * Called from ABreakawayGameMode during initial match object setup.
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayMidfieldDividerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayMidfieldDividerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Spawn the divider actor at MidfieldSpawnLocation if not already present. Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Sudden Death")
	void SpawnMidfieldDivider();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Sudden Death")
	void SetSuddenDeathDividerVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Sudden Death")
	ABwayMidfieldDividerActor* GetMidfieldDivider() const { return SpawnedDivider; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	TSubclassOf<ABwayMidfieldDividerActor> DividerActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FVector MidfieldSpawnLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FRotator MidfieldSpawnRotation = FRotator::ZeroRotator;

protected:
	UPROPERTY(Transient)
	TObjectPtr<ABwayMidfieldDividerActor> SpawnedDivider;
};
