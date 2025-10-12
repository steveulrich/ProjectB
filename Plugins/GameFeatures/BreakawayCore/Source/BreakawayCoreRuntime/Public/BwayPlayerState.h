// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/LyraPlayerState.h"
#include "BwayHeroDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "BwayPlayerState.generated.h"
/**
 * 
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API ABwayPlayerState : public ALyraPlayerState
{
	GENERATED_BODY()

public:
	ABwayPlayerState();
 
	/** Does the player currently possess the relic? */
	UPROPERTY(ReplicatedUsing=OnRep_HasRelic, BlueprintReadOnly, Category="Relic")
	bool bHasRelic;
 
	/** Server function to update possession */
	UFUNCTION(BlueprintCallable, Category="Relic")
	void SetHasRelic(bool bNewHasRelic);
 
	/** RepNotify called on clients when possession changes */
	UFUNCTION()
	void OnRep_HasRelic();
 
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing=OnRep_HasCalledForRelic, BlueprintReadOnly, Category="Relic")
	bool bHasCalledForRelic;
 
	UFUNCTION(Server, Reliable)
	void ServerSetHasCalledForRelic(bool bNewStatus);
 
	UFUNCTION()
	void OnRep_HasCalledForRelic();
	
    UFUNCTION(BlueprintCallable, Category = "Hero")
    void ServerSetSelectedHeroId(FPrimaryAssetId NewHeroId);

    UFUNCTION(BlueprintCallable, Category = "Hero")
    FPrimaryAssetId GetSelectedHeroId() const { return SelectedHeroId; }

    // Delegates for external UI listeners
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSelectedHeroChanged OnSelectedHeroChanged;
    
protected:

    UPROPERTY(ReplicatedUsing=OnRep_SelectedHeroId)
    FPrimaryAssetId SelectedHeroId;

    UFUNCTION()
    void OnRep_SelectedHeroId();
};
