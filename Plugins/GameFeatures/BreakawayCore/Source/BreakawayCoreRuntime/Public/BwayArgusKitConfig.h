#pragma once

#include "Engine/DataAsset.h"
#include "BwayArgusKitConfig.generated.h"

/**
 * Data-driven Argus kit values for Step 20.5.
 * Seeded with sheet / existing ability defaults; Step 18c remains the formal parity audit.
 */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UBwayArgusKitConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BwayArgusKitConfig"), GetFName());
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// --- Primary ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Primary", meta = (ClampMin = "0.0"))
	float PrimaryBaseDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Primary", meta = (ClampMin = "0.0"))
	float PrimaryDamageScaling = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Primary", meta = (ClampMin = "0.0"))
	float PrimaryTraceRadius = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Primary", meta = (ClampMin = "0.0"))
	float PrimaryTraceDistance = 175.f;

	// --- Slide ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Slide", meta = (ClampMin = "0.0"))
	float SlideDashDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Slide", meta = (ClampMin = "0.05"))
	float SlideDashDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Slide", meta = (ClampMin = "0.1"))
	float SlideCooldown = 18.f;

	// --- No Retreat ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float NoRetreatChargeDistance = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.05"))
	float NoRetreatChargeDuration = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float NoRetreatTraceRadius = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float NoRetreatBaseDamage = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float NoRetreatDamageScaling = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float NoRetreatKnockbackStrength = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.0"))
	float NoRetreatKnockbackUpward = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|NoRetreat", meta = (ClampMin = "0.1"))
	float NoRetreatCooldown = 12.f;

	// --- For Glory ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float ForGloryTraceRadius = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float ForGloryTraceDistance = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float ForGloryBaseDamage = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float ForGloryDamageScaling = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float ForGloryKnockbackStrength = 1600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.0"))
	float ForGloryKnockbackUpward = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|ForGlory", meta = (ClampMin = "0.1"))
	float ForGloryCooldown = 25.f;

	// --- Retribution ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionTraceRadius = 90.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionTraceDistance = 220.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionUppercutBaseDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionDownSmashBaseDamage = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionDamageScaling = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionUppercutDelay = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionDownSmashDelay = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionUppercutKnockback = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionUppercutUpward = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionDownSmashKnockback = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.0"))
	float RetributionDownSmashDownward = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Argus|Retribution", meta = (ClampMin = "0.1"))
	float RetributionCooldown = 30.f;
};
