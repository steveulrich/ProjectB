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

	if (InInstigatorCharacter)
	{
		SetInstigator(InInstigatorCharacter);
		SetOwner(InInstigatorCharacter);
	}

	ConfigureMovement(InSpeed, InLifeSpan);
}

bool ABwayAlonaPrimaryProjectile::HandleDamageHit(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	if (!HasAuthority() || bHasAppliedDamage || !HitActor || HitActor == InstigatorCharacter)
	{
		return false;
	}

	ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(HitActor);
	if (!HitCharacter || !InstigatorCharacter)
	{
		return false;
	}

	bool bIsEnemy = false;
	if (UWorld* World = GetWorld())
	{
		if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			bIsEnemy = !GameState->AreOnSameTeam(InstigatorCharacter, HitCharacter);
		}
		else if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
		{
			bIsEnemy = TeamSubsystem->CompareTeams(InstigatorCharacter, HitCharacter) == ELyraTeamComparison::DifferentTeams;
		}
	}

	if (!bIsEnemy || DamageAmount <= 0.f)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorCharacter);
	if (UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, HitCharacter, DamageAmount, this, InstigatorCharacter))
	{
		bHasAppliedDamage = true;
		return true;
	}

	return false;
}
