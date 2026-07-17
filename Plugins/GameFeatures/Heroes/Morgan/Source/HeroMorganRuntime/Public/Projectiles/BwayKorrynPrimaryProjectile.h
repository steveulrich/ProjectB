#pragma once

#include "GameFramework/Actor.h"
#include "BwayKorrynPrimaryProjectile.generated.h"

class ABwayCharacterWithAbilities;
class UProjectileMovementComponent;
class USphereComponent;
class UGameplayEffect;

/**
 * Korryn LMB projectile — deals scaled damage then applies armor shred GE.
 */
UCLASS()
class HEROMORGANRUNTIME_API ABwayKorrynPrimaryProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABwayKorrynPrimaryProjectile();

	void ConfigureProjectile(
		ABwayCharacterWithAbilities* InInstigator,
		float InDamage,
		float InSpeed,
		float InLifeSpan,
		TSubclassOf<UGameplayEffect> InArmorShredEffectClass);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryApplyHitToActor(AActor* HitActor);
	void ExpireProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Korryn|Primary")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Korryn|Primary")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Korryn|Primary", meta = (ClampMin = "1.0"))
	float SphereRadius = 18.f;

	UPROPERTY(Transient)
	TObjectPtr<ABwayCharacterWithAbilities> InstigatorCharacter;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ArmorShredEffectClass;

	float DamageAmount = 0.f;
	bool bHasAppliedHit = false;
};
