#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_Retribution.generated.h"

BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Argus_Retribution);

/**
 * Argus R — Retribution.
 * Uppercut then down smash (two hits). CD 30s. Damage 10+20 / 0.5.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_Retribution : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_Retribution(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

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
};
