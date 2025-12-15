// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/BwayGameplayAbility_WarCry.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Spartacus_WarCry, "Ability.Spartacus.WarCry");

UBwayGameplayAbility_WarCry::UBwayGameplayAbility_WarCry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

	AbilityTags.AddTag(TAG_Ability_Spartacus_WarCry);
}

bool UBwayGameplayAbility_WarCry::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
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

void UBwayGameplayAbility_WarCry::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	// Commit cooldown
	if (GetCooldownGameplayEffect())
	{
		CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, true);
	}

	// Get nearby allies
	TArray<ABwayCharacterWithAbilities*> NearbyAllies = GetNearbyAllies(BuffRadius);

	// Apply buff to each ally
	for (ABwayCharacterWithAbilities* Ally : NearbyAllies)
	{
		if (Ally)
		{
			ApplyBuffToAlly(Ally);
		}
	}

	// End ability immediately (instant cast)
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

TArray<ABwayCharacterWithAbilities*> UBwayGameplayAbility_WarCry::GetNearbyAllies(float Radius) const
{
	if (!CachedCharacter)
	{
		return TArray<ABwayCharacterWithAbilities*>();
	}

	// Use base class method
	return GetAlliesInRadius(CachedCharacter->GetActorLocation(), Radius);
}

void UBwayGameplayAbility_WarCry::ApplyBuffToAlly(ABwayCharacterWithAbilities* Ally)
{
	if (!Ally || !BuffGameplayEffectClass)
	{
		return;
	}

	// Use base class method to apply effect
	ApplyGameplayEffectToTarget(Ally, BuffGameplayEffectClass, 1.0f);
}

