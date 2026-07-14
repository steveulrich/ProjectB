#include "Abilities/BwayGameplayAbility_AlonaRadiance.h"

#include "Abilities/BwayGameplayEffect_AlonaCooldowns.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_AlonaRadiance)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Alona_Radiance, "Ability.Alona.Radiance");

UBwayGameplayAbility_AlonaRadiance::UBwayGameplayAbility_AlonaRadiance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Alona_Radiance);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CooldownGameplayEffectClass = UGE_Bway_Cooldown_AlonaRadiance::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Alona", "RadianceName", "Radiance");
	DisplayData.Description = NSLOCTEXT("Alona", "RadianceDesc", "Heal the ally closest to the reticule for a short duration.");
}

void UBwayGameplayAbility_AlonaRadiance::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	StoredSpecHandle = Handle;
	CachedCharacter = GetBwayCharacterFromActorInfo();
	if (!CachedCharacter)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&ActivationInfo))
	{
		LockedAlly = FindClosestAllyToReticule();
		if (!LockedAlly)
		{
			// Allow self-heal when no ally is in range / near reticule.
			LockedAlly = CachedCharacter;
		}

		ApplyHealTick();

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HealTickTimerHandle);
			World->GetTimerManager().SetTimer(
				HealTickTimerHandle,
				this,
				&UBwayGameplayAbility_AlonaRadiance::ApplyHealTick,
				HealTickInterval,
				true,
				HealTickInterval);
		}
	}

	// Keep predicted client instance alive for the same window so EndAbility does not cancel server heals.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndAbilityTimerHandle);
		World->GetTimerManager().SetTimer(
			EndAbilityTimerHandle,
			[this, Handle, ActorInfo, ActivationInfo]()
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			},
			ActiveDuration,
			false);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

ABwayCharacterWithAbilities* UBwayGameplayAbility_AlonaRadiance::FindClosestAllyToReticule() const
{
	if (!CachedCharacter)
	{
		return nullptr;
	}

	FVector ViewLocation = CachedCharacter->GetActorLocation();
	FVector ViewDirection = CachedCharacter->GetActorForwardVector();
	if (const AController* Controller = CachedCharacter->GetController())
	{
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		ViewDirection = ViewRotation.Vector().GetSafeNormal();
	}

	TArray<ABwayCharacterWithAbilities*> Allies = GetAlliesInRadius(CachedCharacter->GetActorLocation(), MaxTargetRange);

	// Include self as a valid fallback candidate only if already closest.
	ABwayCharacterWithAbilities* BestAlly = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	auto ScoreAlly = [&](ABwayCharacterWithAbilities* Ally)
	{
		if (!Ally)
		{
			return;
		}

		const FVector ToAlly = Ally->GetActorLocation() - ViewLocation;
		const float DistanceAlongRay = FVector::DotProduct(ToAlly, ViewDirection);
		if (DistanceAlongRay < 0.f || DistanceAlongRay > MaxTargetRange)
		{
			return;
		}

		const FVector ClosestPointOnRay = ViewLocation + ViewDirection * DistanceAlongRay;
		const float DistToRay = FVector::Dist(Ally->GetActorLocation(), ClosestPointOnRay);
		if (DistToRay > MaxRayDistance)
		{
			return;
		}

		// Primary score: distance to ray, then distance along ray.
		const float Score = DistToRay * 10.f + DistanceAlongRay * 0.01f;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestAlly = Ally;
		}
	};

	for (ABwayCharacterWithAbilities* Ally : Allies)
	{
		ScoreAlly(Ally);
	}

	return BestAlly;
}

void UBwayGameplayAbility_AlonaRadiance::ApplyHealTick()
{
	if (!HasAuthority(&CurrentActivationInfo) || !IsValid(LockedAlly))
	{
		return;
	}

	ApplyHealToAlly(LockedAlly, HealPerTick);
}

void UBwayGameplayAbility_AlonaRadiance::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealTickTimerHandle);
		World->GetTimerManager().ClearTimer(EndAbilityTimerHandle);
	}

	LockedAlly = nullptr;
	CachedCharacter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
