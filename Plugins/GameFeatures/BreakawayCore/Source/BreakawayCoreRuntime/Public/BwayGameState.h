// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameModes/LyraGameState.h"
#include "BwayGameState.generated.h"

class ARelicActor;
class ABwayCharacterWithAbilities;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScoreChanged, int32, Team1Score, int32, Team2Score);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundStateChanged, FName, NewRoundState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchStateChanged, FName, NewMatchState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundTimeChanged, int32, RemainingSeconds);

/**
 * Enum for tracking the current state of a round
 */
UENUM(BlueprintType)
enum class ERoundState : uint8
{
	WaitingToStart		UMETA(DisplayName = "Waiting To Start"),
	RoundActive			UMETA(DisplayName = "Round Active"),
	RoundEnding			UMETA(DisplayName = "Round Ending"),
	RoundComplete		UMETA(DisplayName = "Round Complete")
};

/**
 * Struct to hold team information
 */
USTRUCT(BlueprintType)
struct FTeamInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 TeamIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 AlivePlayerCount = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<TObjectPtr<APlayerState>> TeamMembers;

	FTeamInfo() = default;
	FTeamInfo(int32 InTeamIndex) : TeamIndex(InTeamIndex) {}
};

/**
 * Game State for Breakaway - manages replicated match state, scores, teams, and round timing
 * This is the authoritative source of truth for all clients about the match state
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API ABwayGameState : public ALyraGameState
{
	GENERATED_BODY()

public:
	ABwayGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~AGameState interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	//~End of AGameState interface

	// ========================================
	// Team Management
	// ========================================

	/** Get team information by index (0 = Team 1, 1 = Team 2) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Teams")
	const FTeamInfo& GetTeamInfo(int32 TeamIndex) const;

	/** Get the team index for a given player state */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Teams")
	int32 GetPlayerTeam(const APlayerState* PlayerState) const;

	/** Add a player to a team */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Teams", meta = (BlueprintAuthorityOnly))
	void AddPlayerToTeam(APlayerState* PlayerState, int32 TeamIndex);

	/** Remove a player from their team */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Teams", meta = (BlueprintAuthorityOnly))
	void RemovePlayerFromTeam(APlayerState* PlayerState);

	/** Check if two actors are on the same team */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Teams")
	bool AreOnSameTeam(const AActor* ActorA, const AActor* ActorB) const;

	// ========================================
	// Score Management
	// ========================================

	/** Add a point to a team's score */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Score", meta = (BlueprintAuthorityOnly))
	void AddScore(int32 TeamIndex, int32 Points = 1);

	/** Get the current score for a team */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Score")
	int32 GetTeamScore(int32 TeamIndex) const;

	/** Reset all scores to zero */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Score", meta = (BlueprintAuthorityOnly))
	void ResetScores();

	// ========================================
	// Round State Management
	// ========================================

	/** Get the current round state */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Round")
	ERoundState GetCurrentRoundState() const { return CurrentRoundState; }

	/** Set the round state (server only) */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Round", meta = (BlueprintAuthorityOnly))
	void SetRoundState(ERoundState NewState);

	/** Get remaining time in the current round (in seconds) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Round")
	int32 GetRoundTimeRemaining() const;

	/** Get the current round number (1-indexed) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Round")
	int32 GetCurrentRoundNumber() const { return CurrentRoundNumber; }

	// ========================================
	// Relic Tracking
	// ========================================

	/** Set the active relic actor (server only) */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic", meta = (BlueprintAuthorityOnly))
	void SetRelicActor(ARelicActor* NewRelic);

	/** Get the active relic actor */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Relic")
	ARelicActor* GetRelicActor() const { return RelicActor; }

	/** Track which team currently has possession of the relic */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic", meta = (BlueprintAuthorityOnly))
	void SetRelicPossessingTeam(int32 TeamIndex);

	/** Get which team currently possesses the relic (-1 if neutral) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Relic")
	int32 GetRelicPossessingTeam() const { return RelicPossessingTeam; }

	// ========================================
	// Player Tracking
	// ========================================

	/** Update the alive player count for a team (called when players die/respawn) */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Teams", meta = (BlueprintAuthorityOnly))
	void UpdateTeamAliveCount(int32 TeamIndex, int32 AliveCount);

	/** Called when a player dies to update team state */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Teams", meta = (BlueprintAuthorityOnly))
	void OnPlayerDied(APlayerState* PlayerState);

	/** Called when a player respawns to update team state */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Teams", meta = (BlueprintAuthorityOnly))
	void OnPlayerRespawned(APlayerState* PlayerState);

	// ========================================
	// Configuration
	// ========================================

	/** Points needed to win the match */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	int32 PointsToWin = 3;

	/** Duration of each round in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float RoundDuration = 180.0f; // 3 minutes

	/** Time to wait before starting a new round after one ends */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float RoundEndDelay = 5.0f;

	/** Maximum number of players per team */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	int32 MaxPlayersPerTeam = 4;

	// ========================================
	// Events/Delegates
	// ========================================

	/** Broadcast when team scores change */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnScoreChanged OnScoreChanged;

	/** Broadcast when round state changes */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRoundStateChanged OnRoundStateChanged;

	/** Broadcast when match state changes */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnMatchStateChanged OnMatchStateChanged;

	/** Broadcast when round time updates (every second) */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRoundTimeChanged OnRoundTimeChanged;

protected:
	// ========================================
	// Replicated Properties
	// ========================================

	/** Team information arrays */
	UPROPERTY(ReplicatedUsing = OnRep_TeamInfo)
	TArray<FTeamInfo> Teams;

	UFUNCTION()
	void OnRep_TeamInfo();

	/** Current round state */
	UPROPERTY(ReplicatedUsing = OnRep_RoundState)
	ERoundState CurrentRoundState = ERoundState::WaitingToStart;

	UFUNCTION()
	void OnRep_RoundState();

	/** When the current round started (server time) */
	UPROPERTY(Replicated)
	float RoundStartTime = 0.0f;

	/** Current round number */
	UPROPERTY(Replicated)
	int32 CurrentRoundNumber = 0;

	/** Reference to the active relic in the match */
	UPROPERTY(Replicated)
	TObjectPtr<ARelicActor> RelicActor = nullptr;

	/** Which team currently possesses the relic (-1 = neutral/dropped) */
	UPROPERTY(ReplicatedUsing = OnRep_RelicPossessingTeam)
	int32 RelicPossessingTeam = -1;

	UFUNCTION()
	void OnRep_RelicPossessingTeam();

	// ========================================
	// Internal State
	// ========================================

	/** Last time we broadcast the round time update */
	float LastRoundTimeUpdateBroadcast = 0.0f;

	/** Interval for broadcasting round time updates */
	const float RoundTimeUpdateInterval = 1.0f;

	/** Initialize teams on begin play */
	void InitializeTeams();

	/** Helper to find team index for an actor */
	int32 GetTeamIndexForActor(const AActor* Actor) const;
};