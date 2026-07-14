#pragma once

#include "Abilities/BwayGameplayAbility_Base.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_AlonaSunsGrace.generated.h"

HEROALONARUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Alona_SunsGrace);

/**
 * Alona F — Sun's Grace. Invulnerable short-range teleport dodge. CD 22s.
 */
UCLASS()
class HEROALONARUNTIME_API UBwayGameplayAbility_AlonaSunsGrace : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_AlonaSunsGrace(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	FVector ResolveTeleportDirection() const;
	FVector ResolveTeleportDestination(const FVector& Direction) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunsGrace", meta = (ClampMin = "0.0"))
	float TeleportDistance = 750.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunsGrace", meta = (ClampMin = "0.05"))
	float InvulnerabilityDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunsGrace", meta = (ClampMin = "0.0"))
	float FloorTraceUp = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunsGrace", meta = (ClampMin = "0.0"))
	float FloorTraceDown = 400.f;

private:
	FTimerHandle EndInvulnTimerHandle;
};
