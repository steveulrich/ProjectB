// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"
#include "BwayNameplatePolicyComponent.generated.h"

class UBwayCombatReadabilityConfig;
class UIndicatorDescriptor;
class ULyraIndicatorManagerComponent;

/**
 * Local policy layer over Lyra's indicator/nameplate system.
 * Enforces both-team visibility, self-hide, max range, LOS, and distance scale.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayNameplatePolicyComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UBwayNameplatePolicyComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void RefreshConfig();
	void EvaluateIndicators();
	bool HasLineOfSightTo(const FVector& ViewLocation, const FVector& TargetLocation, const AActor* TargetActor) const;

	UPROPERTY(Transient)
	TObjectPtr<const UBwayCombatReadabilityConfig> CachedConfig;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULyraIndicatorManagerComponent> IndicatorManager;

	float LOSAccumulatedTime = 0.f;
	int32 NextLOSTraceIndex = 0;

	/** Cached LOS results keyed by indicator object pointer. */
	TMap<TObjectKey<UIndicatorDescriptor>, bool> CachedLOSByIndicator;
};
