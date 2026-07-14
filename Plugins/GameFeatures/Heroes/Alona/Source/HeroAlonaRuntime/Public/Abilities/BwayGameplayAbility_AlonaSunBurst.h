#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_AlonaSunBurst.generated.h"

HEROALONARUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Alona_SunBurst);

/**
 * Alona E — Sun Burst. Radial knockback around Alona. CD 20s.
 */
UCLASS()
class HEROALONARUNTIME_API UBwayGameplayAbility_AlonaSunBurst : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_AlonaSunBurst(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	void PerformBurst();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.0"))
	float BurstRadius = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 1400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.0"))
	float KnockbackUpward = 400.f;
};
