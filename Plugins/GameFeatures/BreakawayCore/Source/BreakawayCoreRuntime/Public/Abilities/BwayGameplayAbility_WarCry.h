// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BwayCharacterWithAbilities.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayGameplayAbility_WarCry.generated.h"

class UGameplayEffect;

BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Spartacus_WarCry);

/**
 * War Cry Ability (E)
 * AOE buff for nearby allies (speed + damage)
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_WarCry : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_WarCry(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Get nearby allies within radius */
	TArray<ABwayCharacterWithAbilities*> GetNearbyAllies(float Radius) const;

	/** Apply buff to ally */
	void ApplyBuffToAlly(ABwayCharacterWithAbilities* Ally);

	/** Gameplay effect to apply buff */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "War Cry")
	TSubclassOf<UGameplayEffect> BuffGameplayEffectClass;

	/** AOE radius in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "War Cry", meta = (ClampMin = "0.0"))
	float BuffRadius = 1000.0f;

private:
	// No private members needed - using base class
};

