#include "Abilities/BwayGameplayAbility_AlonaRadiance.h"

#include "Abilities/BwayGameplayEffect_AlonaCooldowns.h"
#include "AbilitySystem/Attributes/LyraCombatSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_AlonaRadiance)

DEFINE_LOG_CATEGORY_STATIC(LogBwayAlonaRadiance, Log, All);

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

		float AttackStrength = 0.f;
		float HealMultiplier = 0.f;
		ResolvedHealPerTick = CalculateScaledHealPerTick(AttackStrength, HealMultiplier);
		const float SafeTickInterval = FMath::Max(HealTickInterval, KINDA_SMALL_NUMBER);
		RemainingHealTicks = FMath::Max(
			1,
			FMath::FloorToInt((ActiveDuration / SafeTickInterval) + KINDA_SMALL_NUMBER));

		UE_LOG(
			LogBwayAlonaRadiance,
			Log,
			TEXT("RadianceHeal: base=%.4f str=%.2f divisor=%.2f multiplier=%.4f cap=%.2f -> %.4f/tick (%d ticks over %.2fs)"),
			HealPerTick,
			AttackStrength,
			StrengthDivisor,
			HealMultiplier,
			MaxHealMultiplier,
			ResolvedHealPerTick,
			RemainingHealTicks,
			ActiveDuration);

		ApplyHealTick();

		if (RemainingHealTicks > 0)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(HealTickTimerHandle);
				World->GetTimerManager().SetTimer(
					HealTickTimerHandle,
					this,
					&UBwayGameplayAbility_AlonaRadiance::ApplyHealTick,
					SafeTickInterval,
					true,
					SafeTickInterval);
			}
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

float UBwayGameplayAbility_AlonaRadiance::CalculateScaledHealPerTick(
	float& OutAttackStrength,
	float& OutHealMultiplier) const
{
	OutAttackStrength = 0.f;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (const ULyraCombatSet* CombatSet = ASC->GetSet<ULyraCombatSet>())
		{
			OutAttackStrength = CombatSet->GetBaseDamage();
		}
	}

	const float SafeStrengthDivisor = FMath::Max(StrengthDivisor, KINDA_SMALL_NUMBER);
	const float SafeMaxMultiplier = FMath::Max(0.f, MaxHealMultiplier);
	OutHealMultiplier = FMath::Clamp(OutAttackStrength / SafeStrengthDivisor, 0.f, SafeMaxMultiplier);
	return HealPerTick * OutHealMultiplier;
}

void UBwayGameplayAbility_AlonaRadiance::ApplyHealTick()
{
	if (!HasAuthority(&CurrentActivationInfo) || !IsValid(LockedAlly) || RemainingHealTicks <= 0)
	{
		return;
	}

	ApplyHealToAlly(LockedAlly, ResolvedHealPerTick);
	--RemainingHealTicks;

	if (RemainingHealTicks <= 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HealTickTimerHandle);
		}
	}
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
	ResolvedHealPerTick = 0.f;
	RemainingHealTicks = 0;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
