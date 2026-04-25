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
 * Game Mode for Breakaway
 * 
 * Handles server-authoritative player lifecycle only:
 * - Player login/logout and team assignment
 * - Pawn spawning and hero data application
 * - Team-based spawn point selection
 * - Initial game object spawning (relic, goals)
 * 
 * All domain logic (rounds, scoring, relic tracking, win conditions, player death/respawn)
 * is handled by GameState Components:
 * - UBwayRoundManagementComponent
 * - UBwayScoringComponent
 * - UBwayRelicManagerComponent
 * - UBwayTeamBridgeComponent
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
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	//~End of AGameModeBase interface

	/**
	 * Called after RestartPlayer to apply hero data to the newly spawned pawn.
	 * This ensures the character has the correct mesh, animation, and abilities.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Players")
	void ApplyHeroDataToNewPawn(AController* Controller);

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
	// Configuration
	// ========================================

	/** Tag to identify Team 1 spawn points */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FName Team1SpawnPointTag = FName("Team1Spawn");

	/** Tag to identify Team 2 spawn points */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FName Team2SpawnPointTag = FName("Team2Spawn");

protected:

	/** Get the Breakaway game state (casted for convenience) */
	UFUNCTION(BlueprintPure, Category = "Breakaway")
	ABwayGameState* GetBreakawayGameState() const;

	/** Find all player start actors with a specific tag */
	TArray<AActor*> GetPlayerStartsWithTag(const FName& Tag) const;

	/** Track if we've initialized the game properly */
	bool bGameInitialized = false;

	/** Spawn point manager component reference */
	UPROPERTY()
	TObjectPtr<UBwaySpawnPointManagerComponent> SpawnPointManager;

	/** Gameplay tags for goal spawn points */
	FGameplayTag Goal1SpawnTag;
	FGameplayTag Goal2SpawnTag;

	/** Initialize spawn point tags */
	void InitializeSpawnPointTags();

	/** Spawn initial game objects (relic, goals) using spawn point system */
	void SpawnInitialGameObjects();
};
