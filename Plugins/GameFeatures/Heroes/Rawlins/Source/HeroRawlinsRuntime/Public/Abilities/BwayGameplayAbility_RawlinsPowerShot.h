#pragma once

#include "Abilities/BwayGameplayAbility_RawlinsBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_RawlinsPowerShot.generated.h"

HERORAWLINSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Rawlins_PowerShot);

/**
 * Rawlins Q — Power Shot.
 * Dual pistol blast that knocks opponents back. CD 8s. Damage 30 / 0.65.
 */
UCLASS()
class HERORAWLINSRUNTIME_API UBwayGameplayAbility_RawlinsPowerShot : public UBwayGameplayAbility_RawlinsBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_RawlinsPowerShot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual float GetKitCooldownSeconds(const UBwayRawlinsKitConfig& Config) const override
	{
		return Config.PowerShotCooldown;
	}

	void PerformBlast();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float TraceRadius = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float TraceDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 1600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float KnockbackUpward = 250.f;
};
