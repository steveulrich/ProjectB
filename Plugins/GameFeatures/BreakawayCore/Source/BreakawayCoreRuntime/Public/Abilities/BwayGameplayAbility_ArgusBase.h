#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayArgusKitConfig.h"
#include "BwayGameplayAbility_ArgusBase.generated.h"

/**
 * Shared Argus ability helpers — optional kit config soft ref overlays sheet defaults.
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_ArgusBase : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_ArgusBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	const UBwayArgusKitConfig* ResolveKitConfig() const;

	/** Returns kit cooldown for this ability, or <= 0 to use the native GE duration. */
	virtual float GetKitCooldownSeconds(const UBwayArgusKitConfig& Config) const { return -1.f; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Config")
	TSoftObjectPtr<UBwayArgusKitConfig> KitConfig;
};
