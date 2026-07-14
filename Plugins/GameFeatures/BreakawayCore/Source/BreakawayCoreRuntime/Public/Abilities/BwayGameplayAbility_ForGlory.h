#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_ForGlory.generated.h"

BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Argus_ForGlory);

/**
 * Argus E — For Glory.
 * Unstoppable kick that damages and knocks the opponent backwards. CD 25s. Damage 2 / 0.4.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_ForGlory : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_ForGlory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	void PerformKick();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float TraceRadius = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float TraceDistance = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 1600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float KnockbackUpward = 350.f;
};
