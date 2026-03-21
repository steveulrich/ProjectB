// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "BwayScoringComponent.generated.h"

class ABwayGameState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamScoreChanged, int32, TeamIndex, int32, NewScore);

/**
 * Component responsible for managing Breakaway team scores.
 * Separated from GameState to follow Lyra's modular component pattern.
 * Lives on the GameState for replication.
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayScoringComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayScoringComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Add points to a team's score */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Score")
	void AddScore(int32 TeamIndex, int32 Points = 1);

	/** Get the current score for a team */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Score")
	int32 GetTeamScore(int32 TeamIndex) const;

	/** Reset all scores to zero */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Score")
	void ResetScores();

	/** Get all team scores as an array */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Score")
	TArray<int32> GetAllScores() const;

	/** Broadcast when score changes */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnTeamScoreChanged OnTeamScoreChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Replicated scores per team. Index = TeamIndex. */
	UPROPERTY(ReplicatedUsing = OnRep_Scores)
	TArray<int32> TeamScores;

	UFUNCTION()
	void OnRep_Scores();

	virtual void BeginPlay() override;
};
