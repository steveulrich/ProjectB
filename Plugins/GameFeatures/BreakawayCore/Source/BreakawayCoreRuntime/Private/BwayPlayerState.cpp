// Fill out your copyright notice in the Description page of Project Settings.

#include "BwayPlayerState.h"

ABwayPlayerState::ABwayPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasRelic = false;
	bHasCalledForRelic = false;
}

void ABwayPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Any initialization logic that would have been in BeginPlay goes here
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: PostInitializeComponents called for %s"), *GetName());
}

void ABwayPlayerState::SetHasRelic(bool bNewHasRelic)
{
	if (HasAuthority())
	{
		bHasRelic = bNewHasRelic;
		OnRep_HasRelic();
	}
}

void ABwayPlayerState::OnRep_HasRelic()
{
	// Implement Blueprint bindings to update UI or player effects when this triggers
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s now %s the relic"), 
		*GetName(), bHasRelic ? TEXT("has") : TEXT("does not have"));
}

void ABwayPlayerState::ServerSetHasCalledForRelic_Implementation(bool bNewStatus)
{
	bHasCalledForRelic = bNewStatus;
	OnRep_HasCalledForRelic();
}

void ABwayPlayerState::OnRep_HasCalledForRelic()
{
	// Blueprint event can bind here to update marker visibility on player mesh
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s has called for relic: %s"), 
		*GetName(), bHasCalledForRelic ? TEXT("true") : TEXT("false"));
}

void ABwayPlayerState::ServerSetSelectedHeroId(FPrimaryAssetId NewHeroId)
{
	if (HasAuthority())
	{
		SelectedHeroId = NewHeroId;
		OnRep_SelectedHeroId();
	}
}

void ABwayPlayerState::OnRep_SelectedHeroId()
{
	OnSelectedHeroChanged.Broadcast(SelectedHeroId);
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: Selected hero changed to %s"), *SelectedHeroId.ToString());
}

void ABwayPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABwayPlayerState, bHasRelic);
	DOREPLIFETIME(ABwayPlayerState, bHasCalledForRelic);
	DOREPLIFETIME(ABwayPlayerState, SelectedHeroId);
}