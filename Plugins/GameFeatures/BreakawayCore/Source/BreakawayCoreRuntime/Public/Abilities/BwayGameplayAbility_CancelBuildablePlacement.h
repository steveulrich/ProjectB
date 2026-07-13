#pragma once

#include "CoreMinimal.h"
#include "Abilities/BwayGameplayAbility.h"
#include "BwayGameplayAbility_CancelBuildablePlacement.generated.h"

/**
 * Forwards RMB input to the active buildable placement targeting session (LocalInputCancel).
 * Granted on the humanoid ability set with InputTag.Ability.Buildable.Cancel.
 * Does not spawn buildables — PlaceBuildable + BwayBuildablePlacementLibrary own spawn.
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_CancelBuildablePlacement : public UBwayGameplayAbility
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_CancelBuildablePlacement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
