// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "Feedback/NumberPops/LyraDamagePopStyle.h"
#include "GameplayTagContainer.h"
#include "BwayCombatReadabilityConfig.generated.h"

class ULyraDamagePopStyle;

/**
 * Data-driven combat readability: floating numbers, nameplate range/LOS/scale.
 * Loaded after experience activation via UBwayGameFeatureAction_CombatReadabilityConfig.
 */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UBwayCombatReadabilityConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BwayCombatReadabilityConfig"), GetFName());
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// --- Floating combat numbers ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers")
	FLinearColor IncomingDamageColor = FLinearColor(1.f, 0.15f, 0.1f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers")
	FLinearColor OutgoingDamageColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers")
	FLinearColor IncomingHealColor = FLinearColor(0.2f, 1.f, 0.35f);

	/** World-space offset applied above the target capsule when spawning a number. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers")
	FVector NumberWorldOffset = FVector(0.f, 0.f, 90.f);

	/** Seconds a floating number stays visible and scales down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers", meta = (ClampMin = "0.15"))
	float NumberPopLifespan = 1.0f;

	/** World-Z rise over the number's lifespan (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers", meta = (ClampMin = "0.0"))
	float NumberPopRiseDistance = 80.f;

	/** Camera-relative left/right start offset so consecutive pops do not spawn stacked (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers", meta = (ClampMin = "0.0"))
	float NumberPopLateralOffset = 35.f;

	/** Extra camera-relative outward travel over the lifespan (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers", meta = (ClampMin = "0.0"))
	float NumberPopOutwardDistance = 45.f;

	/** World-size multiplier at the end of the lifespan (1 = no shrink, 0 = shrink away). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NumberPopEndScale = 0.f;

	/** Base world size of the floating number text (scaled up with camera distance). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers", meta = (ClampMin = "8.0"))
	float NumberPopWorldSize = 72.f;

	/** Mesh-text styles matched via TargetTags on FLyraNumberPopRequest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers")
	TArray<TObjectPtr<ULyraDamagePopStyle>> NumberPopStyles;

	// --- Nameplates ---

	/** Maximum camera→target distance at which nameplates remain visible (uu). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates", meta = (ClampMin = "100.0"))
	float MaxNameplateRange = 6000.f;

	/** Distance at which scale curve input is 0. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates", meta = (ClampMin = "0.0"))
	float NameplateScaleDistanceStart = 500.f;

	/** Distance at which scale curve input is 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates", meta = (ClampMin = "100.0"))
	float NameplateScaleDistanceEnd = 4500.f;

	/** Maps normalized distance [0,1] → screen scale. If unset, uses Min/MaxScale lerp. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates")
	TObjectPtr<UCurveFloat> NameplateScaleCurve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates", meta = (ClampMin = "0.1"))
	float MinNameplateScale = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates", meta = (ClampMin = "0.1"))
	float MaxNameplateScale = 1.0f;

	/** Seconds between LOS traces per tracked nameplate (batched). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates|LOS", meta = (ClampMin = "0.05"))
	float NameplateLOSTraceInterval = 0.15f;

	/** Max LOS traces issued per tick across all nameplates. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates|LOS", meta = (ClampMin = "1"))
	int32 MaxNameplateLOSTracesPerTick = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates|LOS")
	TEnumAsByte<ECollisionChannel> NameplateLOSTraceChannel = ECC_Visibility;

	/**
	 * World Z added to the indicator attach point for LOS traces.
	 * NameplateManager usually attaches at feet/root; without this offset the floor
	 * blocks Visibility traces and every plate stays hidden.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates|LOS", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float NameplateLOSVerticalOffset = 90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates|LOS")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nameplates")
	bool bHideLocalPlayerNameplate = true;

	// --- Accessibility / global feedback ---

	/** Multiplier applied on top of EvaluateNameplateScale (and number pop distance feel). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accessibility", meta = (ClampMin = "0.25", ClampMax = "2.0"))
	float CombatFeedbackScaleMultiplier = 1.0f;

	/** When true, camera shakes from UGCN_BwayCombatFeedback are skipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accessibility")
	bool bReduceCameraShake = false;

	/** When true, optional controller haptics from combat cues are skipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accessibility")
	bool bReduceMotionHaptics = false;

	/** Optional colorblind-friendly overrides (used when bUseColorblindPalette is true). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accessibility")
	bool bUseColorblindPalette = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accessibility", meta = (EditCondition = "bUseColorblindPalette"))
	FLinearColor ColorblindIncomingDamageColor = FLinearColor(0.95f, 0.55f, 0.1f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accessibility", meta = (EditCondition = "bUseColorblindPalette"))
	FLinearColor ColorblindOutgoingDamageColor = FLinearColor(0.85f, 0.85f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Accessibility", meta = (EditCondition = "bUseColorblindPalette"))
	FLinearColor ColorblindIncomingHealColor = FLinearColor(0.2f, 0.75f, 1.0f);

	/** Gameplay cue executed for the attacker on successful enemy damage (hit confirm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Numbers|Cues")
	FGameplayTag HitConfirmationCueTag;

	/** Evaluates scale for a camera distance using curve or min/max lerp. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Nameplates")
	float EvaluateNameplateScale(float Distance) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat Numbers")
	FLinearColor ResolveIncomingDamageColor() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat Numbers")
	FLinearColor ResolveOutgoingDamageColor() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat Numbers")
	FLinearColor ResolveIncomingHealColor() const;
};
