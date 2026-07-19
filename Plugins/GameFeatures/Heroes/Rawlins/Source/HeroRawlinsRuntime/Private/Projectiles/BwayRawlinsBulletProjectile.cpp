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
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetLifeSpan(1.5f);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(SphereRadius);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionSphere->SetGenerateOverlapEvents(true);
	CollisionSphere->CanCharacterStepUpOn = ECB_No;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = 3600.f;
	ProjectileMovement->MaxSpeed = 3600.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bSweepCollision = true;
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

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = InSpeed;
		ProjectileMovement->MaxSpeed = InSpeed;
		ProjectileMovement->Velocity = GetActorForwardVector() * InSpeed;
	}

	SetLifeSpan(InLifeSpan);
}

void ABwayRawlinsBulletProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionSphere)
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABwayRawlinsBulletProjectile::OnSphereBeginOverlap);
		if (InstigatorCharacter)
		{
			CollisionSphere->IgnoreActorWhenMoving(InstigatorCharacter, true);
		}
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ABwayRawlinsBulletProjectile::OnProjectileStop);
	}
}

void ABwayRawlinsBulletProjectile::OnProjectileStop(const FHitResult& ImpactResult)
{
	TryApplyHitToActor(ImpactResult.GetActor());
	ExpireProjectile();
}

void ABwayRawlinsBulletProjectile::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!OtherActor || OtherActor == this || OtherActor == InstigatorCharacter || OtherActor == GetOwner())
	{
		return;
	}

	TryApplyHitToActor(OtherActor);
	if (bHasAppliedHit)
	{
		ExpireProjectile();
	}
}

void ABwayRawlinsBulletProjectile::TryApplyHitToActor(AActor* HitActor)
{
	if (!HasAuthority() || bHasAppliedHit || !HitActor || HitActor == InstigatorCharacter)
	{
		return;
	}

	if (!InstigatorCharacter || DamageAmount <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
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
			return;
		}

		if (UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, HitCage, DamageAmount, this, InstigatorCharacter))
		{
			bHasAppliedHit = true;
		}
		return;
	}

	ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(HitActor);
	if (!HitCharacter)
	{
		return;
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
		return;
	}

	if (UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, HitCharacter, DamageAmount, this, InstigatorCharacter))
	{
		if (!LaunchImpulse.IsNearlyZero())
		{
			HitCharacter->LaunchCharacter(LaunchImpulse, true, true);
		}
		bHasAppliedHit = true;
	}
}

void ABwayRawlinsBulletProjectile::ExpireProjectile()
{
	if (HasAuthority())
	{
		Destroy();
	}
}
