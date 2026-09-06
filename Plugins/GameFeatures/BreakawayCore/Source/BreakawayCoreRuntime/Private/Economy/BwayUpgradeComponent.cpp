#include "Economy/BwayUpgradeComponent.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayEconomyLibrary.h"
#include "Economy/BwayGoldAttributeSet.h"
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

bool UBwayUpgradeComponent::IsPurchaseWindowOpen() const
{
	const ABwayGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	const UBwayRoundManagementComponent* Rounds = GS ? GS->GetRoundManagement() : nullptr;
	if (!Rounds) return false;
	const EBwayMatchPhase Phase = Rounds->GetCurrentMatchPhase();
	if (Phase == EBwayMatchPhase::Prematch || Phase == EBwayMatchPhase::Warmup) return true;
	if (Phase == EBwayMatchPhase::PostRound) return !Rounds->CheckMatchEnd();
	if (Phase != EBwayMatchPhase::Playing) return false;
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner(), true);
	const ULyraHealthSet* Health = ASC ? ASC->GetSet<ULyraHealthSet>() : nullptr;
	// Base-healing access will be added with the authoritative base-zone system.
	// Pawn teardown clears death tags, but the PlayerState ASC retains zero health
	// until the replacement pawn initializes it. Keep the shop open in that gap.
	return ASC && (ASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death)
		|| (Health && FMath::IsFinite(Health->GetHealth()) && Health->GetHealth() <= 0.0f));
}

void UBwayUpgradeComponent::ServerPurchaseUpgrade_Implementation(FName Id, int32 ExpectedCurrentRank)
{
	TryPurchaseUpgrade(Id, ExpectedCurrentRank);
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
	if (!Definition || ExpectedCurrentRank != CurrentRank || !Definition->Ranks.IsValidIndex(CurrentRank)
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
	GetOwner()->ForceNetUpdate();
	OnRep_OwnedUpgrades();
}

void UBwayUpgradeComponent::OnRep_OwnedUpgrades()
{
	OnUpgradesChanged.Broadcast();
}
