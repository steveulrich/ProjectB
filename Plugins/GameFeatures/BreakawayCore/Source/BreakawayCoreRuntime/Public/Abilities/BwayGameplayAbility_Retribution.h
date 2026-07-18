#pragma once

#include "Abilities/BwayGameplayAbility_ArgusBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_Retribution.generated.h"

BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Argus_Retribution);

/**
 * Argus R — Retribution.
 * Uppercut then down smash (two hits). CD 30s. Damage 10+20 / 0.5.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_Retribution : public UBwayGameplayAbility_ArgusBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_Retribution(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual float GetKitCooldownSeconds(const UBwayArgusKitConfig& Config) const override
	{
		return Config.RetributionCooldown;
	}

	UFUNCTION()
	void PerformUppercut();

	UFUNCTION()
	void PerformDownSmash();

	void ApplyMeleeHit(float AbilityBaseDamage, float Scaling, float KnockbackStrength, float KnockbackUpward);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float TraceRadius = 90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float TraceDistance = 220.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float UppercutBaseDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float DownSmashBaseDamage = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float UppercutDelay = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float DownSmashDelay = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float UppercutKnockback = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float UppercutUpward = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float DownSmashKnockback = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float DownSmashDownward = 400.f;

private:
	FTimerHandle UppercutTimerHandle;
	FTimerHandle DownSmashTimerHandle;
	FTimerHandle EndAbilityTimerHandle;

	float ActiveTraceRadius = 90.f;
	float ActiveTraceDistance = 220.f;
	float ActiveUppercutBaseDamage = 10.f;
	float ActiveDownSmashBaseDamage = 20.f;
	float ActiveDamageScaling = 0.5f;
	float ActiveUppercutKnockback = 400.f;
	float ActiveUppercutUpward = 700.f;
	float ActiveDownSmashKnockback = 900.f;
	float ActiveDownSmashDownward = 400.f;
};
