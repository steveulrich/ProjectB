// Fill out your copyright notice in the Description page of Project Settings.

#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
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

	if (HasAuthority())
	{
		if (ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent())
		{
			if (!ASC->GetSet<UBwayGoldAttributeSet>())
			{
				UBwayGoldAttributeSet* GoldSet = NewObject<UBwayGoldAttributeSet>(this);
				ASC->AddSpawnedAttribute(GoldSet);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: PostInitializeComponents called for %s"), *GetName());
}

void ABwayPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (ABwayPlayerState* BwayPlayerState = Cast<ABwayPlayerState>(PlayerState))
	{
		BwayPlayerState->SelectedHeroId = SelectedHeroId;
		BwayPlayerState->bHeroLocked = bHeroLocked;
		BwayPlayerState->PlayerNum = PlayerNum;
	}
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

	if (SelectedHeroId != NewHeroId)
	{
		if (ABwayGameState* BwayGS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
		{
			const int32 TeamIndex = BwayGS->GetPlayerTeam(this);
			if (UBwayHeroSelectionManager* SelectionManager = BwayGS->FindComponentByClass<UBwayHeroSelectionManager>())
			{
				if (TeamIndex >= 0 && !SelectionManager->IsHeroAvailableForTeam(NewHeroId, TeamIndex))
				{
					UE_LOG(LogTemp, Warning, TEXT("BwayPlayerState: Rejecting duplicate hero %s for team %d"), *NewHeroId.ToString(), TeamIndex + 1);
					return;
				}
			}
		}
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

	// Broadcast the locked delegate so the widget can react (e.g., play lock animation)
	if (bHeroLocked && SelectedHeroId.IsValid())
	{
		OnHeroLocked.Broadcast(SelectedHeroId);
	}
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
	DOREPLIFETIME(ABwayPlayerState, Kills);
	DOREPLIFETIME(ABwayPlayerState, Deaths);
	DOREPLIFETIME(ABwayPlayerState, Assists);
	DOREPLIFETIME(ABwayPlayerState, ObjectiveScore);
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

// ========== MATCH STATS ==========

void ABwayPlayerState::AddKill()
{
	if (HasAuthority())
	{
		Kills++;
		OnRep_MatchStats();
		UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s recorded kill (total: %d)"), *GetPlayerName(), Kills);
	}
}

void ABwayPlayerState::AddDeath()
{
	if (HasAuthority())
	{
		Deaths++;
		OnRep_MatchStats();
		UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s recorded death (total: %d)"), *GetPlayerName(), Deaths);
	}
}

void ABwayPlayerState::AddAssist()
{
	if (HasAuthority())
	{
		Assists++;
		OnRep_MatchStats();
		UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s recorded assist (total: %d)"), *GetPlayerName(), Assists);
	}
}

void ABwayPlayerState::AddObjectiveScore(int32 Amount)
{
	if (HasAuthority())
	{
		ObjectiveScore += Amount;
		OnRep_MatchStats();
		UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s recorded objective score +%d (total: %d)"), *GetPlayerName(), Amount, ObjectiveScore);
	}
}

void ABwayPlayerState::ResetMatchStats()
{
	if (HasAuthority())
	{
		Kills = 0;
		Deaths = 0;
		Assists = 0;
		ObjectiveScore = 0;
		OnRep_MatchStats();
		UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s match stats reset"), *GetPlayerName());
	}
}

void ABwayPlayerState::OnRep_MatchStats()
{
	OnMatchStatsChanged.Broadcast();
}
