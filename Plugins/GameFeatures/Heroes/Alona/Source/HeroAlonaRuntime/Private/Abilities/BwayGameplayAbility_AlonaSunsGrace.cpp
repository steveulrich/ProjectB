#include "Abilities/BwayGameplayAbility_AlonaSunsGrace.h"

#include "Abilities/BwayGameplayEffect_AlonaCooldowns.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_AlonaSunsGrace)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Alona_SunsGrace, "Ability.Alona.SunsGrace");

UBwayGameplayAbility_AlonaSunsGrace::UBwayGameplayAbility_AlonaSunsGrace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Alona_SunsGrace);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationOwnedTags.AddTag(TAG_Gameplay_DamageImmunity);
	CooldownGameplayEffectClass = UGE_Bway_Cooldown_AlonaSunsGrace::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Alona", "SunsGraceName", "Sun's Grace");
	DisplayData.Description = NSLOCTEXT("Alona", "SunsGraceDesc", "Teleport a short distance while invulnerable.");
}

void UBwayGameplayAbility_AlonaSunsGrace::ActivateAbility(
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
		const FVector Direction = ResolveTeleportDirection();
		const FVector Destination = ResolveTeleportDestination(Direction);
		CachedCharacter->TeleportTo(Destination, CachedCharacter->GetActorRotation(), false, false);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndInvulnTimerHandle);
		World->GetTimerManager().SetTimer(
			EndInvulnTimerHandle,
			[this, Handle, ActorInfo, ActivationInfo]()
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			},
			InvulnerabilityDuration,
			false);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

FVector UBwayGameplayAbility_AlonaSunsGrace::ResolveTeleportDirection() const
{
	if (!CachedCharacter)
	{
		return FVector::ForwardVector;
	}

	FVector Direction = CachedCharacter->GetLastMovementInputVector().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		if (const AController* Controller = CachedCharacter->GetController())
		{
			FRotator YawOnly = Controller->GetControlRotation();
			YawOnly.Pitch = 0.f;
			YawOnly.Roll = 0.f;
			Direction = YawOnly.Vector().GetSafeNormal2D();
		}
	}

	if (Direction.IsNearlyZero())
	{
		Direction = CachedCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	return Direction;
}

FVector UBwayGameplayAbility_AlonaSunsGrace::ResolveTeleportDestination(const FVector& Direction) const
{
	if (!CachedCharacter)
	{
		return FVector::ZeroVector;
	}

	UWorld* World = GetWorld();
	UCapsuleComponent* Capsule = CachedCharacter->GetCapsuleComponent();
	if (!World || !Capsule)
	{
		return CachedCharacter->GetActorLocation() + Direction * TeleportDistance;
	}

	float CapsuleRadius = 0.f;
	float CapsuleHalfHeight = 0.f;
	Capsule->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	const FVector Start = CachedCharacter->GetActorLocation();
	const FVector IdealEnd = Start + Direction * TeleportDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AlonaSunsGraceTeleport), false, CachedCharacter);
	FHitResult SweepHit;
	FVector Destination = IdealEnd;
	if (World->SweepSingleByChannel(
		SweepHit,
		Start,
		IdealEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
		QueryParams))
	{
		Destination = SweepHit.Location;
	}

	// Floor snap so teleport does not leave character floating / underground.
	const FVector FloorTraceStart = Destination + FVector(0.f, 0.f, FloorTraceUp);
	const FVector FloorTraceEnd = Destination - FVector(0.f, 0.f, FloorTraceDown);
	FHitResult FloorHit;
	if (World->LineTraceSingleByChannel(FloorHit, FloorTraceStart, FloorTraceEnd, ECC_WorldStatic, QueryParams))
	{
		Destination.Z = FloorHit.ImpactPoint.Z + CapsuleHalfHeight + 2.f;
	}

	return Destination;
}

void UBwayGameplayAbility_AlonaSunsGrace::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndInvulnTimerHandle);
	}

	CachedCharacter = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
