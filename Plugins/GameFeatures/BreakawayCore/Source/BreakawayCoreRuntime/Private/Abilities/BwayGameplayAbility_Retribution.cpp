#include "Abilities/BwayGameplayAbility_Retribution.h"

#include "Abilities/BwayGameplayEffect_ArgusCooldowns.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_Retribution)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Argus_Retribution, "Ability.Argus.Retribution");

UBwayGameplayAbility_Retribution::UBwayGameplayAbility_Retribution(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Argus_Retribution);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CooldownGameplayEffectClass = UGE_Bway_Cooldown_Retribution::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Argus", "RetributionName", "Retribution");
	DisplayData.Description = NSLOCTEXT("Argus", "RetributionDesc", "Quick uppercut into a powerful down smash.");
	DisplayData.bIsUltimate = true;
}

void UBwayGameplayAbility_Retribution::ActivateAbility(
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

	UWorld* World = GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	World->GetTimerManager().ClearTimer(UppercutTimerHandle);
	World->GetTimerManager().ClearTimer(DownSmashTimerHandle);
	World->GetTimerManager().ClearTimer(EndAbilityTimerHandle);

	World->GetTimerManager().SetTimer(
		UppercutTimerHandle,
		this,
		&UBwayGameplayAbility_Retribution::PerformUppercut,
		UppercutDelay,
		false);

	World->GetTimerManager().SetTimer(
		DownSmashTimerHandle,
		this,
		&UBwayGameplayAbility_Retribution::PerformDownSmash,
		DownSmashDelay,
		false);

	World->GetTimerManager().SetTimer(
		EndAbilityTimerHandle,
		[this, Handle, ActorInfo, ActivationInfo]()
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		},
		DownSmashDelay + 0.05f,
		false);
}

void UBwayGameplayAbility_Retribution::PerformUppercut()
{
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	ApplyMeleeHit(UppercutBaseDamage, DamageScaling, UppercutKnockback, UppercutUpward);
}

void UBwayGameplayAbility_Retribution::PerformDownSmash()
{
	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	ApplyMeleeHit(DownSmashBaseDamage, DamageScaling, DownSmashKnockback, -DownSmashDownward);
}

void UBwayGameplayAbility_Retribution::ApplyMeleeHit(float InAbilityBaseDamage, float Scaling, float KnockbackStrength, float KnockbackZ)
{
	if (!CachedCharacter)
	{
		return;
	}

	const FVector Start = CachedCharacter->GetActorLocation();
	const FVector End = Start + CachedCharacter->GetActorForwardVector() * TraceDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RetributionHit), false, CachedCharacter);
	TArray<FHitResult> Hits;
	if (UWorld* World = CachedCharacter->GetWorld())
	{
		World->SweepMultiByChannel(
			Hits,
			Start,
			End,
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeSphere(TraceRadius),
			QueryParams);
	}

	const float Damage = CalculateScaledDamage(InAbilityBaseDamage, Scaling);
	TSet<TObjectPtr<ABwayCharacterWithAbilities>> Damaged;
	for (const FHitResult& Hit : Hits)
	{
		ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(Hit.GetActor());
		if (!HitCharacter || Damaged.Contains(HitCharacter) || !IsEnemy(HitCharacter))
		{
			continue;
		}

		Damaged.Add(HitCharacter);
		ApplyDamageToEnemy(HitCharacter, Damage);

		FVector KnockDir = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
		ApplyKnockbackToEnemy(HitCharacter, KnockDir * KnockbackStrength + FVector(0.f, 0.f, KnockbackZ));
	}
}

void UBwayGameplayAbility_Retribution::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UppercutTimerHandle);
		World->GetTimerManager().ClearTimer(DownSmashTimerHandle);
		World->GetTimerManager().ClearTimer(EndAbilityTimerHandle);
	}

	CachedCharacter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
