#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayGameplayAbility_MeleePrimary.generated.h"

/**
 * Argus LMB — short forward melee trace. Blocked when relic carrier (via UBwayGameplayAbility_Base).
 * Cooldown/damage tuned in Blueprint subclass + GE (18c parity).
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_MeleePrimary : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_MeleePrimary(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float TraceRadius = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float TraceDistance = 175.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float DamageAmount = 10.f;
};
