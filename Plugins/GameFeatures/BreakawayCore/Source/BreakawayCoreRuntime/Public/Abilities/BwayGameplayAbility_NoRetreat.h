#pragma once

#include "Abilities/BwayGameplayAbility_ArgusBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_NoRetreat.generated.h"

BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Argus_NoRetreat);

/**
 * Argus Q — No Retreat.
 * Straight-line shoulder charge; damages + knocks back first enemy hit. CD 12s. Damage 2 / 0.4.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_NoRetreat : public UBwayGameplayAbility_ArgusBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_NoRetreat(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual float GetKitCooldownSeconds(const UBwayArgusKitConfig& Config) const override
	{
		return Config.NoRetreatCooldown;
	}

	UFUNCTION()
	void PerformChargeTrace();

	void ApplyHitToEnemy(ABwayCharacterWithAbilities* Enemy);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float ChargeDistance = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.05"))
	float ChargeDuration = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float TraceRadius = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float KnockbackUpward = 250.f;

private:
	FTimerHandle ChargeTraceTimerHandle;
	FTimerHandle EndChargeTimerHandle;
	FVector ChargeDirection = FVector::ForwardVector;
	bool bHasHitEnemy = false;
	static constexpr float ChargeTraceInterval = 0.016f;

	float ActiveChargeDistance = 1000.f;
	float ActiveChargeDuration = 0.4f;
	float ActiveTraceRadius = 120.f;
	float ActiveBaseDamage = 2.f;
	float ActiveDamageScaling = 0.4f;
	float ActiveKnockbackStrength = 1200.f;
	float ActiveKnockbackUpward = 250.f;
};
