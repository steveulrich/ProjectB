// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameModes/LyraGameState.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "BwayGameState.generated.h"

class ARelicActor;
class ABwayCharacterWithAbilities;
class UBwayHeroSelectionManager;
class UBwayHeroSelectionPhaseComponent;
class UBwayRoundManagementComponent;
class UBwayScoringComponent;
class UBwayRelicManagerComponent;
class UBwayMidfieldDividerComponent;
class UBwayTeamBridgeComponent;
class UBwayBuildableRegistryComponent;
class UUserWidget;
class ULyraGamePhaseAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchStateChanged, FName, NewMatchState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamsUpdated);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero Selection")
	TObjectPtr<UBwayHeroSelectionManager> HeroSelectionManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero Selection")
	TObjectPtr<UBwayHeroSelectionPhaseComponent> HeroSelectionPhaseComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakaway|Match")
	TObjectPtr<UBwayRoundManagementComponent> RoundManagementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakaway|Match")
	TObjectPtr<UBwayScoringComponent> ScoringComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakaway|Match")
	TObjectPtr<UBwayRelicManagerComponent> RelicManagerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakaway|Match")
	TObjectPtr<UBwayMidfieldDividerComponent> MidfieldDividerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakaway|Match")
	TObjectPtr<UBwayTeamBridgeComponent> TeamBridgeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakaway|Buildables")
	TObjectPtr<UBwayBuildableRegistryComponent> BuildableRegistryComponent;

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
	// Round State (delegates to UBwayRoundManagementComponent)
	// ========================================

	/** Get the RoundManagementComponent (convenience accessor) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Round")
	UBwayRoundManagementComponent* GetRoundManagement() const;

	/** Only the C++ default RoundManagementComponent may run match-flow orchestration. */
	bool IsCanonicalRoundManagement(const UBwayRoundManagementComponent* Component) const;

	/** Get the current round state (delegates to RoundManagementComponent) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Round")
	ERoundState GetCurrentRoundState() const;

	/** Get remaining time in the current round (delegates to RoundManagementComponent) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Round")
	int32 GetRoundTimeRemaining() const;

	/** Get the current round number (delegates to RoundManagementComponent) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Round")
	int32 GetCurrentRoundNumber() const;

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
	// Match Flow Control
	// ========================================

	/**
	 * Show the post-match results screen (presentation only — PostMatch GAS phase is owned by RoundManagement).
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match Flow")
	void TransitionToPostGame(int32 WinningTeam);

	/** Show the post-game results screen to all players - implement in Blueprint */
	UFUNCTION(BlueprintNativeEvent, Category = "Match Flow")
	void ShowResultsScreen(int32 WinningTeam);
	virtual void ShowResultsScreen_Implementation(int32 WinningTeam);

	/**
	 * Return all players to the front-end / lobby map.
	 */
	UFUNCTION(BlueprintCallable, Category = "Match Flow")
	void ReturnToFrontEnd();

	/** Called when RoundManagement detects match end */
	UFUNCTION()
	void HandleMatchEnded(int32 WinningTeam, int32 TotalRounds);

	/** Maximum number of players per team */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	int32 MaxPlayersPerTeam = 4;

	/** @deprecated PostMatch phase ability is configured on UBwayMatchFlowConfig and started by RoundManagement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow", meta = (DeprecatedProperty, DeprecationMessage = "Use DA_BW_MatchFlow_Dev PostMatchPhaseAbility instead"))
	TSubclassOf<ULyraGamePhaseAbility> PostGamePhaseAbilityClass;

	/**
	 * Widget class for the post-match results screen.
	 * Must be a subclass of UBwayResultsScreenWidget.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow")
	TSoftClassPtr<UUserWidget> ResultsScreenWidgetClass;

	/**
	 * Widget class for the between-round PostRound summary (Step 15).
	 * Must be a subclass of UBwayPostRoundSummaryWidget.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow")
	TSoftClassPtr<UUserWidget> PostRoundSummaryWidgetClass;

	/**
	 * Map to load when returning to lobby / front-end.
	 * Optional override for 11-8+; 11-7 always travels to L_LyraFrontEnd unless this points there too.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow")
	TSoftObjectPtr<UWorld> FrontEndLevel;

	// ========================================
	// Events/Delegates
	// ========================================

	/** Broadcast when round state changes */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRoundStateChanged OnRoundStateChanged;

	/** Broadcast when match state changes */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnMatchStateChanged OnMatchStateChanged;

	/** Broadcast when round time updates (every second) */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRoundTimeChanged OnRoundTimeChanged;

	/** Broadcast when team members change (add/remove) */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnTeamsUpdated OnTeamsUpdated;

protected:
	// ========================================
	// Replicated Properties
	// ========================================

	/** Team information arrays */
	UPROPERTY(ReplicatedUsing = OnRep_TeamInfo)
	TArray<FTeamInfo> Teams;

	UFUNCTION()
	void OnRep_TeamInfo();

	// ========================================
	// Internal State
	// ========================================

	/** Initialize teams on begin play */
	void InitializeTeams();

	/** Helper to find team index for an actor */
	int32 GetTeamIndexForActor(const AActor* Actor) const;
};
