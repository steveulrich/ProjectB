#pragma once

#include "Abilities/BwayGameplayAbility_ArgusBase.h"
#include "BwayGameplayAbility_MeleePrimary.generated.h"

/**
 * Argus LMB — short forward melee trace.
 * Sheet: Base 10, Scaling 0.4 → Final = 10 + AttackStr * 0.4.
 * Blocked when relic carrier (via UBwayGameplayAbility_Base).
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_MeleePrimary : public UBwayGameplayAbility_ArgusBase
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

	/** Sheet Base Damage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float AbilityBaseDamage = 10.f;

	/** Sheet Scaling coefficient (Final = Base + AttackStr * Scaling) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float DamageScaling = 0.4f;
};
