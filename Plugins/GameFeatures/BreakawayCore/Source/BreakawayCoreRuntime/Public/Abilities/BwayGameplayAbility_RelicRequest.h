#pragma once

#include "CoreMinimal.h"
#include "Abilities/BwayGameplayAbility.h"
#include "BwayGameplayAbility_RelicRequest.generated.h"

class URelicSettings;

/**
 * Common Request Relic ability (InputTag.Ability.RelicRequest / RMB).
 * Applies RelicSettings::RequestingGameplayEffectClass so the character can pick up the relic.
 * GE class and requesting tag come from match RelicSettings — not hardcoded asset paths.
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_RelicRequest : public UBwayGameplayAbility
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_RelicRequest(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Active relic settings, else RelicManager RelicSettingsAsset. */
	const URelicSettings* ResolveRelicSettings() const;
};
