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

protected:
	//~AActor interface
	virtual void PostInitializeComponents() override;
	//~End of AActor interface

public:
	ABwayPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	// Relic possession tracking
	UPROPERTY(ReplicatedUsing=OnRep_HasRelic, BlueprintReadOnly, Category="Relic")
	bool bHasRelic;

	UFUNCTION(BlueprintCallable, Category="Relic")
	void SetHasRelic(bool bNewHasRelic);

	UFUNCTION()
	void OnRep_HasRelic();

	// Relic call tracking
	UPROPERTY(ReplicatedUsing=OnRep_HasCalledForRelic, BlueprintReadOnly, Category="Relic")
	bool bHasCalledForRelic;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Relic")
	void ServerSetHasCalledForRelic(bool bNewStatus);

	UFUNCTION()
	void OnRep_HasCalledForRelic();

	// Hero selection
	UFUNCTION(BlueprintCallable, Category = "Hero")
	void ServerSetSelectedHeroId(FPrimaryAssetId NewHeroId);

	UFUNCTION(BlueprintCallable, Category = "Hero")
	FPrimaryAssetId GetSelectedHeroId() const { return SelectedHeroId; }

	// Delegates for external UI listeners
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnSelectedHeroChanged OnSelectedHeroChanged;

	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing=OnRep_SelectedHeroId)
	FPrimaryAssetId SelectedHeroId;

	UFUNCTION()
	void OnRep_SelectedHeroId();
};
