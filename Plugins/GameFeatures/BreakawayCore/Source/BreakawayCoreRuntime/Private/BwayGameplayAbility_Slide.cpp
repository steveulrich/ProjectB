// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayGameplayAbility_Slide.h"
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

	AbilityTags.AddTag(TAG_Ability_Trigger_Slide); // Identify this ability
	ActivationOwnedTags.AddTag(TAG_State_Movement_Sliding); // Tag applied while active

	// Default RequiredStateTag and SlidingStateTag should be set in the Blueprint subclass
	// SlideInputAction should be assigned in the Blueprint subclass
}

bool UBwayGameplayAbility_Slide::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo ||!ActorInfo->AvatarActor.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("CanUseAbility - Slide : FALSE (ActorInfo Not Valid") );
		return false;
	}

	const ABwayCharacterWithAbilities* BwayCharacter = Cast<ABwayCharacterWithAbilities>(ActorInfo->AvatarActor.Get());
	if (!BwayCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("CanUseAbility - Slide : FALSE (BwayCharacter Failed Cast") );
		return false;
	}

	if (!CachedBwayMoveComp)
	{
		CachedBwayMoveComp = BwayCharacter ? Cast<UBwayCharacterMovementComponent>(BwayCharacter->GetBwayCharacterMovement()) : nullptr;
		if (!CachedBwayMoveComp)
		{
			UE_LOG(LogTemp, Warning, TEXT("CanUseAbility - Slide : FALSE (MovementComponent Failed Cast") );
			return false; // Requires the custom CMC
		}
	}

	// --- Check if Already Sliding ---
	if (CachedBwayMoveComp->IsSliding())
	{
		UE_LOG(LogTemp, Warning, TEXT("CanUseAbility - Slide: Condition Failed (Already Sliding)"));
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

	UE_LOG(LogTemp, Warning, TEXT("CanUseAbility - Slide : TRUE") );
	return true; // All conditions met
}

void UBwayGameplayAbility_Slide::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ABwayCharacterWithAbilities* BwayCharacter = Cast<ABwayCharacterWithAbilities>(ActorInfo->AvatarActor.Get());
	CachedBwayMoveComp = BwayCharacter? Cast<UBwayCharacterMovementComponent>(BwayCharacter->GetBwayCharacterMovement()) : nullptr;
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	UE_LOG(LogTemp, Warning, TEXT("ActivateAbility - Slide Start") );

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
		CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, true);
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
	CachedBwayMoveComp->SetMovementMode(MOVE_Custom, (uint8)ECustomMovementMode::CMOVE_Slide);
	UE_LOG(LogTemp, Warning, TEXT("ActivateAbility - Slide: Movement Mode Set") );

	// --- Wait for Input Release ---
	// Create and configure the task to wait for the input release
	UAbilityTask_WaitInputRelease* WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this);
	if (WaitInputReleaseTask)
	{
		// Bind the EndAbility function (or a custom function that calls EndAbility) to the OnRelease delegate
		WaitInputReleaseTask->OnRelease.AddDynamic(this, &UBwayGameplayAbility_Slide::OnInputRelease);
		WaitInputReleaseTask->ReadyForActivation(); // Start the task
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Slide Ability: Failed to create WaitInputRelease task. Ending ability."));
		bool bReplicateEndAbility = true;
		bool bWasCancelled = true; // Treat failure to create task as cancellation
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}
	
	// --- Apply State Tag ---
	// The SlidingStateTag (e.g., State.Movement.Sliding) is automatically applied
	// because it's listed in ActivationOwnedTags in the constructor.

	// Ability remains active while the CMC is in CMOVE_Slide.
	// The CMC is responsible for checking exit conditions (speed drop)
	// and calling SetMovementMode to exit the custom mode.
	// When the CMC exits CMOVE_Slide, OnMovementModeChanged will fire.
	// We don't strictly need a delegate here; the ability can end when the State.Movement.Sliding tag is removed
	// (which happens automatically when the ability ends) or be ended manually if needed.
	// For simplicity, let the ability stay active. It will be implicitly ended if another
	// ability cancels abilities with its tag, or explicitly ended if necessary.

}

// Add this new function to handle the release event
void UBwayGameplayAbility_Slide::OnInputRelease(float TimeHeld)
{
	// Input was released, end the ability
	bool bReplicateEndAbility = true;
	bool bWasCancelled = false; // Input release is not a cancellation
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBwayGameplayAbility_Slide::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Check if the pointer is still valid before using it
	if (CachedBwayMoveComp)
	{
		// Only change mode if we are *still* in slide mode when the ability ends.
		// This prevents overriding a mode change initiated by the CMC itself (e.g., due to low speed).
		if (CachedBwayMoveComp->IsCustomMovementMode((uint8)ECustomMovementMode::CMOVE_Slide))
		{
			UE_LOG(LogTemp, Warning, TEXT("EndAbility - Slide: Forcing exit from CMOVE_Slide"));
			// Determine appropriate default state (Falling or Walking)
			CachedBwayMoveComp->SetMovementMode(CachedBwayMoveComp->IsFalling() ? MOVE_Falling : MOVE_Walking);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("EndAbility - Slide: CMC already exited CMOVE_Slide."));
		}
	}
	else
	{
		// Attempt to get the component again if CachedBwayMoveComp was nullified somehow
		// (though ideally it shouldn't be null here if ActivateAbility succeeded)
		if (ActorInfo && ActorInfo->AvatarActor.IsValid())
		{
			ABwayCharacterWithAbilities* BwayCharacter = Cast<ABwayCharacterWithAbilities>(ActorInfo->AvatarActor.Get());
			UBwayCharacterMovementComponent* MoveComp = BwayCharacter ? Cast<UBwayCharacterMovementComponent>(BwayCharacter->GetBwayCharacterMovement()) : nullptr;
			if (MoveComp && MoveComp->IsCustomMovementMode((uint8)ECustomMovementMode::CMOVE_Slide))
			{
				UE_LOG(LogTemp, Warning, TEXT("EndAbility - Slide (Fallback): Forcing exit from CMOVE_Slide"));
				MoveComp->SetMovementMode(MoveComp->IsFalling() ? MOVE_Falling : MOVE_Walking);
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("EndAbility - Slide: Cleaning up. Cancelled: %s"), bWasCancelled ? TEXT("True") : TEXT("False"));

	// Nullify the cached pointer *before* calling Super::EndAbility,
	// as Super::EndAbility might destroy the ability instance.
	CachedBwayMoveComp = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	// ActivationOwnedTags (State.Movement.Sliding) are automatically removed by Super::EndAbility.
}