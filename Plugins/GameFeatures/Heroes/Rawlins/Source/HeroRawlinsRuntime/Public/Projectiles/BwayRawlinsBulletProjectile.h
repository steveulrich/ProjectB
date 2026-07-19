#pragma once

#include "GameFramework/Actor.h"
#include "BwayRawlinsBulletProjectile.generated.h"

class ABwayCharacterWithAbilities;
class UProjectileMovementComponent;
class USphereComponent;

/**
 * Shared Rawlins bullet — primary, barrage, etc.
 * Server applies damage on enemy hit / overlap; optional launch impulse for juggle.
 */
UCLASS()
class HERORAWLINSRUNTIME_API ABwayRawlinsBulletProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABwayRawlinsBulletProjectile();

	void ConfigureProjectile(
		ABwayCharacterWithAbilities* InInstigatorCharacter,
		float InDamageAmount,
		float InSpeed,
		float InLifeSpan,
		const FVector& InLaunchImpulse = FVector::ZeroVector);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileStop(const FHitResult& ImpactResult);

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void TryApplyHitToActor(AActor* HitActor);
	void ExpireProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1.0"))
	float SphereRadius = 10.f;

	float DamageAmount = 0.f;
	FVector LaunchImpulse = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<ABwayCharacterWithAbilities> InstigatorCharacter;

	bool bHasAppliedHit = false;
};
