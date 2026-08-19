#pragma once

#include "Abilities/BwayGameplayAbility_RawlinsBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_RawlinsPrimary.generated.h"

class ABwayRawlinsBulletProjectile;
class UBwayProjectilePresentationData;

HERORAWLINSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Rawlins_PrimaryAttack);

/**
 * Rawlins LMB — two consecutive pistol shots. Sheet: Base 1, Scaling 0.3 each.
 */
UCLASS()
class HERORAWLINSRUNTIME_API UBwayGameplayAbility_RawlinsPrimary : public UBwayGameplayAbility_RawlinsBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_RawlinsPrimary(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void FireNextShot();

	void SpawnShot();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary")
	TSubclassOf<ABwayRawlinsBulletProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "100.0"))
	float ProjectileSpeed = 3600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "0.1"))
	float ProjectileLifeSpan = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "0.0"))
	float SpawnForwardOffset = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "1"))
	int32 ShotCount = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "0.01"))
	float ShotInterval = 0.12f;

private:
	FTimerHandle ShotTimerHandle;
	int32 ShotsFired = 0;
	float ActiveBaseDamage = 1.f;
	float ActiveDamageScaling = 0.3f;
	float ActiveProjectileSpeed = 3600.f;
	float ActiveProjectileLifeSpan = 1.5f;
	float ActiveSpawnForwardOffset = 80.f;
	int32 ActiveShotCount = 2;
	float ActiveShotInterval = 0.12f;
	TSoftObjectPtr<UBwayProjectilePresentationData> ActiveProjectilePresentation;
};
