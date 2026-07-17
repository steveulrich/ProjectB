#pragma once

#include "Abilities/BwayGameplayAbility_KorrynBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_KorrynAuraOfSilence.generated.h"

class UGameplayEffect;

HEROMORGANRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Korryn_AuraOfSilence);

/**
 * Korryn R — radial damage + timed silence on enemies.
 */
UCLASS()
class HEROMORGANRUNTIME_API UBwayGameplayAbility_KorrynAuraOfSilence : public UBwayGameplayAbility_KorrynBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_KorrynAuraOfSilence(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Aura")
	TSubclassOf<UGameplayEffect> SilenceEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Aura", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 13.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Aura", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Aura", meta = (ClampMin = "50.0"))
	float AuraRadius = 800.f;
};
