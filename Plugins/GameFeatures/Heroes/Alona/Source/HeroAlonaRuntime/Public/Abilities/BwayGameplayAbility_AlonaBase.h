#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayAlonaKitConfig.h"
#include "BwayGameplayAbility_AlonaBase.generated.h"

/**
 * Shared Alona ability helpers — optional kit config soft ref overlays sheet defaults.
 */
UCLASS(Abstract)
class HEROALONARUNTIME_API UBwayGameplayAbility_AlonaBase : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_AlonaBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	const UBwayAlonaKitConfig* ResolveKitConfig() const;

	/** Returns kit cooldown for this ability, or <= 0 to use the native GE duration. */
	virtual float GetKitCooldownSeconds(const UBwayAlonaKitConfig& Config) const { return -1.f; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Config")
	TSoftObjectPtr<UBwayAlonaKitConfig> KitConfig;
};
