// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/BwayGameplayAbility_ShieldBash.h"

#include "AbilitySystemComponent.h"
#include "BwayCharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dashing, "State.Dashing");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Spartacus_ShieldBash, "Ability.Spartacus.ShieldBash");

UBwayGameplayAbility_ShieldBash::UBwayGameplayAbility_ShieldBash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(TAG_Ability_Spartacus_ShieldBash);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	ActivationOwnedTags.AddTag(TAG_State_Dashing);
	DashingStateTag = TAG_State_Dashing;
}

bool UBwayGameplayAbility_ShieldBash::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	const ABwayCharacterWithAbilities* Character = Cast<ABwayCharacterWithAbilities>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		return false;
	}

	// Check if already dashing
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && ASC->HasMatchingGameplayTag(TAG_State_Dashing))
	{
		return false;
	}

	return true;
}

void UBwayGameplayAbility_ShieldBash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	CachedMovementComponent = CachedCharacter->GetBwayCharacterMovement();
	if (!CachedMovementComponent)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Commit cooldown
	if (GetCooldownGameplayEffect())
	{
		CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, true);
	}

	// Get dash direction (forward direction of character)
	AController* Controller = CachedCharacter->GetController();
	if (!Controller)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FRotator ControlRotation = Controller->GetControlRotation();
	ControlRotation.Pitch = 0.0f; // Keep dash horizontal
	DashDirection = ControlRotation.Vector();
	DashStartLocation = CachedCharacter->GetActorLocation();
	bHasHitEnemy = false;

	// Calculate dash velocity
	FVector DashVelocity = DashDirection * (DashDistance / DashDuration);
	
	// Launch character forward
	CachedCharacter->LaunchCharacter(DashVelocity, false, false);

	// Start trace timer to check for enemies during dash
	UWorld* World = GetWorld();
	if (World)
	{
		// Clear any existing timers before setting new ones
		World->GetTimerManager().ClearTimer(DashTraceTimerHandle);
		World->GetTimerManager().ClearTimer(EndDashTimerHandle);

		// Perform trace at regular intervals during dash
		World->GetTimerManager().SetTimer(
			DashTraceTimerHandle,
			this,
			&UBwayGameplayAbility_ShieldBash::PerformDashTrace,
			DashTraceInterval,
			true
		);

		// End dash after duration - stored as member to allow cancellation
		World->GetTimerManager().SetTimer(
			EndDashTimerHandle,
			[this, Handle, ActorInfo, ActivationInfo]()
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			},
			DashDuration,
			false
		);
	}
}

void UBwayGameplayAbility_ShieldBash::PerformDashTrace()
{
	if (!CachedCharacter || bHasHitEnemy)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector CurrentLocation = CachedCharacter->GetActorLocation();
	FVector TraceStart = CurrentLocation;
	FVector TraceEnd = CurrentLocation + (DashDirection * TraceRadius);

	// Prepare collision query
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CachedCharacter);
	QueryParams.bTraceComplex = false;

	// Sphere sweep for enemies
	TArray<FHitResult> HitResults;
	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(TraceRadius);
	
	bool bHit = World->SweepMultiByChannel(
		HitResults,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ECC_Pawn,
		CollisionShape,
		QueryParams
	);

	if (bHit)
	{
		// Check each hit for enemy characters
		for (const FHitResult& Hit : HitResults)
		{
			ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(Hit.GetActor());
			if (HitCharacter && HitCharacter != CachedCharacter)
			{
				// Check if enemy (different team)
				if (IsEnemy(HitCharacter))
				{
					ApplyStunToEnemy(HitCharacter);
					bHasHitEnemy = true;
					
					// Stop dash
					if (CachedMovementComponent)
					{
						CachedMovementComponent->Velocity = FVector::ZeroVector;
					}
					
					// Clear trace timer
					if (World)
					{
						World->GetTimerManager().ClearTimer(DashTraceTimerHandle);
					}
					
					break;
				}
			}
		}
	}
}

void UBwayGameplayAbility_ShieldBash::ApplyStunToEnemy(AActor* EnemyActor)
{
	if (!EnemyActor || !StunGameplayEffectClass)
	{
		return;
	}

	// Use base class method to apply effect
	ApplyGameplayEffectToTarget(EnemyActor, StunGameplayEffectClass, 1.0f);
}

void UBwayGameplayAbility_ShieldBash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Clear all timers to prevent callbacks after ability ends
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashTraceTimerHandle);
		World->GetTimerManager().ClearTimer(EndDashTimerHandle);
	}

	// Reset state
	bHasHitEnemy = false;
	CachedCharacter = nullptr;
	CachedMovementComponent = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

