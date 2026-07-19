#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayRawlinsKitConfig.h"
#include "BwayGameplayAbility_RawlinsBase.generated.h"

/**
 * Shared Rawlins ability helpers — optional kit config soft ref overlays sheet defaults.
 */
UCLASS(Abstract)
class HERORAWLINSRUNTIME_API UBwayGameplayAbility_RawlinsBase : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_RawlinsBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	const UBwayRawlinsKitConfig* ResolveKitConfig() const;

	/** Returns kit cooldown for this ability, or <= 0 to use the native GE duration. */
	virtual float GetKitCooldownSeconds(const UBwayRawlinsKitConfig& Config) const { return -1.f; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Config")
	TSoftObjectPtr<UBwayRawlinsKitConfig> KitConfig;
};
