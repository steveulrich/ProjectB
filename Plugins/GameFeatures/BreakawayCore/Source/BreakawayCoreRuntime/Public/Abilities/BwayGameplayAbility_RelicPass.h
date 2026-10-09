#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayGameplayAbility_RelicPass.generated.h"

/**
 * Carrier pass — selects the best living allied teammate in the carrier's
 * view cone and asks the authoritative relic to pass toward that teammate.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_RelicPass : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_RelicPass(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic|Pass")
	float MaxPassTargetRange = 1600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic|Pass", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float PassTargetConeDegrees = 70.0f;
};
