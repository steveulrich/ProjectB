// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/LyraPlayerState.h"
#include "HeroSystems/BwayHeroDataAsset.h"
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerNumAssigned, int32, PlayerNum);

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

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPlayerNumAssigned OnPlayerNumAssigned;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "PlayerState")
	void SetPlayerNum(int32 NewPlayerNum);

	UFUNCTION(BlueprintCallable, Category = "PlayerState")
	int32 GetPlayerNum() const { return PlayerNum; }

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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
};
