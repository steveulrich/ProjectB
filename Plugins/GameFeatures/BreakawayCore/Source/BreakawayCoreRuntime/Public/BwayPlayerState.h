// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/LyraPlayerState.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "Net/UnrealNetwork.h"
#include "BwayPlayerState.generated.h"

/**
 * ABwayPlayerState
 * 
 * Extended PlayerState for Breakaway that handles:
 * - Relic possession tracking
 * - Hero selection and locking
 * - Team-specific data
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedHeroChanged, FPrimaryAssetId, NewHeroId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeroLocked, FPrimaryAssetId, LockedHeroId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerNumAssigned, int32, PlayerNum);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchStatsChanged);

UCLASS()
class BREAKAWAYCORERUNTIME_API ABwayPlayerState : public ALyraPlayerState
{
	GENERATED_BODY()

protected:
	//~AActor interface
	virtual void PostInitializeComponents() override;
	//~End of AActor interface

public:
	ABwayPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	// ========== RELIC SYSTEM ==========
	
	// Relic possession tracking
	UPROPERTY(ReplicatedUsing=OnRep_HasRelic, BlueprintReadOnly, Category="Relic")
	bool bHasRelic;

	UFUNCTION(BlueprintCallable, Category="Relic")
	void SetHasRelic(bool bNewHasRelic);

	UFUNCTION()
	void OnRep_HasRelic();

	// Relic call tracking (for "Pass to me!" indicators)
	UPROPERTY(ReplicatedUsing=OnRep_HasCalledForRelic, BlueprintReadOnly, Category="Relic")
	bool bHasCalledForRelic;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Relic")
	void ServerSetHasCalledForRelic(bool bNewStatus);

	UFUNCTION()
	void OnRep_HasCalledForRelic();

	// ========== HERO SELECTION SYSTEM ==========
	
	/**
	 * Server RPC to set the player's selected hero
	 * Called by clients during hero selection phase
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Hero")
	void ServerSetSelectedHeroId(FPrimaryAssetId NewHeroId);

	/**
	 * Get the currently selected hero ID
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero")
	FPrimaryAssetId GetSelectedHeroId() const { return SelectedHeroId; }

	/**
	 * Check if this player's hero selection is locked
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero")
	bool IsHeroLocked() const { return bHeroLocked; }

	/**
	 * Server RPC to lock the player's hero selection
	 * Once locked, the player cannot change their hero
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Hero")
	void ServerLockHeroSelection();

	/**
	 * Authority-only function to unlock hero selection (for round resets)
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Hero")
	void UnlockHeroSelection();

	// Delegates for external UI listeners
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnSelectedHeroChanged OnSelectedHeroChanged;

	/** Fired (on client) when bHeroLocked becomes true. Passes the locked hero ID. */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHeroLocked OnHeroLocked;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPlayerNumAssigned OnPlayerNumAssigned;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "PlayerState")
	void SetPlayerNum(int32 NewPlayerNum);

	UFUNCTION(BlueprintCallable, Category = "PlayerState")
	int32 GetPlayerNum() const { return PlayerNum; }

	// ========== MATCH STATS ==========

	/** Get kills this match */
	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetKills() const { return Kills; }

	/** Get deaths this match */
	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetDeaths() const { return Deaths; }

	/** Get assists this match */
	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetAssists() const { return Assists; }

	/** Get objective score this match (goals scored, relics captured, etc.) */
	UFUNCTION(BlueprintPure, Category = "Stats")
	int32 GetObjectiveScore() const { return ObjectiveScore; }

	/** Server-only: Record a kill */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stats")
	void AddKill();

	/** Server-only: Record a death */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stats")
	void AddDeath();

	/** Server-only: Record an assist */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stats")
	void AddAssist();

	/** Server-only: Record an objective score */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stats")
	void AddObjectiveScore(int32 Amount = 1);

	/** Server-only: Reset all match stats (between matches) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stats")
	void ResetMatchStats();

	/** Snapshot of all tracked match stats (including economy / relic stubs). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Stats")
	FBwayPlayerMatchStats GetMatchStatsSnapshot() const;

	/** Stats earned during the most recently completed round (replicated). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Stats")
	FBwayPlayerMatchStats GetLastRoundStats() const { return LastRoundStats; }

	/** Server-only: capture baseline at round start for round-only deltas. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stats")
	void BeginRoundStatTracking();

	/** Server-only: finalize round deltas into LastRoundStats. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Stats")
	void FinalizeRoundStats();

	/** Fired when any stat changes (K/D/A/Objective). Used by scoreboard/results UI. */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnMatchStatsChanged OnMatchStatsChanged;

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;

protected:
	// The selected hero asset ID
	UPROPERTY(ReplicatedUsing=OnRep_SelectedHeroId)
	FPrimaryAssetId SelectedHeroId;

	// Whether the hero selection is locked (can't change anymore)
	UPROPERTY(ReplicatedUsing=OnRep_HeroLocked)
	bool bHeroLocked;

	// Player number assigned to this player
	UPROPERTY(ReplicatedUsing=OnRep_PlayerNum)
	int32 PlayerNum = 0;

	UFUNCTION()
	void OnRep_SelectedHeroId();

	UFUNCTION()
	void OnRep_HeroLocked();

	UFUNCTION()
	void OnRep_PlayerNum();

	// ========== Match Stats (Replicated) ==========

	UPROPERTY(ReplicatedUsing=OnRep_MatchStats)
	int32 Kills = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MatchStats)
	int32 Deaths = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MatchStats)
	int32 Assists = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MatchStats)
	int32 ObjectiveScore = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MatchStats)
	int32 DamageDealt = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MatchStats)
	int32 HealingDone = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MatchStats)
	int32 ForcedFumbles = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MatchStats)
	int32 Interceptions = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MatchStats)
	int32 BuildablesDestroyed = 0;

	UPROPERTY(ReplicatedUsing=OnRep_LastRoundStats)
	FBwayPlayerMatchStats LastRoundStats;

	/** Server-only baseline captured at round start. */
	FBwayPlayerMatchStats RoundStartStatsBaseline;

	/** Server-only gold at round start for GoldEarned delta. */
	int32 GoldAtRoundStart = 0;

	UFUNCTION()
	void OnRep_MatchStats();

	UFUNCTION()
	void OnRep_LastRoundStats();

	FBwayPlayerMatchStats BuildCurrentStatSnapshot() const;
	int32 GetCurrentGoldTotal() const;
};
