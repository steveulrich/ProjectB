#pragma once

#include "CoreMinimal.h"
#include "Abilities/BwayGameplayAbility.h"
#include "BwayGameplayAbility_ConfirmBuildablePlacement.generated.h"

/**
 * Forwards LMB input to the active buildable placement targeting session (LocalInputConfirm).
 * Granted on the humanoid ability set with InputTag.Ability.Buildable.Confirm.
 * Does not spawn buildables — PlaceBuildable + BwayBuildablePlacementLibrary own spawn.
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_ConfirmBuildablePlacement : public UBwayGameplayAbility
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_ConfirmBuildablePlacement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
