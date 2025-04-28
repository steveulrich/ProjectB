// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayPlayerState.h"
#include "Net/UnrealNetwork.h"

// In BwayPlayerState.cpp, add these implementations:
void ABwayPlayerState::BeginPlay()
{
	// Call the base PlayerState implementation
	AActor::BeginPlay();
    
	// Add your custom initialization logic here
}

void ABwayPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Add your cleanup logic here
    
	// Call the base PlayerState implementation
	AActor::EndPlay(EndPlayReason);
}

ABwayPlayerState::ABwayPlayerState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Initialize the player state
	bIsRequestingRelic = false;
}

void ABwayPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABwayPlayerState, bIsRequestingRelic, COND_None); // Replicate to everyone
}

void ABwayPlayerState::OnRep_IsRequestingRelic()
{
	// This function now runs on all clients when *this specific player's*
	// bIsRequestingRelic state changes.
	// Instead of directly controlling a widget here, we broadcast a delegate
	// that the relevant HUD widget can bind to.
	OnRequestingRelicChanged.Broadcast(bIsRequestingRelic);
}

void ABwayPlayerState::ServerSetIsRequestingRelic_Implementation(bool bNewRequestingState)
{
	if (bIsRequestingRelic!= bNewRequestingState)
	{
		bIsRequestingRelic = bNewRequestingState;
		// Manually call RepNotify on server if needed, and broadcast delegate
		OnRep_IsRequestingRelic();
	}
}
