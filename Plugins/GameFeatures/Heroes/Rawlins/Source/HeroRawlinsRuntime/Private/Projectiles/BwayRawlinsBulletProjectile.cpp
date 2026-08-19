#include "Projectiles/BwayRawlinsBulletProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "Buildable/BwayRawlinsJailCage.h"
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

	if (InInstigatorCharacter)
	{
		SetInstigator(InInstigatorCharacter);
		SetOwner(InInstigatorCharacter);
	}

	ConfigureMovement(InSpeed, InLifeSpan);
}

bool ABwayRawlinsBulletProjectile::HandleDamageHit(const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	if (!HasAuthority() || bHasAppliedHit || !HitActor || HitActor == InstigatorCharacter)
	{
		return false;
	}

	if (!InstigatorCharacter || DamageAmount <= 0.f)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorCharacter);

	// Enemy Jail cages are damageable from inside or outside (retains team filtering).
	// Note: ABwayGameState::AreOnSameTeam does not resolve buildable TeamId — compare Lyra team IDs.
	if (ABwayRawlinsJailCage* HitCage = Cast<ABwayRawlinsJailCage>(HitActor))
	{
		bool bIsEnemyCage = false;
		if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
		{
			const int32 CageTeam = static_cast<int32>(HitCage->GetTeamId().GetId());
			const int32 ShooterTeam = TeamSubsystem->FindTeamFromObject(InstigatorCharacter);
			bIsEnemyCage = ShooterTeam != INDEX_NONE && CageTeam != 0 && ShooterTeam != CageTeam;
		}

		if (!bIsEnemyCage)
		{
			return false;
		}

		if (UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, HitCage, DamageAmount, this, InstigatorCharacter))
		{
			bHasAppliedHit = true;
			return true;
		}
		return false;
	}

	ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(HitActor);
	if (!HitCharacter)
	{
		return false;
	}

	bool bIsEnemy = false;
	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		bIsEnemy = !GameState->AreOnSameTeam(InstigatorCharacter, HitCharacter);
	}
	else if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
	{
		bIsEnemy = TeamSubsystem->CompareTeams(InstigatorCharacter, HitCharacter) == ELyraTeamComparison::DifferentTeams;
	}

	if (!bIsEnemy)
	{
		return false;
	}

	if (UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, HitCharacter, DamageAmount, this, InstigatorCharacter))
	{
		if (!LaunchImpulse.IsNearlyZero())
		{
			HitCharacter->LaunchCharacter(LaunchImpulse, true, true);
		}
		bHasAppliedHit = true;
		return true;
	}

	return false;
}
