#include "Projectiles/BwayAlonaPrimaryProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "Combat/BwayDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAlonaPrimaryProjectile)

ABwayAlonaPrimaryProjectile::ABwayAlonaPrimaryProjectile()
{
	SphereRadius = 12.f;
	if (CollisionSphere)
	{
		CollisionSphere->InitSphereRadius(SphereRadius);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = 3200.f;
		ProjectileMovement->MaxSpeed = 3200.f;
	}
}

void ABwayAlonaPrimaryProjectile::ConfigureProjectile(
	ABwayCharacterWithAbilities* InInstigatorCharacter,
	float InDamageAmount,
	float InSpeed,
	float InLifeSpan)
{
	InstigatorCharacter = InInstigatorCharacter;
	DamageAmount = InDamageAmount;

	ConfigureDamageSource(InInstigatorCharacter);

	ConfigureMovement(InSpeed, InLifeSpan);
}

bool ABwayAlonaPrimaryProjectile::HandleDamageHit(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	if (!HasAuthority() || bHasAppliedDamage || !HitActor || HitActor == InstigatorCharacter)
	{
		return false;
	}

	if (ApplyDamageToEnemyBuildable(HitActor, DamageAmount))
	{
		bHasAppliedDamage = true;
		return true;
	}
	ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(HitActor);
	if (!IsEnemyCharacter(HitCharacter) || DamageAmount <= 0.f)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = GetDamageSourceASC();
	if (UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, HitCharacter, DamageAmount, this, GetDamageInstigator()))
	{
		bHasAppliedDamage = true;
		return true;
	}

	return false;
}
