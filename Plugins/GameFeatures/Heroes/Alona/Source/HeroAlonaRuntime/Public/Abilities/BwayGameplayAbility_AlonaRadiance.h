#pragma once

#include "Abilities/BwayGameplayAbility_AlonaBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_AlonaRadiance.generated.h"

HEROALONARUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Alona_Radiance);

/**
 * Alona Q — Radiance. Heal ally closest to reticule for ActiveDuration. CD 8s.
 * Sheet heal formula: BaseHealPerTick * clamp(AttackStrength / 52, 0, 1.75).
 */
UCLASS()
class HEROALONARUNTIME_API UBwayGameplayAbility_AlonaRadiance : public UBwayGameplayAbility_AlonaBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_AlonaRadiance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual float GetKitCooldownSeconds(const UBwayAlonaKitConfig& Config) const override
	{
		return Config.RadianceCooldown;
	}

	ABwayCharacterWithAbilities* FindClosestAllyToReticule() const;

	UFUNCTION()
	void ApplyHealTick();

	float CalculateScaledHealPerTick(float& OutAttackStrength, float& OutHealMultiplier) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.1"))
	float ActiveDuration = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.05"))
	float HealTickInterval = 1.f;

	/** Base heal before the sheet STR multiplier. With a one-second interval this is also base HP/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float HealPerTick = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.01"))
	float StrengthDivisor = 52.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float MaxHealMultiplier = 1.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float MaxTargetRange = 2500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float MaxRayDistance = 400.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<ABwayCharacterWithAbilities> LockedAlly;

	FTimerHandle HealTickTimerHandle;
	FTimerHandle EndAbilityTimerHandle;
	float ResolvedHealPerTick = 0.f;
	int32 RemainingHealTicks = 0;

	float ActiveMaxTargetRange = 2500.f;
	float ActiveMaxRayDistance = 400.f;
};
