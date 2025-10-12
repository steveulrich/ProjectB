// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayPlayerState.h"
 
ABwayPlayerState::ABwayPlayerState()
{
	bHasRelic = false;
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
}

void ABwayPlayerState::OnRep_HasCalledForRelic()
{
	// Blueprint event can bind here to update marker visibility on player mesh
}
 
void ABwayPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 
	DOREPLIFETIME(ABwayPlayerState, bHasRelic);
	DOREPLIFETIME(ABwayPlayerState, bHasCalledForRelic);
}

void ABwayPlayerState::ServerSetHasCalledForRelic_Implementation(bool bNewStatus)
{
	bHasCalledForRelic = bNewStatus;
	OnRep_HasCalledForRelic();
}
 