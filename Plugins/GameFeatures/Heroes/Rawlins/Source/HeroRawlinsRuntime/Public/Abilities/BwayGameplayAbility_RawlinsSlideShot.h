#pragma once

#include "Abilities/BwayGameplayAbility_RawlinsBase.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_RawlinsSlideShot.generated.h"

HERORAWLINSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Rawlins_SlideShot);

/**
 * Rawlins E — Slide Shot.
 * Slides forward quickly and launches opponents into the air. CD 18s. Damage 10 / 0.6.
 */
UCLASS()
class HERORAWLINSRUNTIME_API UBwayGameplayAbility_RawlinsSlideShot : public UBwayGameplayAbility_RawlinsBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_RawlinsSlideShot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual float GetKitCooldownSeconds(const UBwayRawlinsKitConfig& Config) const override
	{
		return Config.SlideShotCooldown;
	}

	UFUNCTION()
	void PerformSlideTrace();

	void ApplyHitToEnemy(ABwayCharacterWithAbilities* Enemy);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float SlideDistance = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.05"))
	float SlideDuration = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float TraceRadius = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float AbilityBaseDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float DamageScaling = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float LaunchStrength = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float LaunchUpward = 900.f;

private:
	FTimerHandle SlideTraceTimerHandle;
	FTimerHandle EndSlideTimerHandle;
	FVector SlideDirection = FVector::ForwardVector;
	TSet<TObjectPtr<ABwayCharacterWithAbilities>> HitEnemies;
	static constexpr float SlideTraceInterval = 0.016f;

	float ActiveSlideDistance = 1000.f;
	float ActiveSlideDuration = 0.4f;
	float ActiveTraceRadius = 120.f;
	float ActiveBaseDamage = 10.f;
	float ActiveDamageScaling = 0.6f;
	float ActiveLaunchStrength = 400.f;
	float ActiveLaunchUpward = 900.f;
};
