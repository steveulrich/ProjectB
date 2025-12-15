// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/BwayGameplayAbility_DefensiveStance.h"
#include "AbilitySystemComponent.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_State_DefensiveStance, "State.DefensiveStance");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Spartacus_DefensiveStance, "Ability.Spartacus.DefensiveStance");

UBwayGameplayAbility_DefensiveStance::UBwayGameplayAbility_DefensiveStance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

	AbilityTags.AddTag(TAG_Ability_Spartacus_DefensiveStance);
	DefensiveStanceStateTag = TAG_State_DefensiveStance;
}

bool UBwayGameplayAbility_DefensiveStance::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	return true;
}

void UBwayGameplayAbility_DefensiveStance::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Store the handle for use in callbacks
	StoredSpecHandle = Handle;

	// Cache character from base class
	CachedCharacter = GetBwayCharacterFromActorInfo();
	if (!CachedCharacter)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Toggle stance
	ToggleStance();
}

void UBwayGameplayAbility_DefensiveStance::ToggleStance()
{
	if (bStanceActive)
	{
		// Deactivate stance
		RemoveStanceEffect();
		bStanceActive = false;
		EndAbility(StoredSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	else
	{
		// Activate stance
		ApplyStanceEffect();
		bStanceActive = true;
		// Ability stays active while stance is on
	}
}

void UBwayGameplayAbility_DefensiveStance::ApplyStanceEffect()
{
	if (!CachedCharacter || !StanceGameplayEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	// Remove existing effect if any
	if (StanceEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(StanceEffectHandle);
		StanceEffectHandle.Invalidate();
	}

	// Use the ability's MakeEffectContext method which properly sets up the ability handle
	FGameplayEffectContextHandle EffectContextHandle = MakeEffectContext(StoredSpecHandle, CurrentActorInfo);

	// Apply stance effect (infinite duration)
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(StanceGameplayEffectClass, 1.0f, EffectContextHandle);
	if (SpecHandle.IsValid())
	{
		StanceEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void UBwayGameplayAbility_DefensiveStance::RemoveStanceEffect()
{
	if (!StanceEffectHandle.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->RemoveActiveGameplayEffect(StanceEffectHandle);
		StanceEffectHandle.Invalidate();
	}
}

void UBwayGameplayAbility_DefensiveStance::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// If stance was active, remove it
	if (bStanceActive)
	{
		RemoveStanceEffect();
		bStanceActive = false;
	}

	CachedCharacter = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

