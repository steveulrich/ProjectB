#pragma once

#include "Abilities/BwayGameplayAbility_KorrynBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_KorrynCircleOfSpite.generated.h"

class ABwayKorrynSpiteZone;
class UGameplayEffect;

HEROMORGANRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Korryn_CircleOfSpite);

/**
 * Korryn E — spawn Circle of Spite ground zone (slow + damage amp).
 */
UCLASS()
class HEROMORGANRUNTIME_API UBwayGameplayAbility_KorrynCircleOfSpite : public UBwayGameplayAbility_KorrynBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_KorrynCircleOfSpite(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual float GetKitCooldownSeconds(const UBwayKorrynKitConfig& Config) const override
	{
		return Config.CircleCooldown;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle")
	TSubclassOf<ABwayKorrynSpiteZone> ZoneClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle")
	TSubclassOf<UGameplayEffect> SlowEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle")
	TSubclassOf<UGameplayEffect> DamageAmpEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle", meta = (ClampMin = "50.0"))
	float ZoneRadius = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle", meta = (ClampMin = "0.05"))
	float ZoneDuration = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle", meta = (ClampMin = "0.0"))
	float SpawnForwardOffset = 400.f;
};
