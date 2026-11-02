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

	/** Base speed value used for slide entry and slope scaling (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float BaseSlideSpeed = 600.0f;

	/** Factor multiplied by BaseSlideSpeed to determine maximum downhill slide speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0.0"))
	float SlideMaxSpeedFactor = 3.0f;

	/** Factor multiplied by BaseSlideSpeed to determine downward slope acceleration magnitude. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0.0"))
	float SlideSlopeAccelerationFactor = 2.4f;

	/** Base flat-ground friction factor (1/s) before slope power scaling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0.0"))
	float SlideBaseFrictionFactor = 0.80f;

	/** Exponent used in the slope-based friction calculation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "0.1"))
	float SlideFrictionPower = 4.0f;

	/** Multiplier applied to SlideBaseFrictionFactor when sliding uphill against a slope. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide", meta = (ClampMin = "1.0"))
	float SlideUphillFrictionMultiplier = 3.5f;

	/** Maximum FOV offset added at peak slide speed (degrees). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide|Juice", meta = (ClampMin = "0.0", ForceUnits = "degrees"))
	float SlideFOVOffsetMax = 12.0f;

	/** Interpolation speed for slide camera FOV offset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slide|Juice", meta = (ClampMin = "0.0"))
	float SlideFOVInterpSpeed = 6.0f;

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
