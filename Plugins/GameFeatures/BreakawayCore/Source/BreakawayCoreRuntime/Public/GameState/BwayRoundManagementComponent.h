// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameModes/BwayMatchPhaseTypes.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "BwayRoundManagementComponent.generated.h"

class ABwayGameState;
class ABwayPlayerState;
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
DECLARE_MULTICAST_DELEGATE(FOnRoundResetStarting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPostRoundSummaryStarted, FBwayPostRoundSummaryData, SummaryData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchPhaseChanged, EBwayMatchPhase, NewPhase);
/** @deprecated Use OnPostRoundSummaryStarted — retained for legacy Blueprint bindings. */
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

	/** Alias for StartRound() used by round-boundary callers and tests. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Round")
	void StartNewRound();

	/** End the current round with a specific win condition. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Round")
	void EndRound(int32 WinningTeam, EBwayWinCondition WinCondition);

	/** Reset round state (relic, players, buildables, and round-scoped respawn escalation when enabled). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Round")
	void ResetRoundState();

	/** Reset per-player within-round death counts to 0 (called on round start when bResetRespawnEscalationEachRound is true, and on match reset). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Round")
	void ResetRoundRespawnEscalation();

	/** Reset round/respawn escalation and timers for a new match. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Round")
	void ResetForNextMatch();

	// ========================================
	// Win Condition Checks
	// ========================================

	/** Called when the relic enters a goal trigger. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Scoring")
	void OnRelicScored(int32 ScoringTeam, ABwayPlayerState* Scorer = nullptr);

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

	/** Recalculate a pending death after a committed shop transaction, preserving elapsed time. */
	void RefreshRespawnSchedule(APlayerState* Player);

	/** Get number of deaths recorded for PlayerState in the current round. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Player")
	int32 GetPlayerDeathsInCurrentRound(const APlayerState* PlayerState) const;

	/** Resolve configured base respawn delay, honoring direct RespawnDelay overrides from legacy probes. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Player")
	float GetConfiguredBaseRespawnDelaySeconds() const;

	/** Compute escalated base respawn delay for PriorDeathsInRound (0 = 1st death -> BaseRespawnDelaySeconds, clamped to MaxRespawnDelaySeconds). */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Player")
	float ComputeEscalatedRespawnBaseDelay(int32 PriorDeathsInRound) const;

	/** Compute effective respawn delay for PlayerState given PriorDeathsInRound and their current RespawnTimeReduction attribute. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Player")
	float ComputeEffectiveRespawnDelayForPlayer(const APlayerState* PlayerState, int32 PriorDeathsInRound) const;

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

	/** Get remaining time in the pre-round Warmup / Shop phase (in seconds, e.g. 15 -> 0). */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow")
	int32 GetWarmupTimeRemaining() const;

	/** Get remaining time in the Prematch countdown phase (in seconds). */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow")
	int32 GetPrematchTimeRemaining() const;

	/** True while the Arena Intro camera fly-through sequence is running between Hero Selection and Warmup. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow")
	bool IsArenaIntroActive() const { return bArenaIntroActive; }

	/** True once the Arena Intro sequence has run in this match. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow")
	bool HasArenaIntroPlayed() const { return bArenaIntroPlayed; }

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

	/** Direct PIE: retry prematch completion after human hero locks (Step 17). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Match Flow")
	void TryCompletePrematchAfterHeroLocks();

	/** Advance to Playing — starts playing GAS phase and round 1 FSM (11-4). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Match Flow")
	void EnterPlaying();

	/** Between-round pause after EndRound when match continues (11-6). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Match Flow")
	void EnterPostRound();

	/** True while PostRound summary data is active for UI. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow")
	bool HasActivePostRoundSummary() const { return bHasActivePostRoundSummary; }

	/** Replicated PostRound summary payload for UI. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow")
	FBwayPostRoundSummaryData GetActivePostRoundSummary() const { return ReplicatedPostRoundSummary; }

	/** Authoritative per-round win conditions (as uint8 EBwayWinCondition) accumulated for TeamIndex (0 or 1). */
	const TArray<uint8>& GetTeamRoundWinHistory(int32 TeamIndex) const
	{
		return (TeamIndex == 1) ? Team1RoundWinHistory : Team0RoundWinHistory;
	}

	// ========================================
	// Configuration
	// ========================================

	/** Authority-resolved win threshold, including hosted rule overrides. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Breakaway|Config")
	int32 PointsToWin = 3;

	/** Authority-only runtime override from UBwayMatchFlowConfig / URL (11-1+). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Config")
	void SetPointsToWin(int32 InPointsToWin);

	/** Authority-only runtime override from UBwayMatchFlowConfig (11-5). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Config")
	void SetRoundDuration(float InRoundDuration);

	/** Authority-only runtime override for escalating respawn delay progression from UBwayMatchFlowConfig / URL. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Config")
	void SetRespawnProgressionRules(
		float InBaseRespawnDelaySeconds,
		float InPerDeathRespawnIncrementSeconds,
		float InMaxRespawnDelaySeconds,
		bool bInResetEachRound);

	/** Duration of each round in seconds (class default; runtime value replicated from authority match-flow resolve). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_RoundDuration, Category = "Breakaway|Config")
	float RoundDuration = 240.0f;

	/** Time to wait before starting a new round after one ends */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float RoundEndDelay = 5.0f;

	/** Base delay in seconds before respawning a player on their first death of the round (Sept 7, 2017 GameSpot evidence: 10.0s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Breakaway|Config|Respawn", meta = (ClampMin = "0.05"))
	float BaseRespawnDelaySeconds = 10.0f;

	/** Additional respawn delay in seconds added per prior death within the current round (5.0s -> 10s, 15s, 20s, 25s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Breakaway|Config|Respawn", meta = (ClampMin = "0.0"))
	float PerDeathRespawnIncrementSeconds = 5.0f;

	/** Maximum escalated base respawn delay in seconds before RespawnTimeReduction is applied (25.0s cap). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Breakaway|Config|Respawn", meta = (ClampMin = "0.05"))
	float MaxRespawnDelaySeconds = 25.0f;

	/** When true, per-player within-round death escalation resets to 0 at each round boundary. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Breakaway|Config|Respawn")
	bool bResetRespawnEscalationEachRound = true;

	/** Synchronized base respawn delay alias retained for legacy probes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Config")
	float RespawnDelay = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Economy")
	float GoldAwardForKill = 50.0f;

	/** Provisional assist tuning; final-public-alpha reference values remain unverified. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Economy", meta = (ClampMin = "0"))
	float GoldAwardForAssist = 25.0f;

	/** Recent positive enemy health damage, once per contributor excluding the killer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Combat", meta = (ClampMin = "0"))
	float AssistCreditWindowSeconds = 10.0f;

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

	/** Authority cleanup before a direct round/rematch reset replaces pawns. */
	FOnRoundResetStarting OnRoundResetStarting;

	/** Fired after a round ends with team-aggregate round stats for PostRoundSummary UI (Step 15). */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnPostRoundSummaryStarted OnPostRoundSummaryStarted;

	/** Fired when replicated match phase changes (PostRound dismiss, etc.). */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnMatchPhaseChanged OnMatchPhaseChanged;

	/** @deprecated Use OnPostRoundSummaryStarted. */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events", meta = (DeprecatedProperty, DeprecationMessage = "Use OnPostRoundSummaryStarted"))
	FOnBetweenRoundPlanningStarted OnBetweenRoundPlanningStarted;

	/** Fired once per round when remaining time crosses SuddenDeathWarningSeconds (awareness only). */
	UPROPERTY(BlueprintAssignable, Category = "Breakaway|Events")
	FOnSuddenDeathWarning OnSuddenDeathWarning;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	void StartArenaIntroSequence(float DurationSeconds);

	void FinishArenaIntroSequence();

	void SeedArenaIntroStartingGold();

	bool ShouldWaitForDirectPieHeroLocks() const;

	bool AreAllHumanPlayersHeroLocked() const;

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
	void ClearRespawnTimers();

	bool IsCanonicalRoundManagementInstance() const;

	TSubclassOf<ULyraGamePhaseAbility> ResolvePrematchPhaseAbilityClass() const;

	TSubclassOf<ULyraGamePhaseAbility> ResolveWarmupPhaseAbilityClass() const;

	TSubclassOf<ULyraGamePhaseAbility> ResolvePlayingPhaseAbilityClass() const;

	TSubclassOf<ULyraGamePhaseAbility> ResolvePostRoundPhaseAbilityClass() const;

	TSubclassOf<ULyraGamePhaseAbility> ResolvePostMatchPhaseAbilityClass() const;

	/** Get the owning game state cast to ABwayGameState */
	ABwayGameState* GetBwayGameState() const;

	void BeginRoundStatTrackingForAllPlayers();

	void FinalizeRoundStatsForAllPlayers();

	void BuildAndBroadcastPostRoundSummary(int32 RoundWinningTeam, float DisplayDurationSeconds);

	void ClearActivePostRoundSummary();

	void NotifyPostRoundSummaryListeners();

	UFUNCTION()
	void OnRep_PostRoundSummary();

	UFUNCTION()
	void OnRep_RoundDuration();

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

	/** When the Warmup / Pre-Round Shop phase started (server time) */
	UPROPERTY(Replicated)
	float WarmupStartTime = 0.0f;

	/** Replicated duration of the Warmup / Pre-Round Shop phase in seconds */
	UPROPERTY(Replicated)
	float WarmupDurationSeconds = 15.0f;

	/** When the Prematch phase started (server time) */
	UPROPERTY(Replicated)
	float PrematchStartTime = 0.0f;

	/** Replicated configured duration of the Prematch phase in seconds */
	UPROPERTY(Replicated)
	float PrematchDurationSeconds = 0.0f;

	/** Replicated flag indicating the Arena Intro fly-through is currently active */
	UPROPERTY(Replicated)
	bool bArenaIntroActive = false;

	/** Replicated flag indicating the Arena Intro fly-through ran for this match */
	UPROPERTY(Replicated)
	bool bArenaIntroPlayed = false;

	FTimerHandle ArenaIntroTimerHandle;

	/** Current round number */
	UPROPERTY(Replicated)
	int32 CurrentRoundNumber = 0;

	/** Top-level match phase owned by RM orchestrator (11-3+). */
	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase)
	EBwayMatchPhase CurrentMatchPhase = EBwayMatchPhase::None;

	UFUNCTION()
	void OnRep_MatchPhase();

	UPROPERTY(ReplicatedUsing = OnRep_PostRoundSummary)
	FBwayPostRoundSummaryData ReplicatedPostRoundSummary;

	UPROPERTY(Replicated)
	bool bHasActivePostRoundSummary = false;

	/** Resolved match-flow settings cached on experience load when orchestrator may run. */
	FBwayResolvedMatchFlowSettings ResolvedMatchFlowSettings;

	/** When true, RM is the sole StartPhase caller and legacy PlayingPhaseTag listener is inactive. */
	bool bOrchestratorActive = false;

	bool bPlayingPhaseListenerRegistered = false;

	bool bPrematchCompletionHandled = false;

	bool bWaitingForDirectPieHeroLock = false;

	bool bWarmupCompletionHandled = false;

	bool bPostRoundCompletionHandled = false;

	/** When true, CompletePostRoundPhase advances to PostMatch instead of Playing. */
	bool bPendingMatchEndAfterPostRound = false;

	int32 PendingMatchWinningTeam = INDEX_NONE;

	/** Guards against duplicate experience-load handling on the canonical component. */
	bool bMatchFlowExperienceHandled = false;

	FTimerHandle MatchPhaseTimerHandle;

	/** Legacy delay between rounds when bOrchestrateMatchFlow is false; orchestrated flow uses PostRoundDuration (11-6). */
	FTimerHandle BetweenRoundTimerHandle;

	/** Last time we broadcast the round time update */
	float LastRoundTimeUpdateBroadcast = 0.0f;

	/** Timer handles */
	FTimerHandle PreRoundTimerHandle;

	friend class FBwayRespawnUpgradeTest;
	friend class FBwayRespawnEscalatingProgressionTest;
	struct FPendingRespawn
	{
		FTimerHandle Timer;
		double StartTime = 0.0;
		float BaseDelay = 0.0f;
		float AppliedReduction = -1.0f;
		int32 Round = 0;
	};
	/** Preserve the death's original delay and clock when buying or replacing a rank. */
	TMap<TWeakObjectPtr<AController>, FPendingRespawn> RespawnTimers;

	/** Authoritative per-player death count within the current round for escalating respawn delay progression. */
	TMap<TWeakObjectPtr<APlayerState>, int32> PlayerRoundDeathCounts;

	/** Non-owning: disconnects and bot replacement can destroy players before round cleanup. */
	TMap<TWeakObjectPtr<APlayerState>, FActiveGameplayEffectHandle> PassiveGoldEffectHandles;

	bool bSuddenDeathWarningBroadcastThisRound = false;

	EBwayWinCondition LastRoundWinCondition = EBwayWinCondition::None;
	FString LastRoundScoringPlayerName;
	TArray<uint8> Team0RoundWinHistory;
	TArray<uint8> Team1RoundWinHistory;
};
