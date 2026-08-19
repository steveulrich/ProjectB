#pragma once

#include "Combat/BwayHeroProjectileBase.h"
#include "BwayRawlinsBulletProjectile.generated.h"

class ABwayCharacterWithAbilities;

/**
 * Shared Rawlins bullet — primary, barrage, etc.
 * Server applies damage on enemy hit / overlap; optional launch impulse for juggle.
 */
UCLASS()
class HERORAWLINSRUNTIME_API ABwayRawlinsBulletProjectile : public ABwayHeroProjectileBase
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
	virtual bool HandleDamageHit(const FHitResult& Hit) override;

	float DamageAmount = 0.f;
	FVector LaunchImpulse = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<ABwayCharacterWithAbilities> InstigatorCharacter;

	bool bHasAppliedHit = false;
};
