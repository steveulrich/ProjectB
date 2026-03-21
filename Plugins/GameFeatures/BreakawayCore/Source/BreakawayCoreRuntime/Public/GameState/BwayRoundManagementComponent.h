// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "BwayRoundManagementComponent.generated.h"

class ABwayGameState;
class UBwaySpawnPointManagerComponent;

/**
 * Win condition types for Breakaway
 */
UENUM(BlueprintType)
enum class EBwayWinCondition : uint8
{
	None			UMETA(DisplayName = "None"),
	GoalScored		UMETA(DisplayName = "Goal Scored"),
	TeamEliminated	UMETA(DisplayName = "Team Eliminated"),
	TimeExpired		UMETA(DisplayName = "Time Expired - Possession")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRoundStarted, int32, RoundNumber, float, RoundDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRoundEnded, int32, WinningTeam, EBwayWinCondition, WinCondition, int32, RoundNumber);

/**
 * Component responsible for managing Breakaway round lifecycle.
 * Handles round start/end, win conditions, and round timing.
 * Lives on the GameState for replication.
 * 
 * Extracted from ABreakawayGameMode to follow Lyra's modular GameStateComponent pattern.
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayRoundManagementComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayRoundManagementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ========================================
	// Round Lifecycle
	// ========================================

	/** Start a new round. Resets round state and begins the timer. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Round")
	void StartRound();

	/** End the current round with a specific win condition. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Round")
	void EndRound(int32 WinningTeam, EBwayWinCondition WinCondition);

	/** Reset round state (relic, players, buildables). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Round")
	void ResetRoundState();

	// ========================================
	// Win Condition Checks
	// ========================================

	/** Called when the relic enters a goal trigger. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Scoring")
	void OnRelicScored(int32 ScoringTeam);

	/** Check if a team has been eliminated (all players dead). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Scoring")
	void CheckTeamElimination();

	/** Handle when round timer expires — check relic possession. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Scoring")
	void OnRoundTimerExpired();

	/** Check if match is over (a team reached the winning score). */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Scoring")
	bool CheckMatchEnd() const;

	// ========================================
	// Configuration
	// ========================================

	/** Points needed to win the match */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	int32 PointsToWin = 3;

	/** Delay before respawning a player after death */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float RespawnDelay = 3.0f;

	/** Delay before starting first round */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float PreRoundDelay = 5.0f;

	/** Whether to automatically start the first round */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	bool bAutoStartFirstRound = true;

	// ========================================
	// Events
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRoundStarted OnRoundStarted;

	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRoundEnded OnRoundEnded;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Get the owning game state cast to ABwayGameState */
	ABwayGameState* GetBwayGameState() const;

	/** Determine which team has relic possession based on location/carrier */
	int32 DetermineRelicPossessionTeam() const;

	/** Timer handles */
	FTimerHandle PreRoundTimerHandle;
};
