#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_AlonaRadiance.generated.h"

HEROALONARUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Alona_Radiance);

/**
 * Alona Q — Radiance. Heal ally closest to reticule for ActiveDuration. CD 8s.
 * 19a uses a flat heal placeholder; STR/52 scaling lands in 19c.
 */
UCLASS()
class HEROALONARUNTIME_API UBwayGameplayAbility_AlonaRadiance : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_AlonaRadiance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	ABwayCharacterWithAbilities* FindClosestAllyToReticule() const;

	UFUNCTION()
	void ApplyHealTick();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.1"))
	float ActiveDuration = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.05"))
	float HealTickInterval = 1.f;

	/** Flat placeholder heal per tick for 19a (parity scaling in 19c). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float HealPerTick = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float MaxTargetRange = 2500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float MaxRayDistance = 400.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<ABwayCharacterWithAbilities> LockedAlly;

	FTimerHandle HealTickTimerHandle;
	FTimerHandle EndAbilityTimerHandle;
};
