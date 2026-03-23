// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/BwayGameplayAbility_GladiatorsLeap.h"

#include "AbilitySystemComponent.h"
#include "BwayCharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_State_Leaping, "State.Leaping");
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Spartacus_GladiatorsLeap, "Ability.Spartacus.GladiatorsLeap");

UBwayGameplayAbility_GladiatorsLeap::UBwayGameplayAbility_GladiatorsLeap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

	FGameplayTagContainer TempTags = GetAssetTags();
	TempTags.AddTag(TAG_Ability_Spartacus_GladiatorsLeap);
	SetAssetTags(TempTags);
	ActivationOwnedTags.AddTag(TAG_State_Leaping);
	LeapingStateTag = TAG_State_Leaping;
}

bool UBwayGameplayAbility_GladiatorsLeap::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
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

	// Check if already leaping
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && ASC->HasMatchingGameplayTag(TAG_State_Leaping))
	{
		return false;
	}

	return true;
}

void UBwayGameplayAbility_GladiatorsLeap::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	// Get target location
	FVector TargetLocation = GetTargetLocation();
	LandingLocation = TargetLocation;

	// Perform leap
	PerformLeap(TargetLocation);

	// Start checking for landing
	bIsLeaping = true;
	UWorld* World = GetWorld();
	if (World)
	{
		// Clear any existing timer before setting a new one
		World->GetTimerManager().ClearTimer(LandingCheckTimerHandle);
		
		// Check for landing at regular intervals
		World->GetTimerManager().SetTimer(
			LandingCheckTimerHandle,
			this,
			&UBwayGameplayAbility_GladiatorsLeap::OnLanding,
			LandingCheckInterval,
			true
		);
	}
}

FVector UBwayGameplayAbility_GladiatorsLeap::GetTargetLocation() const
{
	if (!CachedCharacter)
	{
		return FVector::ZeroVector;
	}

	AController* Controller = CachedCharacter->GetController();
	if (!Controller)
	{
		return FVector::ZeroVector;
	}

	FVector CharacterLocation = CachedCharacter->GetActorLocation();
	FRotator ControlRotation = Controller->GetControlRotation();
	FVector ForwardDirection = ControlRotation.Vector();

	// Use configured max leap distance
	FVector TargetPoint = CharacterLocation + (ForwardDirection * MaxLeapDistance);

	// Ground trace to find landing location
	UWorld* World = GetWorld();
	if (!World)
	{
		return TargetPoint;
	}

	FVector TraceStart = TargetPoint + FVector(0.0f, 0.0f, GroundTraceHeightOffset);
	FVector TraceEnd = TargetPoint - FVector(0.0f, 0.0f, GroundTraceDistance);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CachedCharacter);
	QueryParams.bTraceComplex = false;

	FHitResult HitResult;
	if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		return HitResult.ImpactPoint;
	}

	return TargetPoint;
}

void UBwayGameplayAbility_GladiatorsLeap::PerformLeap(const FVector& TargetLocation)
{
	if (!CachedCharacter || !CachedMovementComponent)
	{
		return;
	}

	FVector StartLocation = CachedCharacter->GetActorLocation();
	FVector Direction = (TargetLocation - StartLocation).GetSafeNormal();
	
	// Calculate launch velocity (parabolic trajectory)
	float Distance = FVector::Dist2D(StartLocation, TargetLocation);
	float Height = TargetLocation.Z - StartLocation.Z;
	
	// Calculate initial velocity for parabolic trajectory using configured constants
	float LaunchSpeed = FMath::Sqrt((Distance * GravityConstant) / FMath::Sin(2.0f * FMath::DegreesToRadians(LeapLaunchAngle)));
	
	FVector LaunchVelocity = Direction * LaunchSpeed;
	LaunchVelocity.Z = LaunchSpeed * FMath::Sin(FMath::DegreesToRadians(LeapLaunchAngle));
	
	// Adjust for height difference
	if (Height > 0)
	{
		LaunchVelocity.Z += FMath::Sqrt(2.0f * GravityConstant * Height);
	}

	// Launch character
	CachedCharacter->LaunchCharacter(LaunchVelocity, false, false);
}

void UBwayGameplayAbility_GladiatorsLeap::OnLanding()
{
	if (!CachedCharacter || !CachedMovementComponent || !bIsLeaping)
	{
		return;
	}

	// Check if character is on ground
	if (CachedMovementComponent->IsMovingOnGround())
	{
		// Character has landed
		FVector CurrentLocation = CachedCharacter->GetActorLocation();
		PerformAOEDamage(CurrentLocation);
		
		bIsLeaping = false;
		
		// End ability
		EndAbility(StoredSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UBwayGameplayAbility_GladiatorsLeap::PerformAOEDamage(const FVector& Location)
{
	if (!CachedCharacter)
	{
		return;
	}

	// Get enemies in radius using base class method
	TArray<ABwayCharacterWithAbilities*> Enemies = GetEnemiesInRadius(Location, DamageRadius);

	// Apply damage to each enemy using base class method
	for (ABwayCharacterWithAbilities* Enemy : Enemies)
	{
		if (Enemy)
		{
			ApplyDamageToEnemy(Enemy, DamageAmount);
		}
	}
}

void UBwayGameplayAbility_GladiatorsLeap::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Clear landing check timer to prevent callbacks after ability ends
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LandingCheckTimerHandle);
	}

	bIsLeaping = false;
	CachedCharacter = nullptr;
	CachedMovementComponent = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

