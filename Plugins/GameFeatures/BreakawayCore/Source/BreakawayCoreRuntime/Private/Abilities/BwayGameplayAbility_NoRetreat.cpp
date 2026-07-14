#include "Abilities/BwayGameplayAbility_NoRetreat.h"

#include "Abilities/BwayGameplayEffect_ArgusCooldowns.h"
#include "BwayCharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_NoRetreat)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Argus_NoRetreat, "Ability.Argus.NoRetreat");

UBwayGameplayAbility_NoRetreat::UBwayGameplayAbility_NoRetreat(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Argus_NoRetreat);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	const FGameplayTag DashingTag = FGameplayTag::RequestGameplayTag(FName("State.Dashing"), /*ErrorIfNotFound*/ false);
	if (DashingTag.IsValid())
	{
		ActivationOwnedTags.AddTag(DashingTag);
	}
	CooldownGameplayEffectClass = UGE_Bway_Cooldown_NoRetreat::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Argus", "NoRetreatName", "No Retreat");
	DisplayData.Description = NSLOCTEXT("Argus", "NoRetreatDesc", "Charge forward and strike the first enemy hit.");
}

void UBwayGameplayAbility_NoRetreat::ActivateAbility(
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

	bHasHitEnemy = false;
	if (const AController* Controller = CachedCharacter->GetController())
	{
		FRotator YawOnly = Controller->GetControlRotation();
		YawOnly.Pitch = 0.f;
		YawOnly.Roll = 0.f;
		ChargeDirection = YawOnly.Vector().GetSafeNormal2D();
	}
	else
	{
		ChargeDirection = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector ChargeVelocity = ChargeDirection * (ChargeDistance / FMath::Max(ChargeDuration, 0.05f));
	CachedCharacter->LaunchCharacter(ChargeVelocity, true, true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTraceTimerHandle);
		World->GetTimerManager().ClearTimer(EndChargeTimerHandle);

		World->GetTimerManager().SetTimer(
			ChargeTraceTimerHandle,
			this,
			&UBwayGameplayAbility_NoRetreat::PerformChargeTrace,
			ChargeTraceInterval,
			true);

		World->GetTimerManager().SetTimer(
			EndChargeTimerHandle,
			[this, Handle, ActorInfo, ActivationInfo]()
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			},
			ChargeDuration,
			false);
	}
}

void UBwayGameplayAbility_NoRetreat::PerformChargeTrace()
{
	if (!CachedCharacter || bHasHitEnemy || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector TraceStart = CachedCharacter->GetActorLocation();
	const FVector TraceEnd = TraceStart + (ChargeDirection * TraceRadius);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NoRetreatCharge), false, CachedCharacter);
	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(TraceRadius),
		QueryParams);

	for (const FHitResult& Hit : Hits)
	{
		ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(Hit.GetActor());
		if (!HitCharacter || !IsEnemy(HitCharacter))
		{
			continue;
		}

		ApplyHitToEnemy(HitCharacter);
		bHasHitEnemy = true;

		if (UBwayCharacterMovementComponent* MoveComp = CachedCharacter->GetBwayCharacterMovement())
		{
			MoveComp->Velocity = FVector::ZeroVector;
		}

		World->GetTimerManager().ClearTimer(ChargeTraceTimerHandle);
		break;
	}
}

void UBwayGameplayAbility_NoRetreat::ApplyHitToEnemy(ABwayCharacterWithAbilities* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	const float Damage = CalculateScaledDamage(AbilityBaseDamage, DamageScaling);
	ApplyDamageToEnemy(Enemy, Damage);

	FVector KnockDir = ChargeDirection;
	if (KnockDir.IsNearlyZero())
	{
		KnockDir = (Enemy->GetActorLocation() - CachedCharacter->GetActorLocation()).GetSafeNormal2D();
	}
	ApplyKnockbackToEnemy(Enemy, KnockDir * KnockbackStrength + FVector(0.f, 0.f, KnockbackUpward));
}

void UBwayGameplayAbility_NoRetreat::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTraceTimerHandle);
		World->GetTimerManager().ClearTimer(EndChargeTimerHandle);
	}

	bHasHitEnemy = false;
	CachedCharacter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
