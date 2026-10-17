#pragma once

#include "Abilities/BwayGameplayAbility.h"
#include "BwayGameplayAbility_RelicThrow.generated.h"

class ARelicActor;

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_RelicThrow : public UBwayGameplayAbility
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_RelicThrow(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnCarrierTagRemoved();

	TWeakObjectPtr<ARelicActor> ChargingRelic;
};
