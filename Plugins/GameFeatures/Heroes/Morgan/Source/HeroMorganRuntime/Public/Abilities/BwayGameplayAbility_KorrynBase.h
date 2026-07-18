#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayKorrynKitConfig.h"
#include "BwayGameplayAbility_KorrynBase.generated.h"

/**
 * Shared Korryn ability helpers — optional kit config soft ref overlays sheet defaults.
 */
UCLASS(Abstract)
class HEROMORGANRUNTIME_API UBwayGameplayAbility_KorrynBase : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_KorrynBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	const UBwayKorrynKitConfig* ResolveKitConfig() const;

	/** Returns kit cooldown for this ability, or <= 0 to use the native GE duration. */
	virtual float GetKitCooldownSeconds(const UBwayKorrynKitConfig& Config) const { return -1.f; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Config")
	TSoftObjectPtr<UBwayKorrynKitConfig> KitConfig;
};
