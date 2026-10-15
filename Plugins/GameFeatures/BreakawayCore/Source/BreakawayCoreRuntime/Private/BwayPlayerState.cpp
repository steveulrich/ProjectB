// Fill out your copyright notice in the Description page of Project Settings.

#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Combat/BwayLifeLeechComponent.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "Net/UnrealNetwork.h"

ABwayPlayerState::ABwayPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UpgradeComponent = CreateDefaultSubobject<UBwayUpgradeComponent>(TEXT("UpgradeComponent"));
	LifeLeechComponent = CreateDefaultSubobject<UBwayLifeLeechComponent>(TEXT("LifeLeechComponent"));
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
			ASC->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute())
				.AddUObject(this, &ThisClass::HandleGoldChanged);
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
		BwayPlayerState->SetGenericTeamId(GetGenericTeamId());
		BwayPlayerState->bHasCarriedTeamAssignment = true;
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

void ABwayPlayerState::ApplyRelicCarrierTag(bool bCarrier, EGameplayTagReplicationState ReplicationState)
{
	ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const FGameplayTag RelicCarrierTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.State.RelicCarrier"), /*ErrorIfNotFound*/ false);
	if (!RelicCarrierTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPlayerState: Gameplay.State.RelicCarrier is not registered — carrier ability blocking will not work."));
		return;
	}

	ASC->SetLooseGameplayTagCount(RelicCarrierTag, bCarrier ? 1 : 0, ReplicationState);
	if (BwayGameplayTags::State_Relic_Carrying.GetTag().IsValid())
	{
		ASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_Carrying, bCarrier ? 1 : 0, ReplicationState);
	}
}

void ABwayPlayerState::OnRep_HasRelic()
{
	if (!HasAuthority())
	{
		// Mirror replicated carrier state onto the client ASC immediately so LocalPredicted
		// combat abilities fail CanActivateAbility without waiting for loose-tag replication.
		ApplyRelicCarrierTag(bHasRelic, EGameplayTagReplicationState::None);
	}

	OnRelicPossessionChanged.Broadcast(bHasRelic);

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
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s has called for relic: %s"), 
		*GetName(), bHasCalledForRelic ? TEXT("true") : TEXT("false"));
	
	// Blueprint events can bind here to update marker visibility on player mesh
}

// ========== HERO SELECTION SYSTEM ==========

void ABwayPlayerState::ServerSetSelectedHeroId_Implementation(FPrimaryAssetId NewHeroId)
{
	TrySetSelectedHeroId(NewHeroId);
}

bool ABwayPlayerState::TrySetSelectedHeroId(FPrimaryAssetId NewHeroId)
{
	if (!HasAuthority()) return false;
	// Don't allow changes if hero is already locked
	if (bHeroLocked)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPlayerState: Cannot change hero for %s - selection is locked"), *GetName());
		return false;
	}

	// Validate the hero ID is not null
	if (!NewHeroId.IsValid() || !UBwayHeroRegistry::GetHeroDataById(NewHeroId))
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayPlayerState: Attempted to set invalid hero ID for %s"), *GetName());
		return false;
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
					return false;
				}
			}
		}
	}

	SelectedHeroId = NewHeroId;
	OnRep_SelectedHeroId();
	
	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s selected hero: %s"), *GetName(), *SelectedHeroId.ToString());
	return true;
}

void ABwayPlayerState::OnRep_SelectedHeroId()
{
	// Broadcast to any listeners (UI, game systems, etc.)
	OnSelectedHeroChanged.Broadcast(SelectedHeroId);
	
	UE_LOG(LogTemp, Verbose, TEXT("BwayPlayerState: Selected hero replicated to %s"), *SelectedHeroId.ToString());
}

void ABwayPlayerState::ServerLockHeroSelection_Implementation()
{
	if (!HasAuthority() || bHeroLocked) return;
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
	DOREPLIFETIME(ABwayPlayerState, DamageDealt);
	DOREPLIFETIME(ABwayPlayerState, HealingDone);
	DOREPLIFETIME(ABwayPlayerState, ForcedFumbles);
	DOREPLIFETIME(ABwayPlayerState, Interceptions);
	DOREPLIFETIME(ABwayPlayerState, BuildablesDestroyed);
	DOREPLIFETIME(ABwayPlayerState, MatchGoldEarned);
	DOREPLIFETIME(ABwayPlayerState, LastRoundStats);
	DOREPLIFETIME(ABwayPlayerState, RespawnSchedule);
	DOREPLIFETIME(ABwayPlayerState, DeathsInCurrentRound);
	DOREPLIFETIME_CONDITION(ABwayPlayerState, bHasPlacedBuildableThisRound, COND_OwnerOnly);
}

void ABwayPlayerState::SetDeathsInCurrentRound(int32 NewCount)
{
	if (!HasAuthority()) return;
	const int32 ClampedCount = FMath::Max(0, NewCount);
	if (DeathsInCurrentRound == ClampedCount) return;
	DeathsInCurrentRound = ClampedCount;
	ForceNetUpdate();
}

int32 ABwayPlayerState::IncrementDeathsInCurrentRound()
{
	if (!HasAuthority()) return DeathsInCurrentRound;
	DeathsInCurrentRound = FMath::Max(0, DeathsInCurrentRound) + 1;
	ForceNetUpdate();
	return DeathsInCurrentRound;
}

void ABwayPlayerState::SetRespawnSchedule(double ServerEndTime, int32 RoundNumber)
{
	if (!HasAuthority()) return;
	const double EndTime = FMath::IsFinite(ServerEndTime) && ServerEndTime > 0.0 ? ServerEndTime : 0.0;
	const int32 ScheduledRound = EndTime > 0.0 ? RoundNumber : 0;
	if (RespawnSchedule.ServerEndTime == EndTime && RespawnSchedule.RoundNumber == ScheduledRound) return;
	RespawnSchedule.ServerEndTime = EndTime;
	RespawnSchedule.RoundNumber = ScheduledRound;
	OnRep_RespawnSchedule();
	ForceNetUpdate();
}

void ABwayPlayerState::OnRep_RespawnSchedule()
{
	OnRespawnScheduleChanged.Broadcast();
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

void ABwayPlayerState::AddDamageDealt(int32 Amount)
{
	if (HasAuthority() && Amount > 0)
	{
		DamageDealt += Amount;
		OnRep_MatchStats();
	}
}

void ABwayPlayerState::AddHealingDone(int32 Amount)
{
	if (HasAuthority() && Amount > 0)
	{
		HealingDone += Amount;
		OnRep_MatchStats();
	}
}

void ABwayPlayerState::AddForcedFumble()
{
	if (HasAuthority())
	{
		ForcedFumbles++;
		OnRep_MatchStats();
		UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s recorded forced fumble (total: %d)"), *GetPlayerName(), ForcedFumbles);
	}
}

void ABwayPlayerState::AddInterception()
{
	if (HasAuthority())
	{
		Interceptions++;
		OnRep_MatchStats();
		UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s recorded interception (total: %d)"), *GetPlayerName(), Interceptions);
	}
}

void ABwayPlayerState::SetInHealingZone(bool bInZone, int32 ZoneTeamIndex)
{
	if (UpgradeComponent)
	{
		UpgradeComponent->SetInHealingZone(bInZone, ZoneTeamIndex);
	}
}

bool ABwayPlayerState::IsPlayerInHealingZone() const
{
	return UpgradeComponent ? UpgradeComponent->IsPlayerInHealingZone() : false;
}

void ABwayPlayerState::ResetMatchStats()
{
	if (HasAuthority())
	{
		SetRespawnSchedule(0.0, 0);
		DeathsInCurrentRound = 0;
		Kills = 0;
		Deaths = 0;
		Assists = 0;
		ObjectiveScore = 0;
		DamageDealt = 0;
		HealingDone = 0;
		ForcedFumbles = 0;
		Interceptions = 0;
		BuildablesDestroyed = 0;
		LastRoundStats.Reset();
		RoundStartStatsBaseline.Reset();
		MatchGoldEarned = 0.0;
		OnRep_MatchStats();
		OnRep_LastRoundStats();
		UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s match stats reset"), *GetPlayerName());
	}
}

FBwayPlayerMatchStats ABwayPlayerState::GetMatchStatsSnapshot() const
{
	return BuildCurrentStatSnapshot();
}

void ABwayPlayerState::HandleGoldChanged(const FOnAttributeChangeData& Change)
{
	// Attribute notifications contain the actual clamped wallet change, so
	// overflow at MaxGold and failed/rejected awards do not inflate earnings.
	if (HasAuthority() && FMath::IsFinite(Change.NewValue) && FMath::IsFinite(Change.OldValue)
		&& Change.NewValue > Change.OldValue)
	{
		MatchGoldEarned = FMath::Min(static_cast<double>(MAX_int32),
			MatchGoldEarned + static_cast<double>(Change.NewValue) - Change.OldValue);
		OnRep_MatchStats();
	}
}

FBwayPlayerMatchStats ABwayPlayerState::BuildCurrentStatSnapshot() const
{
	FBwayPlayerMatchStats Snapshot;
	Snapshot.Kills = Kills;
	Snapshot.Deaths = Deaths;
	Snapshot.Assists = Assists;
	Snapshot.GoldEarned = FMath::RoundToInt(MatchGoldEarned);
	Snapshot.DamageDealt = DamageDealt;
	Snapshot.HealingDone = HealingDone;
	Snapshot.RelicScores = ObjectiveScore;
	Snapshot.ForcedFumbles = ForcedFumbles;
	Snapshot.Interceptions = Interceptions;
	Snapshot.BuildablesDestroyed = BuildablesDestroyed;
	return Snapshot;
}

void ABwayPlayerState::MarkBuildablePlacedThisRound()
{
	if (!HasAuthority() || bHasPlacedBuildableThisRound)
	{
		return;
	}

	bHasPlacedBuildableThisRound = true;
	OnRep_HasPlacedBuildableThisRound();
	ForceNetUpdate();
}

void ABwayPlayerState::BeginRoundStatTracking()
{
	if (!HasAuthority())
	{
		return;
	}

	RoundStartStatsBaseline = BuildCurrentStatSnapshot();
	SetRespawnSchedule(0.0, 0);
	if (bHasPlacedBuildableThisRound)
	{
		bHasPlacedBuildableThisRound = false;
		OnRep_HasPlacedBuildableThisRound();
		ForceNetUpdate();
	}
	LastRoundStats.Reset();
	OnRep_LastRoundStats();

	UE_LOG(LogTemp, Verbose, TEXT("BwayPlayerState: %s began round stat tracking (gold baseline=%d)"),
		*GetPlayerName(), RoundStartStatsBaseline.GoldEarned);
}

void ABwayPlayerState::FinalizeRoundStats()
{
	if (!HasAuthority())
	{
		return;
	}

	FBwayPlayerMatchStats CurrentSnapshot = BuildCurrentStatSnapshot();
	LastRoundStats = FBwayPlayerMatchStats::Diff(CurrentSnapshot, RoundStartStatsBaseline);
	OnRep_LastRoundStats();

	UE_LOG(LogTemp, Log, TEXT("BwayPlayerState: %s round stats finalized — K/D/A %d/%d/%d Gold=%d"),
		*GetPlayerName(),
		LastRoundStats.Kills,
		LastRoundStats.Deaths,
		LastRoundStats.Assists,
		LastRoundStats.GoldEarned);
}

void ABwayPlayerState::OnRep_MatchStats()
{
	OnMatchStatsChanged.Broadcast();
}

void ABwayPlayerState::OnRep_HasPlacedBuildableThisRound()
{
	OnBuildablePlacedThisRoundChanged.Broadcast(bHasPlacedBuildableThisRound);
}

void ABwayPlayerState::OnRep_LastRoundStats()
{
	// Reserved for UI refresh hooks (PostRound summary).
}
