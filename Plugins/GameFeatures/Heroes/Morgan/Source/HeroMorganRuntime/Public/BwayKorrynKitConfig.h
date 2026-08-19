#pragma once

#include "Engine/DataAsset.h"
#include "BwayKorrynKitConfig.generated.h"

class UBwayProjectilePresentationData;

/**
 * Data-driven Korryn kit values for Steps 20a–20c.
 * Sheet defaults are authoritative when the DA is present; C++ ability defaults are fallbacks.
 */
UCLASS(BlueprintType)
class HEROMORGANRUNTIME_API UBwayKorrynKitConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BwayKorrynKitConfig"), GetFName());
	}

	// --- Primary ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "0.0"))
	float PrimaryBaseDamage = 22.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "0.0"))
	float PrimaryDamageScaling = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "100.0"))
	float PrimaryProjectileSpeed = 2800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "0.1"))
	float PrimaryProjectileLifeSpan = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary")
	float ArmorShredPerHit = -2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "0.05"))
	float ArmorShredDuration = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "1"))
	int32 ArmorShredMaxStacks = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary|Presentation")
	TSoftObjectPtr<UBwayProjectilePresentationData> PrimaryProjectilePresentation;

	// --- Flock ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Flock", meta = (ClampMin = "0.05"))
	float FlockDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Flock", meta = (ClampMin = "0.1"))
	float FlockCooldown = 22.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Flock", meta = (ClampMin = "0.0"))
	float FlockHoverSpeed = 600.f;

	// --- Burden of Sin ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "0.0"))
	float BurdenBaseDamage = 33.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "0.0"))
	float BurdenDamageScaling = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "0.1"))
	float BurdenCooldown = 14.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "0.05"))
	float BurdenSlowDuration = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float BurdenSlowMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "50.0"))
	float BurdenConeRange = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Burden", meta = (ClampMin = "5.0", ClampMax = "180.0"))
	float BurdenConeHalfAngleDegrees = 35.f;

	// --- Circle of Spite ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle", meta = (ClampMin = "0.1"))
	float CircleCooldown = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle", meta = (ClampMin = "0.05"))
	float CircleDuration = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle", meta = (ClampMin = "50.0"))
	float CircleRadius = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float CircleSlowMultiplier = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle", meta = (ClampMin = "1.0"))
	float CircleIncomingDamageMultiplier = 1.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Circle", meta = (ClampMin = "0.0"))
	float CircleSpawnForwardOffset = 400.f;

	// --- Aura of Silence ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Aura", meta = (ClampMin = "0.0"))
	float AuraBaseDamage = 13.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Aura", meta = (ClampMin = "0.0"))
	float AuraDamageScaling = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Aura", meta = (ClampMin = "0.1"))
	float AuraCooldown = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Aura", meta = (ClampMin = "50.0"))
	float AuraRadius = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Aura", meta = (ClampMin = "0.05"))
	float AuraSilenceDuration = 5.f;

	// --- Cursed Ward ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|CursedWard", meta = (ClampMin = "1.0"))
	float WardMaxHealth = 600.f;

	/** Sheet: 6m. Project convention is ~100 uu per meter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|CursedWard", meta = (ClampMin = "50.0"))
	float WardSlowRadius = 600.f;

	/** MoveSpeedMultiplier while enemies remain in radius (0.5 = 50% slow). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|CursedWard", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float WardSlowMultiplier = 0.5f;
};
