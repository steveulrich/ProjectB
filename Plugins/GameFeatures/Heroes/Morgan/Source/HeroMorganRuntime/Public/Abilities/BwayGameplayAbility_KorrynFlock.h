#pragma once

#include "Abilities/BwayGameplayAbility_KorrynBase.h"
#include "Engine/EngineTypes.h"
#include "NativeGameplayTags.h"
#include "BwayGameplayAbility_KorrynFlock.generated.h"

HEROMORGANRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Korryn_Flock);

/**
 * Korryn F — Flock. 1.5s ethereal + damage immunity, pawn pass-through, hover flight.
 */
UCLASS()
class HEROMORGANRUNTIME_API UBwayGameplayAbility_KorrynFlock : public UBwayGameplayAbility_KorrynBase
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_KorrynFlock(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	void ApplyEtherealMovement();
	void RestoreMovement();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Flock", meta = (ClampMin = "0.05"))
	float FlockDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Flock", meta = (ClampMin = "0.0"))
	float HoverMaxSpeed = 600.f;

private:
	FTimerHandle EndFlockTimerHandle;

	bool bStoredOrientRotationToMovement = false;
	float StoredGravityScale = 1.f;
	float StoredMaxWalkSpeed = 600.f;
	EMovementMode StoredMovementMode = MOVE_Walking;
	uint8 StoredCustomMovementMode = 0;
	ECollisionResponse StoredPawnResponse = ECR_Block;
	bool bAppliedEthereal = false;
};
