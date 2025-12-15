// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BwayCharacterWithAbilities.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayGameplayAbility_DefensiveStance.generated.h"

class UGameplayEffect;

BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_DefensiveStance);
BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Spartacus_DefensiveStance);

/**
 * Defensive Stance Ability (F)
 * Toggle ability that reduces incoming damage and slows movement
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_DefensiveStance : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_DefensiveStance(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Toggle stance on/off */
	void ToggleStance();

	/** Apply defensive stance effect */
	void ApplyStanceEffect();

	/** Remove defensive stance effect */
	void RemoveStanceEffect();

	/** Tag representing the defensive stance state */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defensive Stance")
	FGameplayTag DefensiveStanceStateTag;

	/** Gameplay effect for defensive stance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defensive Stance")
	TSubclassOf<UGameplayEffect> StanceGameplayEffectClass;

private:
	/** Handle to the active stance effect */
	FActiveGameplayEffectHandle StanceEffectHandle;

	/** Whether stance is currently active */
	bool bStanceActive = false;
};

