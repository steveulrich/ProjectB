#pragma once

#include "Engine/DataAsset.h"
#include "BwayRawlinsKitConfig.generated.h"

/**
 * Data-driven Rawlins kit values for Steps 21a–21c.
 * Sheet defaults are authoritative when the DA is present; C++ ability defaults are fallbacks.
 * Jail radii are implementation defaults (sheet specifies Health only; no radius values).
 */
UCLASS(BlueprintType)
class HERORAWLINSRUNTIME_API UBwayRawlinsKitConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BwayRawlinsKitConfig"), GetFName());
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// --- Primary (two-shot) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "0.0"))
	float PrimaryBaseDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "0.0"))
	float PrimaryDamageScaling = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "100.0"))
	float PrimaryProjectileSpeed = 3600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "0.1"))
	float PrimaryProjectileLifeSpan = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "0.0"))
	float PrimarySpawnForwardOffset = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "1"))
	int32 PrimaryShotCount = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Primary", meta = (ClampMin = "0.01"))
	float PrimaryShotInterval = 0.12f;

	// --- Double Down (F / Ability4) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|DoubleDown", meta = (ClampMin = "0.0"))
	float DoubleDownDashDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|DoubleDown", meta = (ClampMin = "0.05"))
	float DoubleDownDashDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|DoubleDown", meta = (ClampMin = "0.1"))
	float DoubleDownCooldown = 14.f;

	// --- Power Shot ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float PowerShotBaseDamage = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float PowerShotDamageScaling = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float PowerShotTraceRadius = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float PowerShotTraceDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float PowerShotKnockbackStrength = 1600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.0"))
	float PowerShotKnockbackUpward = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|PowerShot", meta = (ClampMin = "0.1"))
	float PowerShotCooldown = 8.f;

	// --- Slide Shot ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float SlideShotSlideDistance = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.05"))
	float SlideShotSlideDuration = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float SlideShotTraceRadius = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float SlideShotBaseDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float SlideShotDamageScaling = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float SlideShotLaunchStrength = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.0"))
	float SlideShotLaunchUpward = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|SlideShot", meta = (ClampMin = "0.1"))
	float SlideShotCooldown = 18.f;

	// --- Blazing Barrage ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.0"))
	float BarrageBaseDamagePerBullet = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.0"))
	float BarrageDamageScaling = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "1"))
	int32 BarrageShotCount = 12;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.01"))
	float BarrageShotInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "100.0"))
	float BarrageProjectileSpeed = 3600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.1"))
	float BarrageProjectileLifeSpan = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.0"))
	float BarrageSpawnForwardOffset = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.0"))
	float BarrageJuggleUpward = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|BlazingBarrage", meta = (ClampMin = "0.1"))
	float BarrageCooldown = 25.f;

	// --- Jail ---
	/** Per-victim cage HP (sheet: 450). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Jail", meta = (ClampMin = "1.0"))
	float JailMaxHealth = 450.f;

	/** Floor-trap trigger sphere radius (uu). Implementation default (sheet has no radius). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Jail", meta = (ClampMin = "50.0"))
	float JailTriggerRadius = 300.f;

	/** Radius used on first trigger to capture all nearby enemies (uu). Implementation default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Jail", meta = (ClampMin = "50.0"))
	float JailCaptureRadius = 450.f;

	/** Per-victim cage hit/collision sphere radius (uu). Implementation default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rawlins|Jail", meta = (ClampMin = "50.0"))
	float JailCageRadius = 120.f;
};
