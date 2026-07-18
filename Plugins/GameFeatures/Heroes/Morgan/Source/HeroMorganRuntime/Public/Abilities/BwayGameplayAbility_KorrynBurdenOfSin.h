#pragma once

#include "Abilities/BwayGameplayAbility_KorrynBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_KorrynBurdenOfSin.generated.h"

class UGameplayEffect;

HEROMORGANRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Korryn_BurdenOfSin);

/**
 * Korryn Q — forward raven cone: damage + timed slow.
 */
UCLASS()
class HEROMORGANRUNTIME_API UBwayGameplayAbility_KorrynBurdenOfSin : public UBwayGameplayAbility_KorrynBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_KorrynBurdenOfSin(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual float GetKitCooldownSeconds(const UBwayKorrynKitConfig& Config) const override
	{
		return Config.BurdenCooldown;
	}

	TArray<ABwayCharacterWithAbilities*> FindEnemiesInCone(const FVector& Origin, const FVector& Forward) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden")
	TSubclassOf<UGameplayEffect> SlowEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 33.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "50.0"))
	float ConeRange = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "5.0", ClampMax = "180.0"))
	float ConeHalfAngleDegrees = 35.f;
};
