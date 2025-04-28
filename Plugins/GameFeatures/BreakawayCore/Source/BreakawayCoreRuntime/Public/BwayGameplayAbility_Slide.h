// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BwayCharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "InputAction.h"
#include "BwayGameplayAbility_Slide.generated.h"


class UGameplayEffect;
class UInputAction;
class ULyraCharacterMovementComponent_Slide;

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_Slide : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_Slide(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                     FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Tag required on the character to initiate the slide (e.g., State.Movement.Sprinting). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Slide Trigger")
	FGameplayTag RequiredStateTag; // Assign State.Movement.Sprinting in BP

	/** Tag representing the sliding state, applied by this ability via ActivationOwnedTags. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Slide Trigger")
	FGameplayTag SlidingStateTag; // Assign State.Movement.Sliding in BP

	/** Input Action for Slide (needed for Hold check if IA configured for Press/Release) */
	UPROPERTY(EditDefaultsOnly, Category = "Lyra|Slide Trigger")
	TObjectPtr<const UInputAction> SlideInputAction; // Assign IA_Slide in BP

private:
	UPROPERTY(Transient)
	TObjectPtr<UBwayCharacterMovementComponent> CachedBwayMoveComp;
};