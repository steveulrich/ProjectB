#include "Abilities/BwayGameplayAbility_KorrynFlock.h"

#include "Abilities/BwayGameplayEffect_KorrynEffects.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BwayGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_KorrynFlock)

UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Korryn_Flock, "Ability.Korryn.Flock");

UBwayGameplayAbility_KorrynFlock::UBwayGameplayAbility_KorrynFlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Korryn_Flock);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationOwnedTags.AddTag(TAG_Gameplay_DamageImmunity);
	ActivationOwnedTags.AddTag(BwayGameplayTags::State_Status_Ethereal);
	CooldownGameplayEffectClass = UGE_Bway_Cooldown_KorrynFlock::StaticClass();

	DisplayData.AbilityName = NSLOCTEXT("Korryn", "FlockName", "Flock");
	DisplayData.Description = NSLOCTEXT("Korryn", "FlockDesc", "Become ethereal and float briefly.");
}

void UBwayGameplayAbility_KorrynFlock::ActivateAbility(
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

	float Duration = FlockDuration;
	if (const UBwayKorrynKitConfig* Config = ResolveKitConfig())
	{
		Duration = Config->FlockDuration;
		HoverMaxSpeed = Config->FlockHoverSpeed;
	}

	ApplyEtherealMovement();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndFlockTimerHandle);
		World->GetTimerManager().SetTimer(
			EndFlockTimerHandle,
			[this, Handle, ActorInfo, ActivationInfo]()
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			},
			Duration,
			false);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UBwayGameplayAbility_KorrynFlock::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndFlockTimerHandle);
	}

	RestoreMovement();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBwayGameplayAbility_KorrynFlock::ApplyEtherealMovement()
{
	if (!CachedCharacter || bAppliedEthereal)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = CachedCharacter->GetCapsuleComponent())
	{
		StoredPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	if (UCharacterMovementComponent* MoveComp = CachedCharacter->GetCharacterMovement())
	{
		StoredMovementMode = MoveComp->MovementMode;
		StoredCustomMovementMode = MoveComp->CustomMovementMode;
		StoredGravityScale = MoveComp->GravityScale;
		StoredMaxWalkSpeed = MoveComp->MaxWalkSpeed;
		bStoredOrientRotationToMovement = MoveComp->bOrientRotationToMovement;

		MoveComp->SetMovementMode(MOVE_Flying);
		MoveComp->GravityScale = 0.f;
		MoveComp->MaxFlySpeed = HoverMaxSpeed;
		MoveComp->MaxWalkSpeed = HoverMaxSpeed;
		MoveComp->Velocity.Z = FMath::Max(MoveComp->Velocity.Z, 0.f);
	}

	bAppliedEthereal = true;
}

void UBwayGameplayAbility_KorrynFlock::RestoreMovement()
{
	if (!CachedCharacter || !bAppliedEthereal)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = CachedCharacter->GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, StoredPawnResponse);
	}

	if (UCharacterMovementComponent* MoveComp = CachedCharacter->GetCharacterMovement())
	{
		MoveComp->GravityScale = StoredGravityScale;
		MoveComp->MaxWalkSpeed = StoredMaxWalkSpeed;
		MoveComp->bOrientRotationToMovement = bStoredOrientRotationToMovement;

		if (StoredMovementMode == MOVE_Custom)
		{
			MoveComp->SetMovementMode(MOVE_Custom, StoredCustomMovementMode);
		}
		else if (StoredMovementMode == MOVE_Flying)
		{
			MoveComp->SetMovementMode(MOVE_Falling);
		}
		else
		{
			MoveComp->SetMovementMode(StoredMovementMode);
		}
	}

	bAppliedEthereal = false;
}
