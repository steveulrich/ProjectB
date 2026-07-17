#pragma once

#include "Abilities/BwayGameplayAbility_KorrynBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_KorrynPrimary.generated.h"

class ABwayKorrynPrimaryProjectile;
class UGameplayEffect;

HEROMORGANRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Korryn_PrimaryAttack);

UCLASS()
class HEROMORGANRUNTIME_API UBwayGameplayAbility_KorrynPrimary : public UBwayGameplayAbility_KorrynBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_KorrynPrimary(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary")
	TSubclassOf<ABwayKorrynPrimaryProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary")
	TSubclassOf<UGameplayEffect> ArmorShredEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 22.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "100.0"))
	float ProjectileSpeed = 2800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "0.1"))
	float ProjectileLifeSpan = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "0.0"))
	float SpawnForwardOffset = 80.f;
};
