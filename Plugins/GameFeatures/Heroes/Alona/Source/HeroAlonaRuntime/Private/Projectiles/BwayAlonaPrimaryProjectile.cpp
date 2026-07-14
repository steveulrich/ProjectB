#include "Projectiles/BwayAlonaPrimaryProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "LyraGameplayTags.h"
#include "System/LyraAssetManager.h"
#include "System/LyraGameData.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAlonaPrimaryProjectile)

ABwayAlonaPrimaryProjectile::ABwayAlonaPrimaryProjectile()
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
	ProjectileMovement->InitialSpeed = 3200.f;
	ProjectileMovement->MaxSpeed = 3200.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bSweepCollision = true;
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

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = InSpeed;
		ProjectileMovement->MaxSpeed = InSpeed;
		ProjectileMovement->Velocity = GetActorForwardVector() * InSpeed;
	}

	SetLifeSpan(InLifeSpan);
}

void ABwayAlonaPrimaryProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionSphere)
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABwayAlonaPrimaryProjectile::OnSphereBeginOverlap);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ABwayAlonaPrimaryProjectile::OnProjectileStop);
	}
}

void ABwayAlonaPrimaryProjectile::OnProjectileStop(const FHitResult& ImpactResult)
{
	TryApplyDamageToActor(ImpactResult.GetActor());
	ExpireProjectile();
}

void ABwayAlonaPrimaryProjectile::OnSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == InstigatorCharacter || OtherActor == GetOwner())
	{
		return;
	}

	TryApplyDamageToActor(OtherActor);
	if (bHasAppliedDamage)
	{
		ExpireProjectile();
	}
}

void ABwayAlonaPrimaryProjectile::TryApplyDamageToActor(AActor* HitActor)
{
	if (!HasAuthority() || bHasAppliedDamage || !HitActor || HitActor == InstigatorCharacter)
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

	if (!bIsEnemy || DamageAmount <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorCharacter);
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitCharacter);
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	const ULyraGameData& GameData = ULyraGameData::Get();
	TSubclassOf<UGameplayEffect> DamageEffectClass = ULyraAssetManager::GetSubclass(GameData.DamageGameplayEffect_SetByCaller);
	if (!DamageEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	EffectContext.AddInstigator(InstigatorCharacter, this);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, EffectContext);
	if (SpecHandle.IsValid())
	{
		if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
		{
			Spec->SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage, DamageAmount);
		}
		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		bHasAppliedDamage = true;
	}
}

void ABwayAlonaPrimaryProjectile::ExpireProjectile()
{
	if (HasAuthority())
	{
		Destroy();
	}
}
