// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BwayGameState.h"
#include "GameModes/LyraGameMode.h"
#include "GameplayTagContainer.h"
#include "BreakawayGameMode.generated.h"

class ARelicActor;
class ABwayCharacterWithAbilities;
class ABwayPlayerState;
class UBwaySpawnPointManagerComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogBreakawayGame, Log, All);

/**
 * Win condition types for Breakaway
 */
UENUM(BlueprintType)
enum class EWinCondition : uint8
{
	None			UMETA(DisplayName = "None"),
	GoalScored		UMETA(DisplayName = "Goal Scored"),
	TeamEliminated	UMETA(DisplayName = "Team Eliminated"),
	TimeExpired		UMETA(DisplayName = "Time Expired - Possession")
};

/**
 * Game Mode for Breakaway
 * Handles server-authoritative game logic including:
 * - Round lifecycle management
 * - All three win conditions (Goal scoring, Team elimination, Timer possession)
 * - Team assignment and balancing
 * - Relic spawning and respawning via new spawn point system
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API ABreakawayGameMode : public ALyraGameMode
{
	GENERATED_BODY()

public:
	ABreakawayGameMode();

	//~AGameModeBase interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	//~End of AGameModeBase interface

	// ========================================
	// Round Management
	// ========================================

	/** Start a new round */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Round")
	void StartRound();

	/** End the current round with a specific win condition */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Round")
	void EndRound(int32 WinningTeam, EWinCondition WinCondition);

	/** Reset all players and relic for a new round */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Round")
	void ResetRoundState();

	// ========================================
	// Scoring & Win Conditions
	// ========================================

	/** Called when the relic enters a goal trigger */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Scoring")
	void OnRelicScored(int32 ScoringTeam);

	/** Check if a team has been eliminated (all players dead) */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Scoring")
	void CheckTeamElimination();

	/** Handle when round timer expires - check relic possession */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Scoring")
	void OnRoundTimerExpired();

	/** Check if match is over (a team reached the winning score) */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Scoring")
	bool CheckMatchEnd();

	// ========================================
	// Team Management
	// ========================================

	/** Assign a player to the team with fewer players */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Teams")
	void AssignPlayerToTeam(APlayerController* Player);

	/** Get the team with fewer players for balancing */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Teams")
	int32 GetTeamWithFewerPlayers() const;

	// ========================================
	// Relic Management
	// ========================================

	/** Reset the relic to its spawn location */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic")
	void ResetRelic();

	/** Called when relic carrier changes */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic")
	void OnRelicCarrierChanged(ABwayCharacterWithAbilities* NewCarrier);

	// ========================================
	// Player Management
	// ========================================

	/** Called when a player character dies */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Players")
	void OnPlayerDied(AController* VictimController, AController* KillerController);

	/** Respawn a player after death */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Players")
	void RespawnPlayer(AController* Controller);

	// ========================================
	// Configuration
	// ========================================

	/** Class to use for the relic actor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakaway|Config")
	TSubclassOf<ARelicActor> RelicClass;

	/** Tag to identify relic spawn points in the level */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FName RelicSpawnPointTag = FName("RelicSpawn");

	/** Tag to identify Team 1 spawn points */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FName Team1SpawnPointTag = FName("Team1Spawn");

	/** Tag to identify Team 2 spawn points */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FName Team2SpawnPointTag = FName("Team2Spawn");

	/** Delay before respawning a player after death */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float RespawnDelay = 3.0f;

	/** Delay before starting first round */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float PreRoundDelay = 5.0f;

	/** Whether to automatically start the first round */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	bool bAutoStartFirstRound = true;

protected:

	/** Get the Breakaway game state (casted for convenience) */
	UFUNCTION(BlueprintPure, Category = "Breakaway")
	ABwayGameState* GetBreakawayGameState() const;

	/** Find all player start actors with a specific tag */
	TArray<AActor*> GetPlayerStartsWithTag(const FName& Tag) const;

	/** Handle to track round timer */
	FTimerHandle RoundTimerHandle;

	/** Handle to track pre-round countdown */
	FTimerHandle PreRoundTimerHandle;

	/** Handles for respawn timers (one per player) */
	TMap<AController*, FTimerHandle> RespawnTimers;

	/** Current active relic in the match */
	UPROPERTY()
	TObjectPtr<ARelicActor> ActiveRelic;

	/** Track if we've initialized the game properly */
	bool bGameInitialized = false;

	/** Spawn point manager component reference */
	UPROPERTY()
	TObjectPtr<UBwaySpawnPointManagerComponent> SpawnPointManager;

	/** Gameplay tags for spawn points */
	FGameplayTag RelicSpawnTag;
	FGameplayTag Goal1SpawnTag;
	FGameplayTag Goal2SpawnTag;

	/** Initialize spawn point tags */
	void InitializeSpawnPointTags();

	/** Spawn initial game objects (relic, goals) using spawn point system */
	void SpawnInitialGameObjects();

	/** Internal helper to determine which team has relic possession based on location */
	int32 DetermineRelicPossessionTeam() const;
};
