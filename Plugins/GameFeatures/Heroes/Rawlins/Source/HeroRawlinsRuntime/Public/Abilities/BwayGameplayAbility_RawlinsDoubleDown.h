#pragma once

#include "Abilities/BwayGameplayAbility_RawlinsBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_RawlinsDoubleDown.generated.h"

HERORAWLINSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Rawlins_DoubleDown);

/**
 * Rawlins F — Double Down (sheet "RMB" dodge column → in-game F / Ability4).
 * Invulnerable roll forward. CD 14s.
 */
UCLASS()
class HERORAWLINSRUNTIME_API UBwayGameplayAbility_RawlinsDoubleDown : public UBwayGameplayAbility_RawlinsBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_RawlinsDoubleDown(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual float GetKitCooldownSeconds(const UBwayRawlinsKitConfig& Config) const override
	{
		return Config.DoubleDownCooldown;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|DoubleDown", meta = (ClampMin = "0.0"))
	float DashDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|DoubleDown", meta = (ClampMin = "0.05"))
	float DashDuration = 0.35f;

private:
	FTimerHandle EndDashTimerHandle;
};
