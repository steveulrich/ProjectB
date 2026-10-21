#include "Projectiles/BwayRawlinsBulletProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"

#include "Combat/BwayDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRawlinsBulletProjectile)

ABwayRawlinsBulletProjectile::ABwayRawlinsBulletProjectile()
{
	SphereRadius = 10.f;
	if (CollisionSphere)
	{
		CollisionSphere->InitSphereRadius(SphereRadius);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = 3600.f;
		ProjectileMovement->MaxSpeed = 3600.f;
	}
}

void ABwayRawlinsBulletProjectile::ConfigureProjectile(
	ABwayCharacterWithAbilities* InInstigatorCharacter,
	float InDamageAmount,
	float InSpeed,
	float InLifeSpan,
	const FVector& InLaunchImpulse)
{
	InstigatorCharacter = InInstigatorCharacter;
	DamageAmount = InDamageAmount;
	LaunchImpulse = InLaunchImpulse;

	ConfigureDamageSource(InInstigatorCharacter);

	ConfigureMovement(InSpeed, InLifeSpan);
}

bool ABwayRawlinsBulletProjectile::HandleDamageHit(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	if (!HasAuthority() || bHasAppliedHit || !HitActor || HitActor == InstigatorCharacter)
	{
		return false;
	}

	if (!GetDamageSourceASC() || DamageAmount <= 0.f)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = GetDamageSourceASC();

	if (ApplyDamageToEnemyBuildable(HitActor, DamageAmount))
	{
		bHasAppliedHit = true;
		return true;
	}

	ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(HitActor);
	if (!IsEnemyCharacter(HitCharacter))
	{
		return false;
	}

	if (UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, HitCharacter, DamageAmount, this, GetDamageInstigator()))
	{
		if (!LaunchImpulse.IsNearlyZero() && GetDamageSourceASC() && IsEnemyCharacter(HitCharacter))
		{
			HitCharacter->LaunchCharacter(LaunchImpulse, true, true);
		}
		bHasAppliedHit = true;
		return true;
	}

	return false;
}
