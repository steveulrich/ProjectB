#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_AlonaPrimary.generated.h"

class ABwayAlonaPrimaryProjectile;

HEROALONARUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Alona_PrimaryAttack);

/**
 * Alona LMB — light projectile. Sheet: Base 28, Scaling 0.25.
 */
UCLASS()
class HEROALONARUNTIME_API UBwayGameplayAbility_AlonaPrimary : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_AlonaPrimary(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary")
	TSubclassOf<ABwayAlonaPrimaryProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 28.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary", meta = (ClampMin = "100.0"))
	float ProjectileSpeed = 3200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary", meta = (ClampMin = "0.1"))
	float ProjectileLifeSpan = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary", meta = (ClampMin = "0.0"))
	float SpawnForwardOffset = 80.f;
};
