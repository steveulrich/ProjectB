#pragma once

#include "Engine/DataAsset.h"
#include "BwayAlonaKitConfig.generated.h"

class UBwayProjectilePresentationData;

/**
 * Data-driven Alona kit values for Step 20.5.
 * Seeded with sheet / existing ability defaults; Step 19c remains the formal parity audit.
 */
UCLASS(BlueprintType)
class HEROALONARUNTIME_API UBwayAlonaKitConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BwayAlonaKitConfig"), GetFName());
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// --- Primary ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary", meta = (ClampMin = "0.0"))
	float PrimaryBaseDamage = 28.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary", meta = (ClampMin = "0.0"))
	float PrimaryDamageScaling = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary", meta = (ClampMin = "100.0"))
	float PrimaryProjectileSpeed = 3200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary", meta = (ClampMin = "0.1"))
	float PrimaryProjectileLifeSpan = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary", meta = (ClampMin = "0.0"))
	float PrimarySpawnForwardOffset = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Primary|Presentation")
	TSoftObjectPtr<UBwayProjectilePresentationData> PrimaryProjectilePresentation;

	// --- Sun's Grace ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunsGrace", meta = (ClampMin = "0.0"))
	float SunsGraceTeleportDistance = 750.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunsGrace", meta = (ClampMin = "0.05"))
	float SunsGraceInvulnerabilityDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunsGrace", meta = (ClampMin = "0.1"))
	float SunsGraceCooldown = 22.f;

	// --- Radiance ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.1"))
	float RadianceActiveDuration = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.05"))
	float RadianceHealTickInterval = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float RadianceHealPerTick = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.01"))
	float RadianceStrengthDivisor = 52.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float RadianceMaxHealMultiplier = 1.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float RadianceMaxTargetRange = 2500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.0"))
	float RadianceMaxRayDistance = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Radiance", meta = (ClampMin = "0.1"))
	float RadianceCooldown = 8.f;

	// --- Sun Burst ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.0"))
	float SunBurstRadius = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.0"))
	float SunBurstBaseDamage = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.0"))
	float SunBurstDamageScaling = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.0"))
	float SunBurstKnockbackStrength = 1400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.0"))
	float SunBurstKnockbackUpward = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|SunBurst", meta = (ClampMin = "0.1"))
	float SunBurstCooldown = 20.f;

	// --- Blessing ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.0"))
	float BlessingForwardPlacementDistance = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "50.0"))
	float BlessingZoneRadius = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.1"))
	float BlessingZoneDuration = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.0"))
	float BlessingInitialHeal = 70.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.0"))
	float BlessingHealPerSecond = 35.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Alona|Blessing", meta = (ClampMin = "0.1"))
	float BlessingCooldown = 25.f;
};
