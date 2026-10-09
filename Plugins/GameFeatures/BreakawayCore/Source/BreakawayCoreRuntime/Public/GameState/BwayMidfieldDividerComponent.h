// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "BwayMidfieldDividerComponent.generated.h"

class ABwayMidfieldDividerActor;

/**
 * Spawns and drives the midfield divider using the same goal geometry as scoring.
 * Called from ABreakawayGameMode during initial match object setup.
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayMidfieldDividerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayMidfieldDividerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Spawn or align the divider actor. Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Sudden Death")
	void SpawnMidfieldDivider();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Sudden Death")
	void SetSuddenDeathDividerVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Sudden Death")
	ABwayMidfieldDividerActor* GetMidfieldDivider() const { return SpawnedDivider; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	TSubclassOf<ABwayMidfieldDividerActor> DividerActorClass;

	/** Align local +X with the team-0 to team-1 goal axis. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	bool bAlignToTeamGoals = true;

	/** XY offset from the goal midpoint and absolute floor height; absolute position when alignment is disabled. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FVector MidfieldSpawnLocation = FVector(0.0f, 0.0f, 2.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FRotator MidfieldSpawnRotation = FRotator::ZeroRotator;

protected:
	UPROPERTY(Transient)
	TObjectPtr<ABwayMidfieldDividerActor> SpawnedDivider;
};
