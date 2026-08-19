// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BwayMovementFeelConfig.generated.h"

/**
 * Data-driven movement feel tuning (slide-jump, etc.).
 * Register primary asset type "BwayMovementFeelConfig" under /BreakawayCore/Movement.
 */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UBwayMovementFeelConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Multiplier applied to horizontal velocity when jumping out of a slide. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide Jump", meta = (ClampMin = "1.0"))
	float SlideJumpMomentumBoost = 1.2f;

	/** Minimum horizontal speed after a successful slide-jump (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide Jump", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float SlideJumpMinHorizontalSpeed = 400.0f;

	/** Maximum horizontal speed after a successful slide-jump (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide Jump", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float SlideJumpMaxHorizontalSpeed = 2000.0f;

	/** Absolute AirControl used while slide-jumping (normal jumps keep CMC AirControl). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide Jump", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlideJumpAirControl = 0.15f;

	/** GravityScale applied only while bIsSlideJumping is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide Jump", meta = (ClampMin = "0.0"))
	float SlideJumpGravityScale = 1.0f;

	/** Multiplier applied to horizontal velocity when landing from a slide-jump. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide Jump", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlideJumpLandedVelocityFactor = 0.5f;

	/** If true, keep slide-jump state until landing (or hard mode interrupt). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide Jump")
	bool bPreserveSlideJumpUntilLanding = true;
};
