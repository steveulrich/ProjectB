// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BwayHeroProjectileBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/BwayProjectilePresentationData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroProjectileBase)

ABwayHeroProjectileBase::ABwayHeroProjectileBase()
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
	CollisionSphere->SetNotifyRigidBodyCollision(true);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->SetIsReplicated(true);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionSphere);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetGenerateOverlapEvents(false);
	ProjectileMesh->SetHiddenInGame(true);
	ProjectileMesh->SetVisibility(false, true);

	ProjectileSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ProjectileSkeletalMesh"));
	ProjectileSkeletalMesh->SetupAttachment(CollisionSphere);
	ProjectileSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileSkeletalMesh->SetGenerateOverlapEvents(false);
	ProjectileSkeletalMesh->SetHiddenInGame(true);
	ProjectileSkeletalMesh->SetVisibility(false, true);

	TrailComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailComponent"));
	TrailComponent->SetupAttachment(CollisionSphere);
	TrailComponent->SetAutoActivate(false);
	TrailComponent->SetHiddenInGame(true);
}

void ABwayHeroProjectileBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwayHeroProjectileBase, PresentData);
}

void ABwayHeroProjectileBase::ApplyPresentation(UBwayProjectilePresentationData* Data)
{
	PresentData = Data;
	ApplyPresentationInternal(Data);
}

void ABwayHeroProjectileBase::ConfigureMovement(float InSpeed, float InLifeSpan, float InGravityScale)
{
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = InSpeed;
		ProjectileMovement->MaxSpeed = InSpeed;
		ProjectileMovement->ProjectileGravityScale = InGravityScale;
		ProjectileMovement->Velocity = GetActorForwardVector() * InSpeed;
	}

	if (InLifeSpan > 0.f)
	{
		SetLifeSpan(InLifeSpan);
	}
}

void ABwayHeroProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	if (PresentData && !bPresentationApplied)
	{
		ApplyPresentationInternal(PresentData);
	}

	IgnoreInstigatorCollision();
	BindHitDelegates();

	if (HasAuthority() && !bHasExecutedSpawnCue)
	{
		bHasExecutedSpawnCue = true;
		ExecuteProjectileCue(SpawnCueTag, GetActorLocation());
		PlayLocalFeedback(CachedSpawnNiagara, CachedSpawnSound, GetActorLocation(), GetActorForwardVector());
	}
}

void ABwayHeroProjectileBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && !bHasExecutedExpireCue)
	{
		bHasExecutedExpireCue = true;
		ExecuteProjectileCue(ExpireCueTag, GetActorLocation());
		PlayLocalFeedback(CachedExpireNiagara, CachedExpireSound, GetActorLocation(), FVector::UpVector);
	}

	Super::EndPlay(EndPlayReason);
}

void ABwayHeroProjectileBase::OnRep_PresentData()
{
	if (PresentData && !bPresentationApplied)
	{
		ApplyPresentationInternal(PresentData);
	}
}

void ABwayHeroProjectileBase::OnProjectileStop(const FHitResult& ImpactResult)
{
	ProcessHit(ImpactResult, /*bForceExpire*/ true);
}

void ABwayHeroProjectileBase::OnSphereHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	(void)HitComp;
	(void)OtherComp;
	(void)NormalImpulse;

	AActor* HitActor = OtherActor ? OtherActor : Hit.GetActor();
	if (ShouldIgnoreActor(HitActor))
	{
		return;
	}

	// Blocking hits (world / pawns) should expire even when no damage is applied.
	ProcessHit(Hit, /*bForceExpire*/ Hit.bBlockingHit);
}

void ABwayHeroProjectileBase::OnSphereBeginOverlap(
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

	if (ShouldIgnoreActor(OtherActor))
	{
		return;
	}

	FHitResult Hit = SweepResult;
	if (!Hit.GetActor())
	{
		const FVector HitLoc = OtherActor ? OtherActor->GetActorLocation() : GetActorLocation();
		const FVector HitNorm = -GetActorForwardVector();
		Hit = FHitResult(OtherActor, OtherComp, HitLoc, HitNorm);
	}

	ProcessHit(Hit, /*bForceExpire*/ false);
}

void ABwayHeroProjectileBase::ProcessHit(const FHitResult& Hit, bool bForceExpire)
{
	if (bHasProcessedTerminalHit)
	{
		return;
	}

	const FVector ImpactLocation = Hit.bBlockingHit || !Hit.ImpactPoint.IsNearlyZero()
		? FVector(Hit.ImpactPoint)
		: GetActorLocation();
	const FVector ImpactNormal = Hit.ImpactNormal.IsNearlyZero() ? -GetActorForwardVector() : FVector(Hit.ImpactNormal);

	if (HasAuthority())
	{
		ExecuteProjectileCue(ImpactCueTag, ImpactLocation, ImpactNormal);
	}

	PlayLocalFeedback(CachedImpactNiagara, CachedImpactSound, ImpactLocation, ImpactNormal);
	PlayImpactCameraShake();

	bool bShouldExpire = bForceExpire;
	if (HasAuthority())
	{
		bShouldExpire = HandleDamageHit(Hit) || bForceExpire;
	}

	if (bShouldExpire)
	{
		bHasProcessedTerminalHit = true;
		ExpireProjectile();
	}
}

bool ABwayHeroProjectileBase::HandleDamageHit(const FHitResult& Hit)
{
	(void)Hit;
	return false;
}

bool ABwayHeroProjectileBase::ShouldIgnoreActor(const AActor* OtherActor) const
{
	if (!OtherActor || OtherActor == this)
	{
		return true;
	}

	if (OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return true;
	}

	return false;
}

void ABwayHeroProjectileBase::ExpireProjectile()
{
	if (HasAuthority() && !IsActorBeingDestroyed())
	{
		Destroy();
	}
}

void ABwayHeroProjectileBase::ExecuteProjectileCue(
	FGameplayTag CueTag,
	const FVector& Location,
	const FVector& Normal,
	float Magnitude) const
{
	if (!CueTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetInstigator());
	if (!ASC)
	{
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	}
	if (!ASC)
	{
		return;
	}

	FGameplayCueParameters Params;
	Params.Location = FVector_NetQuantize10(Location);
	if (!Normal.IsNearlyZero())
	{
		Params.Normal = FVector_NetQuantizeNormal(Normal.GetSafeNormal());
	}
	Params.RawMagnitude = Magnitude;
	Params.NormalizedMagnitude = Magnitude;
	Params.Instigator = GetInstigator();
	Params.EffectCauser = const_cast<ABwayHeroProjectileBase*>(this);
	Params.SourceObject = this;

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(GetInstigator(), const_cast<ABwayHeroProjectileBase*>(this));
	Context.AddSourceObject(this);
	Params.EffectContext = Context;

	ASC->ExecuteGameplayCue(CueTag, Params);
}

void ABwayHeroProjectileBase::PlayLocalFeedback(
	UNiagaraSystem* Niagara,
	USoundBase* Sound,
	const FVector& Location,
	const FVector& Normal) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}

	if (Niagara)
	{
		const FRotator Rotation = Normal.IsNearlyZero() ? GetActorRotation() : Normal.Rotation();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			Niagara,
			Location,
			Rotation,
			FVector::OneVector,
			true,
			true,
			ENCPoolMethod::AutoRelease);
	}
}

void ABwayHeroProjectileBase::PlayImpactCameraShake() const
{
	if (!CachedImpactCameraShake)
	{
		return;
	}

	const APawn* InstigatorPawn = GetInstigator();
	if (!InstigatorPawn)
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(InstigatorPawn->GetController()))
	{
		PC->ClientStartCameraShake(CachedImpactCameraShake, CachedImpactCameraShakeScale);
	}
}

void ABwayHeroProjectileBase::ApplyPresentationInternal(const UBwayProjectilePresentationData* Data)
{
	if (!Data)
	{
		return;
	}

	bPresentationApplied = true;

	SphereRadius = Data->CollisionRadius;
	if (CollisionSphere)
	{
		CollisionSphere->SetSphereRadius(SphereRadius);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = Data->InitialSpeed;
		ProjectileMovement->MaxSpeed = Data->MaxSpeed;
		ProjectileMovement->ProjectileGravityScale = Data->GravityScale;
		if (ProjectileMovement->Velocity.IsNearlyZero())
		{
			ProjectileMovement->Velocity = GetActorForwardVector() * Data->InitialSpeed;
		}
	}

	if (Data->LifeSpan > 0.f)
	{
		SetLifeSpan(Data->LifeSpan);
	}

	SpawnCueTag = Data->SpawnCueTag;
	ImpactCueTag = Data->ImpactCueTag;
	ExpireCueTag = Data->ExpireCueTag;

	CachedSpawnNiagara = Data->SpawnNiagara.LoadSynchronous();
	CachedSpawnSound = Data->SpawnSound.LoadSynchronous();
	CachedImpactNiagara = Data->ImpactNiagara.LoadSynchronous();
	CachedImpactSound = Data->ImpactSound.LoadSynchronous();
	CachedExpireNiagara = Data->ExpireNiagara.LoadSynchronous();
	CachedExpireSound = Data->ExpireSound.LoadSynchronous();
	CachedImpactCameraShake = Data->ImpactCameraShake;
	CachedImpactCameraShakeScale = Data->ImpactCameraShakeScale;

	if (ProjectileMesh)
	{
		if (UStaticMesh* Mesh = Data->StaticMesh.LoadSynchronous())
		{
			ProjectileMesh->SetStaticMesh(Mesh);
			ProjectileMesh->SetRelativeTransform(Data->StaticMeshRelativeTransform);
			ProjectileMesh->SetHiddenInGame(false);
			ProjectileMesh->SetVisibility(true, true);
		}
	}

	if (ProjectileSkeletalMesh)
	{
		if (USkeletalMesh* SkelMesh = Data->SkeletalMesh.LoadSynchronous())
		{
			ProjectileSkeletalMesh->SetSkeletalMesh(SkelMesh);
			ProjectileSkeletalMesh->SetRelativeTransform(Data->SkeletalMeshRelativeTransform);
			ProjectileSkeletalMesh->SetHiddenInGame(false);
			ProjectileSkeletalMesh->SetVisibility(true, true);
		}
	}

	if (TrailComponent)
	{
		if (UNiagaraSystem* Trail = Data->TrailSystem.LoadSynchronous())
		{
			TrailComponent->SetAsset(Trail);
			TrailComponent->SetRelativeTransform(Data->TrailRelativeTransform);
			TrailComponent->SetHiddenInGame(false);
			TrailComponent->Activate(true);
		}
	}
}

void ABwayHeroProjectileBase::BindHitDelegates()
{
	if (CollisionSphere)
	{
		CollisionSphere->OnComponentHit.AddDynamic(this, &ABwayHeroProjectileBase::OnSphereHit);
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ABwayHeroProjectileBase::OnSphereBeginOverlap);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ABwayHeroProjectileBase::OnProjectileStop);
	}
}

void ABwayHeroProjectileBase::IgnoreInstigatorCollision()
{
	AActor* InstigatorActor = GetInstigator();
	if (!InstigatorActor)
	{
		InstigatorActor = GetOwner();
	}

	if (CollisionSphere && InstigatorActor)
	{
		CollisionSphere->IgnoreActorWhenMoving(InstigatorActor, true);
		if (APawn* InstigatorPawn = Cast<APawn>(InstigatorActor))
		{
			InstigatorPawn->MoveIgnoreActorAdd(this);
		}
	}
}
