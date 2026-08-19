#pragma once

#include "Abilities/BwayGameplayAbility_RawlinsBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_RawlinsBlazingBarrage.generated.h"

class ABwayRawlinsBulletProjectile;
class UBwayProjectilePresentationData;
class ULyraHealthComponent;

HERORAWLINSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Rawlins_BlazingBarrage);

/**
 * Rawlins R — Blazing Barrage.
 * Stationary 12-shot burst; aimable while firing; juggles airborne enemies; interruptible. CD 25s.
 */
UCLASS()
class HERORAWLINSRUNTIME_API UBwayGameplayAbility_RawlinsBlazingBarrage : public UBwayGameplayAbility_RawlinsBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_RawlinsBlazingBarrage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual float GetKitCooldownSeconds(const UBwayRawlinsKitConfig& Config) const override
	{
		return Config.BarrageCooldown;
	}

	UFUNCTION()
	void FireNextShot();

	UFUNCTION()
	void HandleHealthChanged(ULyraHealthComponent* HealthComp, float OldValue, float NewValue, AActor* Instigator);

	void SpawnShot();
	void UnbindHealthInterrupt();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage")
	TSubclassOf<ABwayRawlinsBulletProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "1"))
	int32 ShotCount = 12;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.01"))
	float ShotInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "100.0"))
	float ProjectileSpeed = 3600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.1"))
	float ProjectileLifeSpan = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.0"))
	float SpawnForwardOffset = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.0"))
	float JuggleUpward = 350.f;

private:
	FTimerHandle ShotTimerHandle;
	int32 ShotsFired = 0;
	bool bBoundToHealth = false;
	float HealthAtActivation = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<ULyraHealthComponent> BoundHealthComponent;

	float ActiveBaseDamage = 4.f;
	float ActiveDamageScaling = 0.3f;
	int32 ActiveShotCount = 12;
	float ActiveShotInterval = 0.1f;
	float ActiveProjectileSpeed = 3600.f;
	float ActiveProjectileLifeSpan = 1.5f;
	float ActiveSpawnForwardOffset = 80.f;
	float ActiveJuggleUpward = 350.f;
	TSoftObjectPtr<UBwayProjectilePresentationData> ActiveProjectilePresentation;
};
