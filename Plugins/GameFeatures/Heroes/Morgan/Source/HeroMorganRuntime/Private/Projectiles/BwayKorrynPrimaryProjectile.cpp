#include "Projectiles/BwayKorrynPrimaryProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "Combat/BwayDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayKorrynPrimaryProjectile)

ABwayKorrynPrimaryProjectile::ABwayKorrynPrimaryProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetLifeSpan(3.f);

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
	ProjectileMovement->InitialSpeed = 2800.f;
	ProjectileMovement->MaxSpeed = 2800.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void ABwayKorrynPrimaryProjectile::ConfigureProjectile(
	ABwayCharacterWithAbilities* InInstigator,
	float InDamage,
	float InSpeed,
	float InLifeSpan,
	TSubclassOf<UGameplayEffect> InArmorShredEffectClass)
{
	InstigatorCharacter = InInstigator;
	DamageAmount = InDamage;
	ArmorShredEffectClass = InArmorShredEffectClass;

	if (InInstigator)
	{
		SetOwner(InInstigator);
		SetInstigator(InInstigator);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = InSpeed;
		ProjectileMovement->MaxSpeed = InSpeed;
	}

	SetLifeSpan(InLifeSpan);
}

void ABwayKorrynPrimaryProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionSphere)
	{
		CollisionSphere->OnComponentHit.AddDynamic(this, &ABwayKorrynPrimaryProjectile::OnSphereHit);
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABwayKorrynPrimaryProjectile::OnSphereBeginOverlap);
		if (InstigatorCharacter)
		{
			CollisionSphere->IgnoreActorWhenMoving(InstigatorCharacter, true);
		}
	}
}

void ABwayKorrynPrimaryProjectile::OnSphereHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	(void)HitComp;
	(void)OtherComp;
	(void)NormalImpulse;

	TryApplyHitToActor(OtherActor ? OtherActor : Hit.GetActor());
	if (bHasAppliedHit || (OtherActor && OtherActor != InstigatorCharacter))
	{
		ExpireProjectile();
	}
}

void ABwayKorrynPrimaryProjectile::OnSphereBeginOverlap(
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

	TryApplyHitToActor(OtherActor);
	if (bHasAppliedHit)
	{
		ExpireProjectile();
	}
}

void ABwayKorrynPrimaryProjectile::TryApplyHitToActor(AActor* HitActor)
{
	if (!HasAuthority() || bHasAppliedHit || !HitActor || HitActor == InstigatorCharacter)
	{
		return;
	}

	ABwayCharacterWithAbilities* HitCharacter = Cast<ABwayCharacterWithAbilities>(HitActor);
	if (!HitCharacter || !InstigatorCharacter)
	{
		return;
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

	if (!bIsEnemy)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorCharacter);
	if (DamageAmount > 0.f)
	{
		UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, HitCharacter, DamageAmount, this, InstigatorCharacter);
	}

	if (ArmorShredEffectClass && SourceASC)
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitCharacter))
		{
			FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
			Context.AddSourceObject(this);
			Context.AddInstigator(InstigatorCharacter, this);

			const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(ArmorShredEffectClass, 1.f, Context);
			if (SpecHandle.IsValid())
			{
				TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
	}

	bHasAppliedHit = true;
}

void ABwayKorrynPrimaryProjectile::ExpireProjectile()
{
	if (HasAuthority())
	{
		Destroy();
	}
}
