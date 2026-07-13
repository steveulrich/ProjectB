#include "Abilities/BwayGameplayAbility_RelicRequest.h"

#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameplayEffect.h"
#include "Relic/RelicSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_RelicRequest)

DEFINE_LOG_CATEGORY_STATIC(LogBwayRelicRequest, Log, All);

UBwayGameplayAbility_RelicRequest::UBwayGameplayAbility_RelicRequest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// BP may override these — keep LocalPredicted + InstancedPerActor. ServerInitiated breaks Lyra RMB input (BF-014).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

	// Ability identity tags (e.g. Ability.Relic.RequestPickup) are authored on the BP.
	// Do NOT put InputTag.* on AbilityTags — grants carry InputTag via DynamicSpecSourceTags (BF-009).

	ActivationBlockedTags.AddTag(BwayGameplayTags::State_BuildablePlacement);
	CancelAbilitiesWithTag.Reset();
}

const URelicSettings* UBwayGameplayAbility_RelicRequest::ResolveRelicSettings() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	const UBwayRelicManagerComponent* RelicMgr = GameState ? GameState->RelicManagerComponent : nullptr;
	return RelicMgr ? RelicMgr->GetRelicSettings() : nullptr;
}

void UBwayGameplayAbility_RelicRequest::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}

	const URelicSettings* Settings = ResolveRelicSettings();
	const TSubclassOf<UGameplayEffect> RequestEffectClass = Settings ? Settings->RequestingGameplayEffectClass : nullptr;
	if (!RequestEffectClass)
	{
		UE_LOG(LogBwayRelicRequest, Warning,
			TEXT("RelicRequest: no RequestingGameplayEffectClass (RelicSettings=%s). Wire it on the match RelicSettings asset."),
			*GetNameSafe(Settings));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(RequestEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogBwayRelicRequest, Warning, TEXT("RelicRequest: failed to build GE spec for %s"), *GetNameSafe(RequestEffectClass));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FActiveGameplayEffectHandle ActiveHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	UE_LOG(LogBwayRelicRequest, Log, TEXT("RelicRequest applied %s Active=%d"),
		*GetNameSafe(RequestEffectClass),
		ActiveHandle.IsValid() ? 1 : 0);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
