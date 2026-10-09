// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayGameplayAbility_Slide.h"
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "GameplayEffectTypes.h"
#include "BwayCharacterMovementComponent.h" // Include custom CMC header
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "NativeGameplayTags.h" // For standard tags if needed
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Sliding, "State.Movement.Sliding");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputTag_Ability_Slide, "InputTag.Ability.Slide");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Trigger_Slide, "Ability.Trigger.Slide");

UBwayGameplayAbility_Slide::UBwayGameplayAbility_Slide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted; // Standard for responsive actions [6, 10]
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered; // Assumes input action triggers on press

	FGameplayTagContainer TempTags = GetAssetTags();
	TempTags.AddTag(TAG_Ability_Trigger_Slide); // Identify this ability
	SetAssetTags(TempTags);
	ActivationOwnedTags.AddTag(TAG_State_Movement_Sliding); // Tag applied while active

	// Default RequiredStateTag and SlidingStateTag should be set in the Blueprint subclass
	// SlideInputAction should be assigned in the Blueprint subclass
}

bool UBwayGameplayAbility_Slide::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo ||!ActorInfo->AvatarActor.IsValid())
	{
		UE_LOG(LogTemp, Verbose, TEXT("CanUseAbility - Slide : FALSE (ActorInfo Not Valid") );
		return false;
	}

	const ABwayCharacterWithAbilities* BwayCharacter = Cast<ABwayCharacterWithAbilities>(ActorInfo->AvatarActor.Get());
	if (!BwayCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("CanUseAbility - Slide : FALSE (BwayCharacter Failed Cast") );
		return false;
	}

	// Availability checks also run for rejected activations and after avatar changes.
	// Cache execution state only once an activation has actually begun.
	const UBwayCharacterMovementComponent* MoveComp = BwayCharacter->GetBwayCharacterMovement();
	if (!MoveComp)
	{
		return false;
	}

	// --- Check if Already Sliding ---
	if (MoveComp->IsSliding())
	{
		UE_LOG(LogTemp, Verbose, TEXT("CanUseAbility - Slide: Condition Failed (Already Sliding)"));
		return false;
	}

	// Standard checks (Super includes cooldown check if CooldownGameplayEffectClass is set)
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		UE_LOG(LogTemp, Warning, TEXT("CanUseAbility - Slide : FALSE (Super Fail)") );
		return false;
	}

	// Check explicit cooldown tag (Cooldown.Skill.Slide) if Jump applies it
	// This check might be redundant if Super::CheckCooldown handles it via CooldownGameplayEffectClass,
	// but necessary if cooldown is applied externally by Jump GA.
	// UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cooldown_Skill_Slide, "Cooldown.Skill.Slide");
	// if (ASC && ASC->HasMatchingGameplayTag(TAG_Cooldown_Skill_Slide))
	// {
	//     if (OptionalRelevantTags) OptionalRelevantTags->AddTag(TAG_Cooldown_Skill_Slide);
	//     return false;
	// }

	UE_LOG(LogTemp, Verbose, TEXT("CanUseAbility - Slide : TRUE") );
	return true; // All conditions met
}

void UBwayGameplayAbility_Slide::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo)
	{
		EndAbility(Handle, CurrentActorInfo, ActivationInfo, true, true);
		return;
	}
	const uint64 ThisActivation = ++ActivationSerial;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (ActivationSerial != ThisActivation || !IsActive())
	{
		return;
	}

	ABwayCharacterWithAbilities* BwayCharacter = ActorInfo ? Cast<ABwayCharacterWithAbilities>(ActorInfo->AvatarActor.Get()) : nullptr;
	CachedBwayMoveComp = BwayCharacter? Cast<UBwayCharacterMovementComponent>(BwayCharacter->GetBwayCharacterMovement()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	UE_LOG(LogTemp, Verbose, TEXT("ActivateAbility - Slide Start") );

	if (!CachedBwayMoveComp ||!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("ActivateAbility - Failed: No MoveComp/ASC") );
		bool bReplicateEndAbility = true;
		bool bWasCancelled = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}

	// Commit optional cooldown ON ENTRY (Only if CooldownGameplayEffectClass is set, deviates from Lua)
	if (GetCooldownGameplayEffect())
	{
		// Note: This applies cooldown immediately upon starting the slide.
		if (!CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, true))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
	}
	if (ActivationSerial != ThisActivation || !IsActive() || !CachedBwayMoveComp)
	{
		return;
	}

	// --- Force Downward if Falling ---
	// Optional: Apply a downward impulse if activating while airborne
	if (CachedBwayMoveComp->IsFalling())
	{
		// Example: Add a strong downward impulse. Adjust Z value as needed.
		// This is a simple way; more complex physics might be desired.
		// Consider potential issues with replication and prediction if doing direct velocity changes.
		CachedBwayMoveComp->AddImpulse(FVector(0.f, 0.f, -1000.f), true);
		// Alternatively, just setting the mode might be sufficient, letting gravity do the work.
		UE_LOG(LogTemp, Warning, TEXT("ActivateAbility - Slide: Activating while falling."));
	}

	// --- Trigger Custom Movement Mode ---
	// This is the primary action of this ability.
	BwayCharacter->MovementModeChangedDelegate.AddUniqueDynamic(this, &ThisClass::OnMovementModeChanged);
	CachedBwayMoveComp->SetMovementMode(MOVE_Custom, (uint8)ECustomMovementMode::CMOVE_Slide);
	// Movement callbacks (including Blueprint callbacks) can end this activation.
	if (ActivationSerial != ThisActivation || !IsActive() || !CachedBwayMoveComp)
	{
		return;
	}
	FGameplayCueParameters SlideCueParams;
	SlideCueParams.Location = FVector_NetQuantize10(BwayCharacter->GetActorLocation());
	SlideCueParams.Instigator = BwayCharacter;
	SlideCueParams.EffectCauser = BwayCharacter;
	ASC->ExecuteGameplayCue(BwayGameplayCueTags::MovementSlide, SlideCueParams);
	if (ActivationSerial != ThisActivation || !IsActive() || !CachedBwayMoveComp)
	{
		return;
	}
	UE_LOG(LogTemp, Verbose, TEXT("ActivateAbility - Slide: Movement Mode Set") );

	// --- Wait for Input Release ---
	// Create and configure the task to wait for the input release
	UAbilityTask_WaitInputRelease* WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	if (WaitInputReleaseTask)
	{
		// Bind the EndAbility function (or a custom function that calls EndAbility) to the OnRelease delegate
		WaitInputReleaseTask->OnRelease.AddDynamic(this, &UBwayGameplayAbility_Slide::OnInputRelease);
		// A previously received remote release can end the ability synchronously.
		WaitInputReleaseTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Slide Ability: Failed to create WaitInputRelease task. Ending ability."));
		bool bReplicateEndAbility = true;
		bool bWasCancelled = true; // Treat failure to create task as cancellation
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}
}

// Add this new function to handle the release event
void UBwayGameplayAbility_Slide::OnInputRelease(float TimeHeld)
{
	// Input was released, end the ability
	bool bReplicateEndAbility = true;
	bool bWasCancelled = false; // Input release is not a cancellation
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBwayGameplayAbility_Slide::OnMovementModeChanged(ACharacter* Character, EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (IsActive() && CachedBwayMoveComp && Character == CachedBwayMoveComp->GetCharacterOwner()
		&& PreviousMovementMode == MOVE_Custom && PreviousCustomMode == uint8(ECustomMovementMode::CMOVE_Slide)
		&& !CachedBwayMoveComp->IsSliding())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UBwayGameplayAbility_Slide::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEndingSlide || !IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}

	{
		TGuardValue<bool> EndingGuard(bEndingSlide, true);
		UBwayCharacterMovementComponent* MoveComp = CachedBwayMoveComp;
		CachedBwayMoveComp = nullptr;
		if (MoveComp)
		{
			if (ACharacter* Character = MoveComp->GetCharacterOwner())
			{
				Character->MovementModeChangedDelegate.RemoveDynamic(this, &ThisClass::OnMovementModeChanged);
			}
			// Preserve a jump, fall or other mode that already interrupted sliding.
			// Unbind first because SetMovementMode invokes callbacks synchronously.
			if (MoveComp->IsSliding())
			{
				MoveComp->SetMovementMode(MOVE_Walking);
			}
		}
	}

	// GAS guards its own teardown. Its ended callbacks may synchronously start a
	// new activation, whose termination must not inherit the old cleanup guard.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
