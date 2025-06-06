// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayGameplayAbility_Slide.h"
#include "Character/LyraCharacter.h"
#include "BwayCharacterMovementComponent.h" // Include custom CMC header
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "LyraGameplayTags.h" // Assuming tags are defined here or globally
#include "NativeGameplayTags.h" // For standard tags if needed

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Sliding, "State.Movement.Sliding");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputTag_Ability_Slide, "InputTag.Ability.Slide");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Trigger_Slide, "Ability.Trigger.Slide");

UBwayGameplayAbility_Slide::UBwayGameplayAbility_Slide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted; // Standard for responsive actions [6, 10]
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered; // Assumes input action triggers on press

	AbilityTags.AddTag(TAG_Ability_Trigger_Slide); // Identify this ability
	ActivationOwnedTags.AddTag(TAG_State_Movement_Sliding); // Tag applied while active

	// Default RequiredStateTag and SlidingStateTag should be set in the Blueprint subclass
	// SlideInputAction should be assigned in the Blueprint subclass
}

bool UBwayGameplayAbility_Slide::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo ||!ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	const ABwayCharacterWithAbilities* BwayCharacter = Cast<ABwayCharacterWithAbilities>(ActorInfo->AvatarActor.Get());
	if (!BwayCharacter)
	{
		return false;
	}

	const UBwayCharacterMovementComponent* MoveComp = Cast<UBwayCharacterMovementComponent>(BwayCharacter->GetBwayCharacterMovement());
	if (!MoveComp)
	{
		return false; // Requires the custom CMC
	}

	// Check if character is on the ground
	if (!MoveComp->IsMovingOnGround())
	{
		// Optional: Failure tag Ability.ActivationFailure.NotGrounded
		return false;
	}

	// Check if character is already sliding
	if (MoveComp->IsCustomMovementMode((uint8)ECustomMovementMode::CMOVE_Slide) || MoveComp->MovementMode == MOVE_Custom)
	{
		// Check the specific custom mode if MOVE_Custom is active
		if (MoveComp->MovementMode == MOVE_Custom && MoveComp->CustomMovementMode == (uint8)ECustomMovementMode::CMOVE_Slide)
		{
			return false; // Already sliding
		}
		// Allow if in a *different* custom mode? Depends on design. Assuming not for now.
		// return false;
	}


	// Check for required prerequisite tag (e.g., Sprinting)
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC || (RequiredStateTag.IsValid() &&!ASC->HasMatchingGameplayTag(RequiredStateTag)))
	{
		// Optional: Failure tag Ability.ActivationFailure.MissingRequiredTag
		return false;
	}

	// Standard checks (Super includes cooldown check if CooldownGameplayEffectClass is set)
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
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


	return true; // All conditions met
}

void UBwayGameplayAbility_Slide::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ABwayCharacterWithAbilities* BwayCharacter = Cast<ABwayCharacterWithAbilities>(ActorInfo->AvatarActor.Get());
	CachedBwayMoveComp = BwayCharacter? Cast<UBwayCharacterMovementComponent>(BwayCharacter->GetBwayCharacterMovement()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	if (!CachedBwayMoveComp ||!ASC)
	{
		bool bReplicateEndAbility = true;
		bool bWasCancelled = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}

	// Commit optional cooldown ON ENTRY (Only if CooldownGameplayEffectClass is set, deviates from Lua)
	if (GetCooldownGameplayEffect())
	{
		// Note: This applies cooldown immediately upon starting the slide.
		CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, true);
	}

	// --- Trigger Custom Movement Mode ---
	// This is the primary action of this ability.
	CachedBwayMoveComp->SetMovementMode(MOVE_Custom, (uint8)ECustomMovementMode::CMOVE_Slide);

	// --- Apply State Tag ---
	// The SlidingStateTag (e.g., State.Movement.Sliding) is automatically applied
	// because it's listed in ActivationOwnedTags in the constructor.

	// Ability remains active while the CMC is in CMOVE_Slide.
	// The CMC is responsible for checking exit conditions (input release, speed drop, jump)
	// and calling SetMovementMode to exit the custom mode.
	// When the CMC exits CMOVE_Slide, OnMovementModeChanged will fire.
	// We don't strictly need a delegate here; the ability can end when the State.Movement.Sliding tag is removed
	// (which happens automatically when the ability ends) or be ended manually if needed.
	// For simplicity, let the ability stay active. It will be implicitly ended if another
	// ability cancels abilities with its tag, or explicitly ended if necessary.
}

void UBwayGameplayAbility_Slide::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
        // Ensure the CMC exits slide mode if the ability ends unexpectedly. Normally the
        // movement component handles leaving slide when the input is released, but this is
        // a safeguard for cancellation or other interruptions.
        if (CachedBwayMoveComp && CachedBwayMoveComp->IsCustomMovementMode((uint8)ECustomMovementMode::CMOVE_Slide))
        {
                CachedBwayMoveComp->SetMovementMode(CachedBwayMoveComp->IsFalling() ? MOVE_Falling : MOVE_Walking);
        }

        CachedBwayMoveComp = nullptr;
        Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
        // ActivationOwnedTags (State.Movement.Sliding) are automatically removed by Super::EndAbility.
}
