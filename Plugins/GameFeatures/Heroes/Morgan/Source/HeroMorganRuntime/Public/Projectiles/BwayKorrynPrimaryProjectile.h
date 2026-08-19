#pragma once

#include "Combat/BwayHeroProjectileBase.h"
#include "GameplayEffect.h"
#include "BwayKorrynPrimaryProjectile.generated.h"

class ABwayCharacterWithAbilities;

/**
 * Korryn LMB projectile — deals scaled damage then applies armor shred GE.
 */
UCLASS()
class HEROMORGANRUNTIME_API ABwayKorrynPrimaryProjectile : public ABwayHeroProjectileBase
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
	virtual bool HandleDamageHit(const FHitResult& Hit) override;

	UPROPERTY(Transient)
	TObjectPtr<ABwayCharacterWithAbilities> InstigatorCharacter;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ArmorShredEffectClass;

	float DamageAmount = 0.f;
	bool bHasAppliedHit = false;
};
