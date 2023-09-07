// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Interaction/InteractionQuery.h"
#include "Interaction/Tasks/AbilityTask_WaitForInteractableTargets.h"

#include "AbilityTask_WaitInteract_Sphere.generated.h"

struct FCollisionProfileName;

class UGameplayAbility;
class UObject;
struct FFrame;

UCLASS()
class UAbilityTask_WaitInteract_Sphere : public UAbilityTask_WaitForInteractableTargets
{
	GENERATED_UCLASS_BODY()

	virtual void Activate() override;

	/** Wait until we trace new set of interactables through Sphere Trace.  This task automatically loops. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitInteract_Sphere* WaitInteract_Sphere(UGameplayAbility* OwningAbility, FInteractionQuery InteractionQuery, FCollisionProfileName TraceProfile, FGameplayAbilityTargetingLocationInfo StartLocation, float InteractionScanRange = 100, float InteractionScanRate = 0.100, bool bShowDebug = false);

private:

	virtual void OnDestroy(bool AbilityEnded) override;

	void PerformTrace();

	UPROPERTY()
	FInteractionQuery InteractionQuery;

	UPROPERTY()
	FGameplayAbilityTargetingLocationInfo StartLocation;

	float InteractionScanRange = 100;
	float InteractionScanRate = 0.100;
	bool bShowDebug = false;

	FTimerHandle TimerHandle;
};