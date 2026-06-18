// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameModes/BwayMatchPhaseTypes.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "BwayRoundManagementComponent.generated.h"

class ABwayGameState;
class UBwaySpawnPointManagerComponent;
class AController;
class APlayerState;
class UAbilitySystemComponent;
class ULyraGamePhaseAbility;

// Forward declare the round state enum so GameState.h can reference it
// (GameState.h includes this header)

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
 * Win condition types for Breakaway
 */
UENUM(BlueprintType)
enum class EBwayWinCondition : uint8
{
	None			UMETA(DisplayName = "None"),
	GoalScored		UMETA(DisplayName = "Goal Scored"),
	TeamEliminated	UMETA(DisplayName = "Team Eliminated"),
	TimeExpired		UMETA(DisplayName = "Time Expired - Possession (Legacy)"),
	SuddenDeath		UMETA(DisplayName = "Sudden Death - Midfield")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRoundStarted, int32, RoundNumber, float, RoundDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRoundEnded, int32, WinningTeam, EBwayWinCondition, WinCondition, int32, RoundNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMatchEnded, int32, WinningTeam, int32, TotalRounds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundStateChanged, FName, NewRoundState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundTimeChanged, int32, RemainingSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBetweenRoundPlanningStarted, int32, CompletedRoundNumber, float, PlanningDurationSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSuddenDeathWarning, int32, RemainingSeconds);

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	// ========================================
	// Player Death/Respawn
	// ========================================

	/** Called when a player dies — updates alive count, checks elimination, schedules respawn. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Player")
	void OnPlayerDied(AController* VictimController, AController* KillerController);

	/** Respawns a player after death timer. */
	void RespawnPlayer(AController* Controller);

	/** Check if match is over (a team reached the winning score). */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Scoring")
	bool CheckMatchEnd() const;

	// ========================================
	// Round State Accessors
	// ========================================

	/** Get the current round state */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Round")
	ERoundState GetCurrentRoundState() const { return CurrentRoundState; }

	/** Get the current round number (1-indexed) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Round")
	int32 GetCurrentRoundNumber() const { return CurrentRoundNumber; }

	/** Get remaining time in the current round (in seconds) */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Round")
	int32 GetRoundTimeRemaining() const;

	// ========================================
	// Match Flow Orchestration (11-3+)
	// ========================================

	/** Current top-level match phase (replicated). */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow")
	EBwayMatchPhase GetCurrentMatchPhase() const { return CurrentMatchPhase; }

	/** True when RM orchestrator owns phase flow and pawn spawn should stay frozen. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow")
	bool ShouldBlockPawnSpawning() const;

	/** True when bOrchestrateMatchFlow was honored on experience load. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow")
	bool IsOrchestratingMatchFlow() const { return bOrchestratorActive; }

	/** Begin match at Prematch — sole StartPhase entry for orchestrated flow (11-3). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Match Flow")
	void EnterPrematch();

	/** Advance to Warmup once per match — spawns pawns, starts warmup GAS phase + timer (11-4). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Match Flow")
	void EnterWarmup();

	/** Advance to Playing — starts playing GAS phase and round 1 FSM (11-4). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Match Flow")
	void EnterPlaying();

	/** Between-round pause after EndRound when match continues (11-6). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Match Flow")
	void EnterPostRound();

	// ========================================
	// Configuration
	// ========================================

	/** Points needed to win the match */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	int32 PointsToWin = 3;

	/** Authority-only runtime override from UBwayMatchFlowConfig / URL (11-1+). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Config")
	void SetPointsToWin(int32 InPointsToWin);

	/** Authority-only runtime override from UBwayMatchFlowConfig (11-5). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Config")
	void SetRoundDuration(float InRoundDuration);

	/** Duration of each round in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float RoundDuration = 180.0f;

	/** Time to wait before starting a new round after one ends */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float RoundEndDelay = 5.0f;

	/** Delay before respawning a player after death */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float RespawnDelay = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Economy")
	float GoldAwardForKill = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Economy")
	float GoldAwardForGoalScored = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Economy")
	float PassiveGoldPerSecond = 1.0f;

	/** Delay before starting first round */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float PreRoundDelay = 5.0f;

	/** Whether to automatically start the first round.
	 * Set to false when using Lyra Phase-driven flow (hero selection → warmup → playing).
	 * When false, the component listens for PlayingPhaseTag via ULyraGamePhaseSubsystem. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	bool bAutoStartFirstRound = false;

	/** Gameplay tag that represents the Playing phase (round active).
	 * When this phase begins, the component automatically calls StartRound(). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	FGameplayTag PlayingPhaseTag;

	/** Round time remaining (seconds) when sudden-death awareness is broadcast (log/UI hook). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config|Sudden Death", meta = (ClampMin = "0"))
	int32 SuddenDeathWarningSeconds = 60;

	/** Relic X within [-Tolerance, +Tolerance] of midfield counts as on the line (uses LastPossessingTeam). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config|Sudden Death", meta = (ClampMin = "0"))
	float MidfieldTolerance = 1.0f;

	// ========================================
	// Events
	// ========================================

	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRoundStarted OnRoundStarted;

	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRoundEnded OnRoundEnded;

	/** Broadcast when the match (not just a round) is over */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnMatchEnded OnMatchEnded;

	/** Broadcast when the round state changes (WaitingToStart / RoundActive / RoundEnding / RoundComplete). */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnRoundStateChanged OnRoundStateChanged;

	/** Fired after a round ends and before the next round starts (planning / buildable spend window). */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnBetweenRoundPlanningStarted OnBetweenRoundPlanningStarted;

	/** Fired once per round when remaining time crosses SuddenDeathWarningSeconds (awareness only). */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnSuddenDeathWarning OnSuddenDeathWarning;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Called by ULyraGamePhaseSubsystem when the Playing phase becomes active. */
	void HandlePlayingPhaseActivated(const FGameplayTag& ActivePhaseTag);

	/** High-priority experience hook: resolve config and start orchestrator or legacy listener (11-1 / 11-3). */
	void HandleExperienceLoadedForMatchRules(const class ULyraExperienceDefinition* Experience);

	void RegisterPlayingPhaseListener();

	void SetMatchPhase(EBwayMatchPhase NewPhase);

	void StartPrematchPhaseAbility();

	void StartPrematchPhaseAbilityImpl();

	void HandlePrematchPhaseEnded(const ULyraGamePhaseAbility* PhaseAbility);

	void HandlePrematchTimerExpired();

	void CompletePrematchPhase();

	void StartWarmupPhaseAbility();

	void StartWarmupPhaseAbilityImpl();

	void HandleWarmupPhaseEnded(const ULyraGamePhaseAbility* PhaseAbility);

	void HandleWarmupTimerExpired();

	void CompleteWarmupPhase();

	void StartPlayingPhaseAbility();

	void StartPlayingPhaseAbilityImpl();

	void HandlePlayingPhaseEnded(const ULyraGamePhaseAbility* PhaseAbility);

	void EnterPostMatch();

	void StopRoundFSM();

	void StartPostMatchPhaseAbility();

	void StartPostMatchPhaseAbilityImpl();

	void HandlePostMatchPhaseEnded(const ULyraGamePhaseAbility* PhaseAbility);

	void StartPostRoundPhaseAbility();

	void StartPostRoundPhaseAbilityImpl();

	void HandlePostRoundPhaseEnded(const ULyraGamePhaseAbility* PhaseAbility);

	void HandlePostRoundTimerExpired();

	void CompletePostRoundPhase();

	void RestartDeferredPlayersForWarmup();

	/** True when ERoundState FSM (timer, EndRound, sudden death) may run. */
	bool IsRoundLifecycleActive() const;

	void ClearBetweenRoundTimer();

	bool IsCanonicalRoundManagementInstance() const;

	TSubclassOf<ULyraGamePhaseAbility> ResolvePrematchPhaseAbilityClass() const;

	TSubclassOf<ULyraGamePhaseAbility> ResolveWarmupPhaseAbilityClass() const;

	TSubclassOf<ULyraGamePhaseAbility> ResolvePlayingPhaseAbilityClass() const;

	TSubclassOf<ULyraGamePhaseAbility> ResolvePostRoundPhaseAbilityClass() const;

	TSubclassOf<ULyraGamePhaseAbility> ResolvePostMatchPhaseAbilityClass() const;

	/** Get the owning game state cast to ABwayGameState */
	ABwayGameState* GetBwayGameState() const;

	/** Determine which team has relic possession based on location/carrier */
	int32 DetermineRelicPossessionTeam() const;

	/** Set the round state and broadcast changes */
	void SetRoundState(ERoundState NewState);

	void ApplyGoldDeltaToPlayerState(APlayerState* PlayerState, float GoldDelta) const;
	FActiveGameplayEffectHandle ApplyPassiveGoldIncomeToPlayerState(APlayerState* PlayerState) const;
	void RemovePassiveGoldIncome();

	// ========================================
	// Replicated Round State
	// ========================================

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

	/** Top-level match phase owned by RM orchestrator (11-3+). */
	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase)
	EBwayMatchPhase CurrentMatchPhase = EBwayMatchPhase::None;

	UFUNCTION()
	void OnRep_MatchPhase();

	/** Resolved match-flow settings cached on experience load when orchestrator may run. */
	FBwayResolvedMatchFlowSettings ResolvedMatchFlowSettings;

	/** When true, RM is the sole StartPhase caller and legacy PlayingPhaseTag listener is inactive. */
	bool bOrchestratorActive = false;

	bool bPlayingPhaseListenerRegistered = false;

	bool bPrematchCompletionHandled = false;

	bool bWarmupCompletionHandled = false;

	bool bPostRoundCompletionHandled = false;

	/** Guards against duplicate experience-load handling on the canonical component. */
	bool bMatchFlowExperienceHandled = false;

	FTimerHandle MatchPhaseTimerHandle;

	/** Legacy delay between rounds when bOrchestrateMatchFlow is false; orchestrated flow uses PostRoundDuration (11-6). */
	FTimerHandle BetweenRoundTimerHandle;

	/** Last time we broadcast the round time update */
	float LastRoundTimeUpdateBroadcast = 0.0f;

	/** Timer handles */
	FTimerHandle PreRoundTimerHandle;

	/** Per-player respawn timers */
	TMap<TObjectPtr<AController>, FTimerHandle> RespawnTimers;

	TMap<TObjectPtr<APlayerState>, FActiveGameplayEffectHandle> PassiveGoldEffectHandles;

	bool bSuddenDeathWarningBroadcastThisRound = false;
};
