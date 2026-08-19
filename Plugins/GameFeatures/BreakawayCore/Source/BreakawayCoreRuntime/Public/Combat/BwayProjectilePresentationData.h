// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Camera/CameraShakeBase.h"
#include "BwayProjectilePresentationData.generated.h"

class UNiagaraSystem;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;

/**
 * Data-driven projectile look / feel for Breakaway hero projectiles.
 * Referenced from kit configs; applied via ABwayHeroProjectileBase::ApplyPresentation.
 */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UBwayProjectilePresentationData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BwayProjectilePresentation"), GetFName());
	}

	// --- Movement / collision ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "1.0"))
	float CollisionRadius = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float InitialSpeed = 3000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float MaxSpeed = 3000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement")
	float GravityScale = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.05"))
	float LifeSpan = 3.f;

	// --- Visual mesh ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual")
	FTransform StaticMeshRelativeTransform;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual")
	FTransform SkeletalMeshRelativeTransform;

	/** Optional Niagara trail attached to the projectile root. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual")
	TSoftObjectPtr<UNiagaraSystem> TrailSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Visual")
	FTransform TrailRelativeTransform;

	// --- Gameplay cue tags (preferred; drive GCN_BwayCombatFeedback subclasses) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag SpawnCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag ImpactCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag ExpireCueTag;

	// --- Soft one-shot fallbacks when cue tags are unset ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Spawn")
	TSoftObjectPtr<UNiagaraSystem> SpawnNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Spawn")
	TSoftObjectPtr<USoundBase> SpawnSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Impact")
	TSoftObjectPtr<UNiagaraSystem> ImpactNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Impact")
	TSoftObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Expire")
	TSoftObjectPtr<UNiagaraSystem> ExpireNiagara;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Expire")
	TSoftObjectPtr<USoundBase> ExpireSound;

	/** Optional camera shake on impact (in addition to ImpactCueTag GCN). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Impact")
	TSubclassOf<UCameraShakeBase> ImpactCameraShake;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Impact", meta = (ClampMin = "0.0"))
	float ImpactCameraShakeScale = 1.f;
};
