#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BwaySiegePresentationData.generated.h"

class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

/** Replaceable Siege cosmetics. Gameplay roll speed, collision, damage, and lifetime stay on the buildable. */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UBwaySiegePresentationData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BwaySiegePresentation"), GetFName());
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Visual")
	TSoftObjectPtr<UStaticMesh> StaticMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Visual")
	FTransform MeshRelativeTransform = FTransform::Identity;
	/** Cosmetic rotation only; never rotates the gameplay root or collision. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Visual")
	FRotator RollingMeshRotationRate = FRotator(360.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Rolling")
	TSoftObjectPtr<UNiagaraSystem> RollingNiagara;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Rolling")
	FTransform RollingRelativeTransform = FTransform::Identity;
	/** Supply an authored looping sound. It stops at roll end, destruction, and relevance teardown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Rolling")
	TSoftObjectPtr<USoundBase> RollingSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Rolling", meta=(ClampMin="0.0"))
	float RollingVolumeMultiplier = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Launch")
	TSoftObjectPtr<UNiagaraSystem> LaunchNiagara;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Launch")
	TSoftObjectPtr<USoundBase> LaunchSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Impact")
	TSoftObjectPtr<UNiagaraSystem> ImpactNiagara;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Impact")
	TSoftObjectPtr<USoundBase> ImpactSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Expiry")
	TSoftObjectPtr<UNiagaraSystem> ExpiryNiagara;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Expiry")
	TSoftObjectPtr<USoundBase> ExpirySound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Siege|Audio", meta=(ClampMin="0.0"))
	float OneShotVolumeMultiplier = 0.7f;
};
