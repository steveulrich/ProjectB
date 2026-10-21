#include "Economy/BwayUpgradeComponent.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayEconomyLibrary.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayBaseZone.h"
#include "EngineUtils.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BwayGameState.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "LyraGameplayTags.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayUpgradeComponent)

UBwayUpgradeComponent::UBwayUpgradeComponent()
{
	SetIsReplicatedByDefault(true);
}

void UBwayUpgradeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UBwayUpgradeComponent, Catalog, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UBwayUpgradeComponent, OwnedUpgrades, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UBwayUpgradeComponent, ReplicatedUndoCount, COND_OwnerOnly);
}

void UBwayUpgradeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority() && !Catalog)
	{
		// BreakawayCore is always cooked by the project packaging settings. The
		// selected asset remains a hard reference on this replicated component.
		ConfigureCatalog(LoadObject<UBwayUpgradeCatalog>(nullptr,
			TEXT("/BreakawayCore/Economy/DA_BW_MatchUpgrades.DA_BW_MatchUpgrades")));
	}
}

bool UBwayUpgradeComponent::ConfigureCatalog(UBwayUpgradeCatalog* InCatalog)
{
	FString Error;
	if (!GetOwner() || !GetOwner()->HasAuthority() || bTransactionInProgress
		|| !OwnedUpgrades.IsEmpty() || !IsValid(InCatalog) || !InCatalog->ValidateCatalog(Error))
	{
		return false;
	}
	Catalog = InCatalog;
	GetOwner()->ForceNetUpdate();
	return true;
}

int32 UBwayUpgradeComponent::GetOwnedRank(FName Id) const
{
	const FBwayOwnedUpgrade* Owned = OwnedUpgrades.FindByPredicate(
		[Id](const FBwayOwnedUpgrade& Entry) { return Entry.Id == Id; });
	return Owned ? Owned->Rank : 0;
}

void UBwayUpgradeComponent::SetInHealingZone(bool bInZone, int32 ZoneTeamIndex)
{
	const APlayerState* Player = Cast<APlayerState>(GetOwner());
	if (!Player || Player->IsInactive())
	{
		bExplicitInHealingZone = false;
		ExplicitHealingZoneDepth = 0;
		ExplicitHealingZoneTeamIndex = -1;
		ExplicitHealingZonePawn.Reset();
		ExplicitHealingZonePawnLocation = FVector(FLT_MAX);
		OnUpgradesChanged.Broadcast();
		return;
	}

	const ABwayGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	const int32 PlayerTeam = GS ? GS->GetPlayerTeam(Player) : INDEX_NONE;
	const bool bIsEnemyZone = (ZoneTeamIndex != INDEX_NONE && PlayerTeam != INDEX_NONE && PlayerTeam != ZoneTeamIndex);
	if (bIsEnemyZone)
	{
		// Opposing team base healing zones never grant healing or shop eligibility,
		// and entering/exiting an overlapping enemy volume must not clobber an active friendly zone.
		if (!bExplicitInHealingZone || ExplicitHealingZoneDepth <= 0
			|| (ExplicitHealingZoneTeamIndex != INDEX_NONE && ExplicitHealingZoneTeamIndex != PlayerTeam))
		{
			bExplicitInHealingZone = false;
			ExplicitHealingZoneDepth = 0;
			ExplicitHealingZoneTeamIndex = -1;
			ExplicitHealingZonePawn.Reset();
			ExplicitHealingZonePawnLocation = FVector(FLT_MAX);
		}
		OnUpgradesChanged.Broadcast();
		return;
	}

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player, true);
	const ULyraHealthSet* Health = ASC ? ASC->GetSet<ULyraHealthSet>() : nullptr;
	const bool bIsDead = ASC && (ASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death)
		|| ASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death_Dying)
		|| (Health && (!FMath::IsFinite(Health->GetHealth()) || Health->GetHealth() <= 0.0f)));
	const APawn* Pawn = ASC ? Cast<APawn>(ASC->GetAvatarActor()) : Player->GetPawn();

	if (!bInZone || bIsDead)
	{
		if (!bIsDead && ZoneTeamIndex != INDEX_NONE && bExplicitInHealingZone && ExplicitHealingZoneDepth > 1
			&& (!ExplicitHealingZonePawn.IsValid() || ExplicitHealingZonePawn.Get() == Pawn))
		{
			--ExplicitHealingZoneDepth;
			ExplicitHealingZonePawnLocation = IsValid(Pawn) ? Pawn->GetActorLocation() : FVector(FLT_MAX);
			OnUpgradesChanged.Broadcast();
			return;
		}

		bExplicitInHealingZone = false;
		ExplicitHealingZoneDepth = 0;
		ExplicitHealingZoneTeamIndex = -1;
		ExplicitHealingZonePawn.Reset();
		ExplicitHealingZonePawnLocation = FVector(FLT_MAX);
		OnUpgradesChanged.Broadcast();
		return;
	}

	if (ExplicitHealingZonePawn.Get() != Pawn)
	{
		ExplicitHealingZoneDepth = 0;
	}
	ExplicitHealingZoneDepth = (ZoneTeamIndex != INDEX_NONE) ? (FMath::Max(0, ExplicitHealingZoneDepth) + 1) : 1;
	bExplicitInHealingZone = true;
	ExplicitHealingZoneTeamIndex = (ZoneTeamIndex != INDEX_NONE) ? ZoneTeamIndex : PlayerTeam;
	ExplicitHealingZonePawn = Pawn;
	ExplicitHealingZonePawnLocation = IsValid(Pawn) ? Pawn->GetActorLocation() : FVector(FLT_MAX);
	OnUpgradesChanged.Broadcast();
}

bool UBwayUpgradeComponent::IsPlayerInHealingZone() const
{
	const APlayerState* Player = Cast<APlayerState>(GetOwner());
	if (!IsValid(Player) || Player->IsInactive())
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player, true);
	const ULyraHealthSet* Health = ASC ? ASC->GetSet<ULyraHealthSet>() : nullptr;
	if (ASC && (ASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death)
		|| ASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death_Dying)
		|| (Health && (!FMath::IsFinite(Health->GetHealth()) || Health->GetHealth() <= 0.0f))))
	{
		return false;
	}

	if (GetWorld())
	{
		for (TActorIterator<ABwayBaseZone> Base(GetWorld()); Base; ++Base)
		{
			if (Base->CanUseBase(Player))
			{
				return true;
			}
		}
	}

	if (bExplicitInHealingZone)
	{
		const ABwayGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
		const int32 PlayerTeam = GS ? GS->GetPlayerTeam(Player) : INDEX_NONE;
		if (ExplicitHealingZoneTeamIndex != INDEX_NONE && PlayerTeam != INDEX_NONE && PlayerTeam != ExplicitHealingZoneTeamIndex)
		{
			return false;
		}
		const APawn* Pawn = ASC ? Cast<APawn>(ASC->GetAvatarActor()) : Player->GetPawn();
		if (ExplicitHealingZonePawn.IsValid() && Pawn != ExplicitHealingZonePawn.Get())
		{
			return false;
		}
		if (!IsValid(Pawn) && !ExplicitHealingZonePawnLocation.Equals(FVector(FLT_MAX), 1.0f))
		{
			return false;
		}
		if (IsValid(Pawn) && !ExplicitHealingZonePawnLocation.Equals(FVector(FLT_MAX), 1.0f)
			&& !Pawn->GetActorLocation().Equals(ExplicitHealingZonePawnLocation, 5.0f))
		{
			return false;
		}
		return true;
	}

	return false;
}

bool UBwayUpgradeComponent::IsPurchaseWindowOpen() const
{
	const APlayerState* Player = Cast<APlayerState>(GetOwner());
	if (!IsValid(Player) || Player->IsInactive()) return false;
	const ABwayGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	const UBwayRoundManagementComponent* Rounds = GS ? GS->GetRoundManagement() : nullptr;
	if (!Rounds) return false;
	const EBwayMatchPhase Phase = Rounds->GetCurrentMatchPhase();
	if (Phase == EBwayMatchPhase::Prematch || Phase == EBwayMatchPhase::Warmup) return true;
	if (Phase == EBwayMatchPhase::PostRound) return !Rounds->CheckMatchEnd();
	if (Phase != EBwayMatchPhase::Playing) return false;
	if (IsPlayerInHealingZone()) return true;
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner(), true);
	const ULyraHealthSet* Health = ASC ? ASC->GetSet<ULyraHealthSet>() : nullptr;
	// Pawn teardown clears death tags, but the PlayerState ASC retains zero health
	// until the replacement pawn initializes it. Keep the shop open in that gap.
	return ASC && (ASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death)
		|| ASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death_Dying)
		|| (Health && FMath::IsFinite(Health->GetHealth()) && Health->GetHealth() <= 0.0f));
}

void UBwayUpgradeComponent::ServerPurchaseUpgrade_Implementation(FName Id, int32 ExpectedCurrentRank)
{
	ClientPurchaseResult(Id, TryPurchaseUpgrade(Id, ExpectedCurrentRank));
}

void UBwayUpgradeComponent::ServerTryPurchaseUpgrade_Implementation(FName Id, int32 ExpectedCurrentRank)
{
	const int32 EffectiveRank = (ExpectedCurrentRank >= 0) ? ExpectedCurrentRank : GetOwnedRank(Id);
	ClientPurchaseResult(Id, TryPurchaseUpgrade(Id, EffectiveRank));
}

void UBwayUpgradeComponent::ClientPurchaseResult_Implementation(FName Id, bool bSuccess)
{
	OnPurchaseResult.Broadcast(Id, bSuccess);
}

bool UBwayUpgradeComponent::TryPurchaseUpgrade(FName Id, int32 ExpectedCurrentRank)
{
	APlayerState* Player = Cast<APlayerState>(GetOwner());
	FString Error;
	if (!Player || !Player->HasAuthority() || bTransactionInProgress || !IsPurchaseWindowOpen()
		|| !IsValid(Catalog) || !Catalog->ValidateCatalog(Error)) return false;
	TGuardValue<bool> TransactionGuard(bTransactionInProgress, true);
	const FBwayUpgradeDefinition* Definition = Catalog->FindUpgrade(Id);
	const int32 CurrentRank = GetOwnedRank(Id);
	const int32 EffectiveExpectedRank = (ExpectedCurrentRank >= 0) ? ExpectedCurrentRank : CurrentRank;
	if (!Definition || EffectiveExpectedRank != CurrentRank || !Definition->Ranks.IsValidIndex(CurrentRank)
		|| (CurrentRank == 0 && OwnedUpgrades.Num() >= Catalog->MaxOwnedUpgrades)) return false;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player, true);
	const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	const FBwayUpgradeRank Rank = Definition->Ranks[CurrentRank];
	if (!Gold || !FMath::IsFinite(Gold->GetCurrentGold()) || Gold->GetCurrentGold() < static_cast<double>(Rank.Cost)) return false;
	const UBwayUpgradeEffect* Effect = Definition->EffectClass.GetDefaultObject();
	for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
	{
		if (!ASC->HasAttributeSetForAttribute(Modifier.Attribute)) return false;
	}
	FGameplayEffectSpec Spec(Effect, ASC->MakeEffectContext(), 1.0f);
	Spec.SetSetByCallerMagnitude(UBwayUpgradeEffect::MagnitudeName, Rank.Magnitude);
	const FActiveGameplayEffectHandle NewHandle = ASC->ApplyGameplayEffectSpecToSelf(Spec);
	if (!NewHandle.IsValid()) return false;
	if (!UBwayEconomyLibrary::TrySpendGold(Player, Rank.Cost))
	{
		ASC->RemoveActiveGameplayEffect(NewHandle);
		return false;
	}
	if (const FActiveGameplayEffectHandle* OldHandle = EffectHandles.Find(Id))
	{
		ASC->RemoveActiveGameplayEffect(*OldHandle);
	}
	EffectHandles.Add(Id, NewHandle);
	FBwayOwnedUpgrade* Owned = OwnedUpgrades.FindByPredicate([Id](const FBwayOwnedUpgrade& Entry) { return Entry.Id == Id; });
	if (!Owned) Owned = &OwnedUpgrades.AddDefaulted_GetRef();
	Owned->Id = Id;
	Owned->Rank = CurrentRank + 1;

	FBwayUpgradeTransactionRecord& Record = TransactionHistory.AddDefaulted_GetRef();
	Record.Id = Id;
	Record.PreviousRank = CurrentRank;
	Record.NewRank = CurrentRank + 1;
	Record.GoldCost = Rank.Cost;
	Record.bWasPurchase = true;
	ReplicatedUndoCount = TransactionHistory.Num();

	if (ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>())
	{
		// Read only the committed modifier: rank replacement briefly has both
		// effects, and a rejected debit must not shorten an existing death timer.
		if (UBwayRoundManagementComponent* Rounds = GS->GetRoundManagement()) Rounds->RefreshRespawnSchedule(Player);
	}
	Player->ForceNetUpdate();
	OnRep_OwnedUpgrades();
	return true;
}

bool UBwayUpgradeComponent::ApplyOwnedRankInternal(
	FName Id,
	int32 TargetRank,
	UAbilitySystemComponent* ASC,
	const FBwayUpgradeDefinition* Definition)
{
	if (!ASC || !Definition)
	{
		return false;
	}

	if (TargetRank <= 0)
	{
		if (const FActiveGameplayEffectHandle* OldHandle = EffectHandles.Find(Id))
		{
			ASC->RemoveActiveGameplayEffect(*OldHandle);
			EffectHandles.Remove(Id);
		}
		OwnedUpgrades.RemoveAll([Id](const FBwayOwnedUpgrade& Entry) { return Entry.Id == Id; });
		return true;
	}

	if (!Definition->Ranks.IsValidIndex(TargetRank - 1))
	{
		return false;
	}

	const UBwayUpgradeEffect* Effect = Definition->EffectClass.GetDefaultObject();
	for (const FGameplayModifierInfo& Modifier : Effect->Modifiers)
	{
		if (!ASC->HasAttributeSetForAttribute(Modifier.Attribute))
		{
			return false;
		}
	}

	FGameplayEffectSpec Spec(Effect, ASC->MakeEffectContext(), 1.0f);
	Spec.SetSetByCallerMagnitude(UBwayUpgradeEffect::MagnitudeName, Definition->Ranks[TargetRank - 1].Magnitude);
	const FActiveGameplayEffectHandle NewHandle = ASC->ApplyGameplayEffectSpecToSelf(Spec);
	if (!NewHandle.IsValid())
	{
		return false;
	}

	if (const FActiveGameplayEffectHandle* OldHandle = EffectHandles.Find(Id))
	{
		ASC->RemoveActiveGameplayEffect(*OldHandle);
	}
	EffectHandles.Add(Id, NewHandle);

	FBwayOwnedUpgrade* Owned = OwnedUpgrades.FindByPredicate([Id](const FBwayOwnedUpgrade& Entry) { return Entry.Id == Id; });
	if (!Owned)
	{
		Owned = &OwnedUpgrades.AddDefaulted_GetRef();
		Owned->Id = Id;
	}
	Owned->Rank = TargetRank;
	return true;
}

bool UBwayUpgradeComponent::CanSellUpgrade(FName Id) const
{
	return IsPurchaseWindowOpen() && GetOwnedRank(Id) > 0 && IsValid(Catalog) && Catalog->FindUpgrade(Id) != nullptr;
}

bool UBwayUpgradeComponent::CanUndoLastTransaction() const
{
	return IsPurchaseWindowOpen() && ReplicatedUndoCount > 0;
}

void UBwayUpgradeComponent::ServerSellUpgrade_Implementation(FName Id)
{
	ClientPurchaseResult(Id, TrySellUpgrade(Id));
}

bool UBwayUpgradeComponent::TrySellUpgrade(FName Id)
{
	APlayerState* Player = Cast<APlayerState>(GetOwner());
	FString Error;
	if (!Player || !Player->HasAuthority() || bTransactionInProgress || !IsPurchaseWindowOpen()
		|| !IsValid(Catalog) || !Catalog->ValidateCatalog(Error))
	{
		return false;
	}

	TGuardValue<bool> TransactionGuard(bTransactionInProgress, true);
	const FBwayUpgradeDefinition* Definition = Catalog->FindUpgrade(Id);
	const int32 CurrentRank = GetOwnedRank(Id);
	if (!Definition || CurrentRank <= 0 || !Definition->Ranks.IsValidIndex(CurrentRank - 1))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player, true);
	if (!ASC)
	{
		return false;
	}

	const int32 RefundAmount = Definition->Ranks[CurrentRank - 1].Cost;
	const int32 NewRank = CurrentRank - 1;
	if (!ApplyOwnedRankInternal(Id, NewRank, ASC, Definition))
	{
		return false;
	}

	UBwayEconomyLibrary::TryRefundGold(Player, RefundAmount);

	FBwayUpgradeTransactionRecord& Record = TransactionHistory.AddDefaulted_GetRef();
	Record.Id = Id;
	Record.PreviousRank = CurrentRank;
	Record.NewRank = NewRank;
	Record.GoldCost = RefundAmount;
	Record.bWasPurchase = false;
	ReplicatedUndoCount = TransactionHistory.Num();

	if (ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>())
	{
		if (UBwayRoundManagementComponent* Rounds = GS->GetRoundManagement())
		{
			Rounds->RefreshRespawnSchedule(Player);
		}
	}
	Player->ForceNetUpdate();
	OnRep_OwnedUpgrades();
	return true;
}

void UBwayUpgradeComponent::ServerUndoLastTransaction_Implementation()
{
	const FName LastId = !TransactionHistory.IsEmpty() ? TransactionHistory.Last().Id : NAME_None;
	ClientPurchaseResult(LastId, TryUndoLastTransaction());
}

bool UBwayUpgradeComponent::TryUndoLastTransaction()
{
	APlayerState* Player = Cast<APlayerState>(GetOwner());
	FString Error;
	if (!Player || !Player->HasAuthority() || bTransactionInProgress || !IsPurchaseWindowOpen()
		|| TransactionHistory.IsEmpty() || !IsValid(Catalog) || !Catalog->ValidateCatalog(Error))
	{
		return false;
	}

	TGuardValue<bool> TransactionGuard(bTransactionInProgress, true);
	const FBwayUpgradeTransactionRecord Record = TransactionHistory.Last();
	const FBwayUpgradeDefinition* Definition = Catalog->FindUpgrade(Record.Id);
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player, true);
	if (!Definition || !ASC || GetOwnedRank(Record.Id) != Record.NewRank)
	{
		return false;
	}

	if (Record.bWasPurchase)
	{
		// Undo a purchase: revert rank to PreviousRank and refund GoldCost
		if (!ApplyOwnedRankInternal(Record.Id, Record.PreviousRank, ASC, Definition))
		{
			return false;
		}
		UBwayEconomyLibrary::TryRefundGold(Player, Record.GoldCost);
	}
	else
	{
		// Undo a sell: spend GoldCost back and restore rank to PreviousRank
		if (Record.NewRank == 0 && OwnedUpgrades.Num() >= Catalog->MaxOwnedUpgrades)
		{
			return false;
		}
		if (!UBwayEconomyLibrary::TrySpendGold(Player, Record.GoldCost))
		{
			return false;
		}
		if (!ApplyOwnedRankInternal(Record.Id, Record.PreviousRank, ASC, Definition))
		{
			UBwayEconomyLibrary::TryRefundGold(Player, Record.GoldCost);
			return false;
		}
	}

	TransactionHistory.Pop();
	ReplicatedUndoCount = TransactionHistory.Num();

	if (ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>())
	{
		if (UBwayRoundManagementComponent* Rounds = GS->GetRoundManagement())
		{
			Rounds->RefreshRespawnSchedule(Player);
		}
	}
	Player->ForceNetUpdate();
	OnRep_OwnedUpgrades();
	return true;
}

void UBwayUpgradeComponent::ResetUpgrades()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bTransactionInProgress) return;
	TGuardValue<bool> TransactionGuard(bTransactionInProgress, true);
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner(), true))
	{
		for (const auto& Entry : EffectHandles) ASC->RemoveActiveGameplayEffect(Entry.Value);
	}
	EffectHandles.Reset();
	OwnedUpgrades.Reset();
	TransactionHistory.Reset();
	ReplicatedUndoCount = 0;
	if (ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>())
	{
		if (UBwayRoundManagementComponent* Rounds = GS->GetRoundManagement()) Rounds->RefreshRespawnSchedule(Cast<APlayerState>(GetOwner()));
	}
	GetOwner()->ForceNetUpdate();
	OnRep_OwnedUpgrades();
}

void UBwayUpgradeComponent::OnRep_OwnedUpgrades()
{
	OnUpgradesChanged.Broadcast();
}
