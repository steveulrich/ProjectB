// Fill out your copyright notice in the Description page of Project Settings.

#include "BwayPlayerState.h"
#include "Net/UnrealNetwork.h"

ABwayPlayerState::ABwayPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasRelic = false;
	bHasCalledForRelic = false;
	bHeroLocked = false;
}

void ABwayPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: PostInitializeComponents called for %s"), *GetName());
}

// ========== RELIC SYSTEM ==========

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
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s now %s the relic"), 
		*GetName(), bHasRelic ? TEXT("has") : TEXT("does not have"));
	
	// Blueprint events can bind here to update UI or player effects
}

void ABwayPlayerState::ServerSetHasCalledForRelic_Implementation(bool bNewStatus)
{
	bHasCalledForRelic = bNewStatus;
	OnRep_HasCalledForRelic();
}

void ABwayPlayerState::OnRep_HasCalledForRelic()
{
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s has called for relic: %s"), 
		*GetName(), bHasCalledForRelic ? TEXT("true") : TEXT("false"));
	
	// Blueprint events can bind here to update marker visibility on player mesh
}

// ========== HERO SELECTION SYSTEM ==========

void ABwayPlayerState::ServerSetSelectedHeroId_Implementation(FPrimaryAssetId NewHeroId)
{
	// Don't allow changes if hero is already locked
	if (bHeroLocked)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPlayerState: Cannot change hero for %s - selection is locked"), *GetName());
		return;
	}

	// Validate the hero ID is not null
	if (!NewHeroId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPlayerState: Attempted to set invalid hero ID for %s"), *GetName());
		return;
	}

	SelectedHeroId = NewHeroId;
	OnRep_SelectedHeroId();
	
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s selected hero: %s"), *GetName(), *SelectedHeroId.ToString());
}

void ABwayPlayerState::OnRep_SelectedHeroId()
{
	// Broadcast to any listeners (UI, game systems, etc.)
	OnSelectedHeroChanged.Broadcast(SelectedHeroId);
	
	UE_LOG(LogTemp, Verbose, TEXT("BwayPlayerState: Selected hero replicated to %s"), *SelectedHeroId.ToString());
}

void ABwayPlayerState::ServerLockHeroSelection_Implementation()
{
	// Validate we have a hero selected
	if (!SelectedHeroId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPlayerState: Cannot lock hero selection for %s - no hero selected"), *GetName());
		return;
	}

	bHeroLocked = true;
	OnRep_HeroLocked();
	
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: Hero selection locked for %s (Hero: %s)"), 
		*GetName(), *SelectedHeroId.ToString());
}

void ABwayPlayerState::OnRep_HeroLocked()
{
	UE_LOG(LogTemp, Verbose, TEXT("BwayPlayerState: Hero lock status changed to %s for %s"), 
		bHeroLocked ? TEXT("LOCKED") : TEXT("UNLOCKED"), *GetName());
	
	// Blueprint events can bind here to update UI (disable hero change buttons, etc.)
}

void ABwayPlayerState::UnlockHeroSelection()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayPlayerState: UnlockHeroSelection called on client - this is authority-only"));
		return;
	}

	bHeroLocked = false;
	OnRep_HeroLocked();
	
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: Hero selection unlocked for %s"), *GetName());
}

// ========== REPLICATION ==========

void ABwayPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABwayPlayerState, bHasRelic);
	DOREPLIFETIME(ABwayPlayerState, bHasCalledForRelic);
	DOREPLIFETIME(ABwayPlayerState, SelectedHeroId);
	DOREPLIFETIME(ABwayPlayerState, bHeroLocked);
	DOREPLIFETIME(ABwayPlayerState, PlayerNum);
}

void ABwayPlayerState::SetPlayerNum(int32 NewPlayerNum)
{
	if (HasAuthority())
	{
		PlayerNum = NewPlayerNum;
		OnRep_PlayerNum();
	}
}

void ABwayPlayerState::OnRep_PlayerNum()
{
	OnPlayerNumAssigned.Broadcast(PlayerNum);
}
