// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayCharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayGameplayAbility_GladiatorsLeap.generated.h"

class UGameplayEffect;
class UAbilityTask_WaitGameplayEvent;

BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Leaping);
BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Spartacus_GladiatorsLeap);

/**
 * Gladiator's Leap Ability (R - Ultimate)
 * Leap to target location and deal AOE damage on landing
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_GladiatorsLeap : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_GladiatorsLeap(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Get target location using ground trace */
	FVector GetTargetLocation() const;

	/** Perform leap to target location */
	void PerformLeap(const FVector& TargetLocation);

	/** Called when character lands */
	UFUNCTION()
	void OnLanding();

	/** Perform AOE damage at landing location */
	void PerformAOEDamage(const FVector& Location);

	/** Tag representing the leaping state */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gladiator's Leap")
	FGameplayTag LeapingStateTag;

	/** Gameplay effect for AOE damage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gladiator's Leap")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	/** Max leap distance in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gladiator's Leap", meta = (ClampMin = "0.0"))
	float MaxLeapDistance = 1500.0f;

	/** AOE damage radius in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gladiator's Leap", meta = (ClampMin = "0.0"))
	float DamageRadius = 500.0f;

	/** AOE damage amount */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gladiator's Leap", meta = (ClampMin = "0.0"))
	float DamageAmount = 100.0f;

	/** Ground trace distance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gladiator's Leap", meta = (ClampMin = "0.0"))
	float GroundTraceDistance = 5000.0f;

private:
	UPROPERTY(Transient)
	mutable TObjectPtr<UBwayCharacterMovementComponent> CachedMovementComponent;

	/** Whether we're currently leaping */
	bool bIsLeaping = false;

	/** Landing location */
	FVector LandingLocation;
};

