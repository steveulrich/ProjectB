#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_ArgusSlide.generated.h"

BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Argus_Slide);

/**
 * Argus F — Sheet "Slide" defense dodge.
 * Invulnerable dash in the held movement direction (falls back to facing). CD 18s.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_ArgusSlide : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_ArgusSlide(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Slide", meta = (ClampMin = "0.0"))
	float DashDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Slide", meta = (ClampMin = "0.05"))
	float DashDuration = 0.35f;

private:
	FTimerHandle EndDashTimerHandle;
};
