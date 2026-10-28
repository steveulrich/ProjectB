// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Camera/CameraShakeBase.h"
#include "BwayHeroProjectileBase.generated.h"

class UBwayProjectilePresentationData;
class APawn;
class ABwayCharacterWithAbilities;
class ABwayPlayerState;
class UAbilitySystemComponent;
class UBwayRoundManagementComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USkeletalMeshComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

UENUM()
enum class EBwayProjectilePresentationEvent : uint8
{
	Spawn,
	Impact,
	Expire
};

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
	virtual void OnRep_Instigator() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void LifeSpanExpired() override;

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
	 * Authority hit entry: apply damage, present an accepted terminal hit, then expire.
	 * @param bForceExpire Destroy after processing (e.g. world stop) even if HandleDamageHit returns false.
	 */
	void ProcessHit(const FHitResult& Hit, bool bForceExpire);

	/**
	 * Subclass damage / status logic. Return true to destroy the projectile after this hit.
	 * Called on authority. Reentrant collision callbacks are ignored during damage application.
	 */
	virtual bool HandleDamageHit(const FHitResult& Hit);

	/** Keep fired shots attributed to the persistent player when the firing pawn ends. */
	void ConfigureDamageSource(ABwayCharacterWithAbilities* SourceCharacter);
	UAbilitySystemComponent* GetDamageSourceASC() const;
	AActor* GetDamageInstigator() const;
	bool IsEnemyCharacter(ABwayCharacterWithAbilities* Target) const;
	bool ApplyDamageToEnemyBuildable(AActor* Target, float RawDamage);

	UFUNCTION()
	void OnRoundStateChanged(FName NewRoundState);
	void RemoveForRoundTransition();

	/** True for self, owner, instigator, or another hero projectile from the same firing pawn. */
	virtual bool ShouldIgnoreActor(const AActor* OtherActor) const;

	/** Present natural expiry before destruction; teardown and round cleanup are silent. */
	virtual void ExpireProjectile();

	void DispatchPresentationEvent(EBwayProjectilePresentationEvent Event, const FVector& Location, const FVector& Normal);

	// Reliable events open the projectile channel even for a short-lived shot. They are
	// not replicated state: a peer arriving after an event does not replay that event.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayPresentationEvent(EBwayProjectilePresentationEvent Event, FGameplayTag CueTag,
		UNiagaraSystem* Niagara, USoundBase* Sound, FVector_NetQuantize10 Location, FVector_NetQuantizeNormal Normal,
		APawn* FeedbackPawn, AActor* CueInstigator, TSubclassOf<UCameraShakeBase> CameraShake, float CameraShakeScale);

	virtual void ExecuteProjectileCue(FGameplayTag CueTag, const FVector& Location, const FVector& Normal,
		APawn* FeedbackPawn, AActor* CueInstigator) const;
	virtual void PlayLocalFeedback(UNiagaraSystem* Niagara, USoundBase* Sound, const FVector& Location, const FVector& Normal) const;
	bool PlayImpactCameraShake(APawn* FeedbackPawn, TSubclassOf<UCameraShakeBase> CameraShake, float Scale) const;

	void ApplyPresentationInternal(const UBwayProjectilePresentationData* Data);
	void BindHitDelegates();
	void IgnoreInstigatorCollision();
	bool IsSiblingProjectile(const AActor* OtherActor) const;
	void ClearSiblingCollisionIgnores();

	// MoveIgnoreActors contains strong references. Track pairs weakly and remove
	// both directions explicitly when either projectile ends play.
	TArray<TWeakObjectPtr<ABwayHeroProjectileBase>> IgnoredSiblingProjectiles;

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

	UPROPERTY(Transient)
	TWeakObjectPtr<ABwayPlayerState> SourcePlayerState;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;

	TWeakObjectPtr<UBwayRoundManagementComponent> RoundManager;
	FDelegateHandle RoundResetHandle;
	bool bRemovingForRoundTransition = false;

	bool bHasProcessedTerminalHit = false;
	bool bProcessingHit = false;
	bool bExpiryRequestedDuringHit = false;
	uint8 DispatchedPresentationEvents = 0;
	uint8 ReceivedPresentationEvents = 0;
	bool bPresentationApplied = false;
};
