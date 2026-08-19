// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Camera/CameraShakeBase.h"
#include "BwayHeroProjectileBase.generated.h"

class UBwayProjectilePresentationData;
class UNiagaraComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USkeletalMeshComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Shared Breakaway hero projectile: collision, movement, optional visuals, and cue hooks.
 * Subclasses implement HandleDamageHit for damage / status application.
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API ABwayHeroProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	ABwayHeroProjectileBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Apply presentation data (collision, movement defaults, meshes, trail, cue tags).
	 * Safe to call before FinishSpawning; also applied from PresentData in BeginPlay / OnRep.
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Presentation")
	void ApplyPresentation(UBwayProjectilePresentationData* Data);

	/** Configure projectile movement speed / gravity / lifespan without replacing presentation visuals. */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Movement")
	void ConfigureMovement(float InSpeed, float InLifeSpan, float InGravityScale = 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_PresentData, Category = "Projectile|Presentation")
	TObjectPtr<UBwayProjectilePresentationData> PresentData;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_PresentData();

	UFUNCTION()
	void OnProjectileStop(const FHitResult& ImpactResult);

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/**
	 * Shared hit entry: impact cue, then subclass damage, then optional expire.
	 * @param bForceExpire Destroy after processing (e.g. world stop) even if HandleDamageHit returns false.
	 */
	void ProcessHit(const FHitResult& Hit, bool bForceExpire);

	/**
	 * Subclass damage / status logic. Return true to destroy the projectile after this hit.
	 * Called on authority after the impact cue is executed.
	 */
	virtual bool HandleDamageHit(const FHitResult& Hit);

	/** True when OtherActor should be ignored (self, owner, instigator). */
	virtual bool ShouldIgnoreActor(const AActor* OtherActor) const;

	/** Authority destroy. Expire cue fires from EndPlay. */
	virtual void ExpireProjectile();

	void ExecuteProjectileCue(FGameplayTag CueTag, const FVector& Location, const FVector& Normal = FVector::ZeroVector, float Magnitude = 0.f) const;
	void PlayLocalFeedback(UNiagaraSystem* Niagara, USoundBase* Sound, const FVector& Location, const FVector& Normal) const;
	void PlayImpactCameraShake() const;

	void ApplyPresentationInternal(const UBwayProjectilePresentationData* Data);
	void BindHitDelegates();
	void IgnoreInstigatorCollision();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Visual")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Visual")
	TObjectPtr<USkeletalMeshComponent> ProjectileSkeletalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Visual")
	TObjectPtr<UNiagaraComponent> TrailComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1.0"))
	float SphereRadius = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag SpawnCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag ImpactCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag ExpireCueTag;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> CachedSpawnNiagara;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CachedSpawnSound;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> CachedImpactNiagara;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CachedImpactSound;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> CachedExpireNiagara;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CachedExpireSound;

	UPROPERTY(Transient)
	TSubclassOf<UCameraShakeBase> CachedImpactCameraShake;

	float CachedImpactCameraShakeScale = 1.f;

	bool bHasProcessedTerminalHit = false;
	bool bHasExecutedSpawnCue = false;
	bool bHasExecutedExpireCue = false;
	bool bPresentationApplied = false;
};
