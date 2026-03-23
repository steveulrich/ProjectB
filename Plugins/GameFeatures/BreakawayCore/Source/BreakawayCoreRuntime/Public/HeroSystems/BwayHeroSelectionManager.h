// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "BwayHeroDataAsset.h"
#include "BwayHeroSelectionManager.generated.h"

class ABwayPlayerState;
class ABwayGameState;

/**
 * Tracks the state of hero selection for a single player
 */
USTRUCT(BlueprintType)
struct FPlayerHeroSelectionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FPrimaryAssetId SelectedHeroId;

	UPROPERTY(BlueprintReadOnly)
	bool bIsLocked = false;

	UPROPERTY(BlueprintReadOnly)
	int32 TeamIndex = -1;

	bool IsValid() const { return PlayerState != nullptr; }
};

/**
 * Delegate fired when a player's hero selection changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerHeroSelectionChanged, 
	APlayerState*, PlayerState, 
	FPrimaryAssetId, NewHeroId,
	int32, TeamIndex);

/**
 * Delegate fired when a player locks their hero selection
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerHeroLocked,
	APlayerState*, PlayerState,
	FPrimaryAssetId, LockedHeroId);

/**
 * Delegate fired when all players have locked their selections
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllPlayersReady);

/**
 * UBwayHeroSelectionManager
 * 
 * Game State Component that manages the hero selection phase
 * Tracks all player selections, enforces rules, and signals when ready
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayHeroSelectionManager : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayHeroSelectionManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	// ========== HERO SELECTION TRACKING ==========

	/**
	 * Register a player for hero selection tracking
	 * Called automatically when players join
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	void RegisterPlayer(APlayerState* PlayerState);

	/**
	 * Unregister a player (when they leave)
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	void UnregisterPlayer(APlayerState* PlayerState);

	/**
	 * Get the current hero selection state for all players
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	TArray<FPlayerHeroSelectionState> GetAllPlayerSelections() const;

	/**
	 * Get selection state for a specific player
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	FPlayerHeroSelectionState GetPlayerSelection(APlayerState* PlayerState) const;

	/**
	 * Check if a specific hero is already selected by another player on the same team
	 * Returns true if the hero is available for selection
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	bool IsHeroAvailableForTeam(FPrimaryAssetId HeroId, int32 TeamIndex) const;

	/**
	 * Check if all players have locked their selections
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	bool AreAllPlayersReady() const;

	/**
	 * Get the number of players who have locked their selections
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	int32 GetNumPlayersReady() const;

	/**
	 * Get the total number of players in selection
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	int32 GetTotalPlayers() const { return PlayerSelections.Num(); }

	// ========== SELECTION PHASE CONTROL ==========

	/**
	 * Start the hero selection phase
	 * Call this when transitioning to hero select screen
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Hero Selection")
	void StartHeroSelection();

	/**
	 * End the hero selection phase and validate all selections
	 * Returns true if all selections are valid
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Hero Selection")
	bool EndHeroSelection();

	/**
	 * Reset hero selection (for new round or match)
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Hero Selection")
	void ResetHeroSelection();

	/**
	 * Force lock all players (e.g., when timer expires)
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Hero Selection")
	void ForceLockAllPlayers();

	// ========== CONFIGURATION ==========

	/**
	 * Whether to allow duplicate heroes on the same team
	 * Default: false (each team must have unique heroes)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hero Selection Rules")
	bool bAllowDuplicateHeroes = false;

	/**
	 * Maximum selection time in seconds (0 = no limit)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hero Selection Rules")
	float SelectionTimeLimit = 60.0f;

	/**
	 * Whether to automatically lock selections when time expires
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hero Selection Rules")
	bool bAutoLockOnTimeout = true;

	// ========== EVENTS ==========

	UPROPERTY(BlueprintAssignable, Category = "Hero Selection Events")
	FOnPlayerHeroSelectionChanged OnPlayerHeroSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Hero Selection Events")
	FOnPlayerHeroLocked OnPlayerHeroLocked;

	UPROPERTY(BlueprintAssignable, Category = "Hero Selection Events")
	FOnAllPlayersReady OnAllPlayersReady;

private:
	// Track all player selections
	UPROPERTY()
	TArray<FPlayerHeroSelectionState> PlayerSelections;

	// Timer handle for selection timeout
	FTimerHandle SelectionTimerHandle;

	// Internal flag for whether selection phase is active
	bool bSelectionActive = false;

	// Bind to player state delegates
	void BindToPlayerState(ABwayPlayerState* PlayerState);
	void UnbindFromPlayerState(ABwayPlayerState* PlayerState);

	// Callbacks for player state changes
	UFUNCTION()
	void OnPlayerSelectedHero(FPrimaryAssetId NewHeroId);

	// Handle selection timeout
	void OnSelectionTimeout();

	// Update a player's selection state
	void UpdatePlayerSelection(APlayerState* PlayerState);

	// Check and broadcast if all players are ready
	void CheckAllPlayersReady();

	// Get the game state
	ABwayGameState* GetBwayGameState() const;
};
