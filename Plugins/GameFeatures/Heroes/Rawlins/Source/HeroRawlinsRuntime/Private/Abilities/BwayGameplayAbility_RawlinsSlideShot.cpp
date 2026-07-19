#include "Abilities/BwayGameplayAbility_RawlinsSlideShot.h"

#include "Abilities/BwayGameplayEffect_RawlinsCooldowns.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_RawlinsSlideShot)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Rawlins_SlideShot, "Ability.Rawlins.SlideShot");

UBwayGameplayAbility_RawlinsSlideShot::UBwayGameplayAbility_RawlinsSlideShot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Rawlins_SlideShot);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	const FGameplayTag DashingTag = FGameplayTag::RequestGameplayTag(FName("State.Dashing"), /*ErrorIfNotFound*/ false);
	if (DashingTag.IsValid())
	{
		ActivationOwnedTags.AddTag(DashingTag);
	}
	CooldownGameplayEffectClass = UGE_Bway_Cooldown_RawlinsSlideShot::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Rawlins", "SlideShotName", "Slide Shot");
	DisplayData.Description = NSLOCTEXT("Rawlins", "SlideShotDesc", "Slide forward and launch opponents into the air.");
}

void UBwayGameplayAbility_RawlinsSlideShot::ActivateAbility(
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

	ActiveSlideDistance = SlideDistance;
	ActiveSlideDuration = SlideDuration;
	ActiveTraceRadius = TraceRadius;
	ActiveBaseDamage = AbilityBaseDamage;
	ActiveDamageScaling = DamageScaling;
	ActiveLaunchStrength = LaunchStrength;
	ActiveLaunchUpward = LaunchUpward;
	if (const UBwayRawlinsKitConfig* Config = ResolveKitConfig())
	{
		ActiveSlideDistance = Config->SlideShotSlideDistance;
		ActiveSlideDuration = Config->SlideShotSlideDuration;
		ActiveTraceRadius = Config->SlideShotTraceRadius;
		ActiveBaseDamage = Config->SlideShotBaseDamage;
		ActiveDamageScaling = Config->SlideShotDamageScaling;
		ActiveLaunchStrength = Config->SlideShotLaunchStrength;
		ActiveLaunchUpward = Config->SlideShotLaunchUpward;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	HitEnemies.Reset();
	if (const AController* Controller = CachedCharacter->GetController())
	{
		FRotator YawOnly = Controller->GetControlRotation();
		YawOnly.Pitch = 0.f;
		YawOnly.Roll = 0.f;
		SlideDirection = YawOnly.Vector().GetSafeNormal2D();
	}
	else
	{
		SlideDirection = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector SlideVelocity = SlideDirection * (ActiveSlideDistance / FMath::Max(ActiveSlideDuration, 0.05f));
	CachedCharacter->LaunchCharacter(SlideVelocity, true, true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlideTraceTimerHandle);
		World->GetTimerManager().ClearTimer(EndSlideTimerHandle);

		World->GetTimerManager().SetTimer(
			SlideTraceTimerHandle,
			this,
			&UBwayGameplayAbility_RawlinsSlideShot::PerformSlideTrace,
			SlideTraceInterval,
			true);

		World->GetTimerManager().SetTimer(
			EndSlideTimerHandle,
			[this, Handle, ActorInfo, ActivationInfo]()
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			},
			ActiveSlideDuration,
			false);
	}
}

void UBwayGameplayAbility_RawlinsSlideShot::PerformSlideTrace()
{
	if (!CachedCharacter || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector TraceStart = CachedCharacter->GetActorLocation();
	const FVector TraceEnd = TraceStart + (SlideDirection * ActiveTraceRadius);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RawlinsSlideShot), false, CachedCharacter);
	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(ActiveTraceRadius),
		QueryParams);

	for (const FHitResult& Hit : Hits)
	{
		ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(Hit.GetActor());
		if (!HitCharacter || HitEnemies.Contains(HitCharacter) || !IsEnemy(HitCharacter))
		{
			continue;
		}

		HitEnemies.Add(HitCharacter);
		ApplyHitToEnemy(HitCharacter);
	}
}

void UBwayGameplayAbility_RawlinsSlideShot::ApplyHitToEnemy(ABwayCharacterWithAbilities* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	const float Damage = CalculateScaledDamage(ActiveBaseDamage, ActiveDamageScaling);
	ApplyDamageToEnemy(Enemy, Damage);

	FVector LaunchDir = SlideDirection;
	if (LaunchDir.IsNearlyZero())
	{
		LaunchDir = (Enemy->GetActorLocation() - CachedCharacter->GetActorLocation()).GetSafeNormal2D();
	}
	ApplyKnockbackToEnemy(Enemy, LaunchDir * ActiveLaunchStrength + FVector(0.f, 0.f, ActiveLaunchUpward));
}

void UBwayGameplayAbility_RawlinsSlideShot::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlideTraceTimerHandle);
		World->GetTimerManager().ClearTimer(EndSlideTimerHandle);
	}

	HitEnemies.Reset();
	CachedCharacter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
