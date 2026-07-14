#pragma once

#include "GameFramework/Actor.h"
#include "BwayAlonaPrimaryProjectile.generated.h"

class ABwayCharacterWithAbilities;
class UProjectileMovementComponent;
class USphereComponent;

/**
 * Alona LMB light projectile. Server applies damage on enemy hit / overlap.
 */
UCLASS()
class HEROALONARUNTIME_API ABwayAlonaPrimaryProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABwayAlonaPrimaryProjectile();

	void ConfigureProjectile(ABwayCharacterWithAbilities* InInstigatorCharacter, float InDamageAmount, float InSpeed, float InLifeSpan);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileStop(const FHitResult& ImpactResult);

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryApplyDamageToActor(AActor* HitActor);
	void ExpireProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1.0"))
	float SphereRadius = 12.f;

	float DamageAmount = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<ABwayCharacterWithAbilities> InstigatorCharacter;

	bool bHasAppliedDamage = false;
};
