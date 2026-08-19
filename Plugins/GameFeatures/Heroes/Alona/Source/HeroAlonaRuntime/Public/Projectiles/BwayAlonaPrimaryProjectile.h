#pragma once

#include "Combat/BwayHeroProjectileBase.h"
#include "BwayAlonaPrimaryProjectile.generated.h"

class ABwayCharacterWithAbilities;

/**
 * Alona LMB light projectile. Server applies damage on enemy hit / overlap.
 */
UCLASS()
class HEROALONARUNTIME_API ABwayAlonaPrimaryProjectile : public ABwayHeroProjectileBase
{
	GENERATED_BODY()

public:
	ABwayAlonaPrimaryProjectile();

	void ConfigureProjectile(ABwayCharacterWithAbilities* InInstigatorCharacter, float InDamageAmount, float InSpeed, float InLifeSpan);

protected:
	virtual bool HandleDamageHit(const FHitResult& Hit) override;

	float DamageAmount = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<ABwayCharacterWithAbilities> InstigatorCharacter;

	bool bHasAppliedDamage = false;
};
