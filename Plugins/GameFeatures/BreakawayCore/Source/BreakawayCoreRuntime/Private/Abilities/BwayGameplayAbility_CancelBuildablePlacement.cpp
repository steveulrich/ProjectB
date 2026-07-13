#include "Abilities/BwayGameplayAbility_CancelBuildablePlacement.h"

#include "AbilitySystemComponent.h"
#include "BwayGameplayTags.h"
#include "GameplayTagContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_CancelBuildablePlacement)

DEFINE_LOG_CATEGORY_STATIC(LogBwayPlaceCancel, Log, All);

UBwayGameplayAbility_CancelBuildablePlacement::UBwayGameplayAbility_CancelBuildablePlacement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;
	bRetriggerInstancedAbility = true;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(BwayGameplayTags::Ability_Buildable_PlacementExempt);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationRequiredTags.AddTag(BwayGameplayTags::State_BuildablePlacement);
	CancelAbilitiesWithTag.Reset();
}

bool UBwayGameplayAbility_CancelBuildablePlacement::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const bool bCanActivate = Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
	if (!bCanActivate)
	{
		FGameplayTagContainer Owned;
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			ActorInfo->AbilitySystemComponent->GetOwnedGameplayTags(Owned);
		}

		FGameplayTagContainer BlockedReason;
		if (OptionalRelevantTags)
		{
			BlockedReason = *OptionalRelevantTags;
		}

		UE_LOG(LogBwayPlaceCancel, Warning,
			TEXT("CancelBuildable CanActivate=FALSE. OwnedTags=[%s] FailTags=[%s] HasStatePlacement=%d"),
			*Owned.ToStringSimple(),
			*BlockedReason.ToStringSimple(),
			Owned.HasTag(BwayGameplayTags::State_BuildablePlacement) ? 1 : 0);
	}

	return bCanActivate;
}

void UBwayGameplayAbility_CancelBuildablePlacement::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogBwayPlaceCancel, Log, TEXT("CancelBuildable ActivateAbility — calling LocalInputCancel"));

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->LocalInputCancel();
	}
	else
	{
		UE_LOG(LogBwayPlaceCancel, Error, TEXT("CancelBuildable ActivateAbility: ASC is null"));
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
