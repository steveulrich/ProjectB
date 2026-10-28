// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BwayHeroProjectileBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Buildable/BuildableBase.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "Combat/BwayProjectilePresentationData.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayCueManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroProjectileBase)

DEFINE_LOG_CATEGORY_STATIC(LogBwayProjectileCollision, Log, All);

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarBwayProjectilePresentation(TEXT("bway.Debug.ProjectilePresentation"), 0,
	TEXT("Log received projectile presentation events and actual local camera starts."));
#endif

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
		// Actor forward is already in world space, including before FinishSpawning.
		// Component initialization must not rotate this direction a second time.
		ProjectileMovement->bInitialVelocityInLocalSpace = false;
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

	if (HasAuthority())
	{
		if (const ABwayGameState* GameState = GetWorld()->GetGameState<ABwayGameState>())
		{
			RoundManager = GameState->FindComponentByClass<UBwayRoundManagementComponent>();
			if (UBwayRoundManagementComponent* Manager = RoundManager.Get())
			{
				Manager->OnRoundStateChanged.AddDynamic(this, &ABwayHeroProjectileBase::OnRoundStateChanged);
				RoundResetHandle = Manager->OnRoundResetStarting.AddUObject(this, &ABwayHeroProjectileBase::RemoveForRoundTransition);
			}
		}
	}

	if (PresentData && !bPresentationApplied)
	{
		ApplyPresentationInternal(PresentData);
	}

	IgnoreInstigatorCollision();
	BindHitDelegates();

	if (HasAuthority())
	{
		DispatchPresentationEvent(EBwayProjectilePresentationEvent::Spawn, GetActorLocation(), GetActorForwardVector());
	}
}

void ABwayHeroProjectileBase::OnRep_Instigator()
{
	Super::OnRep_Instigator();
	// A replicated firing pawn may resolve after the initial actor bunch.
	if (HasActorBegunPlay()) IgnoreInstigatorCollision();
}

void ABwayHeroProjectileBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearSiblingCollisionIgnores();
	if (CollisionSphere) CollisionSphere->ClearMoveIgnoreActors();
	if (UBwayRoundManagementComponent* Manager = RoundManager.Get())
	{
		Manager->OnRoundStateChanged.RemoveDynamic(this, &ABwayHeroProjectileBase::OnRoundStateChanged);
		Manager->OnRoundResetStarting.Remove(RoundResetHandle);
	}
	RoundResetHandle.Reset();
	RoundManager.Reset();
	if (APawn* FiringPawn = GetInstigator())
	{
		FiringPawn->MoveIgnoreActorRemove(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ABwayHeroProjectileBase::LifeSpanExpired()
{
	ExpireProjectile();
}

void ABwayHeroProjectileBase::OnRoundStateChanged(FName NewRoundState)
{
	if (NewRoundState != TEXT("RoundActive"))
	{
		RemoveForRoundTransition();
	}
}

void ABwayHeroProjectileBase::RemoveForRoundTransition()
{
	if (HasAuthority())
	{
		bRemovingForRoundTransition = true;
		bHasProcessedTerminalHit = true;
		ExpireProjectile();
	}
}

void ABwayHeroProjectileBase::ConfigureDamageSource(ABwayCharacterWithAbilities* SourceCharacter)
{
	SourcePlayerState = SourceCharacter ? SourceCharacter->GetPlayerState<ABwayPlayerState>() : nullptr;
	SourceAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(SourcePlayerState.Get());
	// Preserve the firing-pawn ownership contract; damage attribution is independent of its lifetime.
	SetOwner(SourceCharacter);
	SetInstigator(SourceCharacter);
}

UAbilitySystemComponent* ABwayHeroProjectileBase::GetDamageSourceASC() const
{
	ABwayPlayerState* Player = SourcePlayerState.Get();
	UAbilitySystemComponent* ASC = SourceAbilitySystem.Get();
	if (!Player || !ASC || ASC->GetOwnerActor() != Player || bRemovingForRoundTransition || IsActorBeingDestroyed())
	{
		return nullptr;
	}
	if (const ABwayGameState* GameState = GetWorld()->GetGameState<ABwayGameState>())
	{
		if (!GameState->PlayerArray.Contains(Player))
		{
			return nullptr;
		}
	}
	return ASC;
}

AActor* ABwayHeroProjectileBase::GetDamageInstigator() const
{
	return SourcePlayerState.Get();
}

bool ABwayHeroProjectileBase::IsEnemyCharacter(ABwayCharacterWithAbilities* Target) const
{
	if (!GetDamageSourceASC() || !IsValid(Target) || Target->IsActorBeingDestroyed())
	{
		return false;
	}
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Target);
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!Health || Health->IsDeadOrDying() || Health->GetHealth() <= 0.f
		|| !TargetASC || TargetASC->GetAvatarActor() != Target)
	{
		return false;
	}
	if (const ABwayGameState* GameState = GetWorld()->GetGameState<ABwayGameState>())
	{
		const int32 SourceTeam = GameState->GetPlayerTeam(SourcePlayerState.Get());
		const int32 TargetTeam = GameState->GetPlayerTeam(Target->GetPlayerState());
		return SourceTeam != INDEX_NONE && TargetTeam != INDEX_NONE && SourceTeam != TargetTeam;
	}
	if (const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>())
	{
		return Teams->CompareTeams(SourcePlayerState.Get(), Target) == ELyraTeamComparison::DifferentTeams;
	}
	return false;
}

bool ABwayHeroProjectileBase::ApplyDamageToEnemyBuildable(AActor* Target, float RawDamage)
{
	const ABuildableActor* Buildable = Cast<ABuildableActor>(Target);
	UAbilitySystemComponent* SourceASC = GetDamageSourceASC();
	if (!HasAuthority() || !SourceASC || !IsValid(Buildable) || !Buildable->CanBeDamaged()
		|| Buildable->IsActorBeingDestroyed() || RawDamage <= 0.f)
	{
		return false;
	}
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	const int32 SourceTeam = Teams ? Teams->FindTeamFromObject(SourcePlayerState.Get()) : INDEX_NONE;
	const int32 TargetTeam = Buildable->GetTeamId().GetId();
	if (SourceTeam == INDEX_NONE || Buildable->GetTeamId() == FGenericTeamId::NoTeam
		|| TargetTeam == 0 || SourceTeam == TargetTeam)
	{
		return false;
	}
	return UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, Target, RawDamage, this, GetDamageInstigator());
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
	UE_LOG(LogBwayProjectileCollision, Verbose, TEXT("Overlap projectile=%s other=%s sweepActor=%s authority=%d"),
		*GetName(), *GetNameSafe(OtherActor), *GetNameSafe(SweepResult.GetActor()), HasAuthority());

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
	if (!HasAuthority() || bProcessingHit || bHasProcessedTerminalHit || bRemovingForRoundTransition || IsActorBeingDestroyed())
	{
		return;
	}

	// A queued callback can outlive the sweep that registered an ignore pair.
	// Sweeps are the primary protection: ignoring only this callback would
	// still allow ProjectileMovement to stop on the sibling.
	if (IsSiblingProjectile(Hit.GetActor())) return;

	const FVector ImpactLocation = Hit.bBlockingHit || !Hit.ImpactPoint.IsNearlyZero()
		? FVector(Hit.ImpactPoint)
		: GetActorLocation();
	const FVector ImpactNormal = Hit.ImpactNormal.IsNearlyZero() ? -GetActorForwardVector() : FVector(Hit.ImpactNormal);

	bool bShouldExpire = false;
	{
		// Damage can synchronously finish the round. Defer our destruction until
		// the confirmed impact event has left the still-live actor channel.
		TGuardValue<bool> ProcessingHit(bProcessingHit, true);
		bShouldExpire = HandleDamageHit(Hit) || bForceExpire;
		UE_LOG(LogBwayProjectileCollision, Verbose, TEXT("DamageHit projectile=%s target=%s instigator=%s expire=%d"),
			*GetName(), *GetNameSafe(Hit.GetActor()), *GetNameSafe(GetInstigator()), bShouldExpire);
		if (bShouldExpire && !IsActorBeingDestroyed())
		{
			bHasProcessedTerminalHit = true;
			DispatchPresentationEvent(EBwayProjectilePresentationEvent::Impact, ImpactLocation, ImpactNormal);
		}
	}

	if (bShouldExpire || bExpiryRequestedDuringHit)
	{
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

	if (OtherActor == GetOwner() || OtherActor == GetInstigator() || IsSiblingProjectile(OtherActor))
	{
		return true;
	}

	return false;
}

void ABwayHeroProjectileBase::ExpireProjectile()
{
	if (HasAuthority() && !IsActorBeingDestroyed())
	{
		if (bProcessingHit)
		{
			bExpiryRequestedDuringHit = true;
			return;
		}
		if (!bRemovingForRoundTransition)
		{
			DispatchPresentationEvent(EBwayProjectilePresentationEvent::Expire, GetActorLocation(), FVector::UpVector);
		}
		Destroy();
	}
}

void ABwayHeroProjectileBase::DispatchPresentationEvent(EBwayProjectilePresentationEvent Event,
	const FVector& Location, const FVector& Normal)
{
	if (!HasAuthority() || IsActorBeingDestroyed()) return;
	const uint8 EventBit = 1 << uint8(Event);
	if ((DispatchedPresentationEvents & EventBit) != 0) return;
	DispatchedPresentationEvents |= EventBit;

	FGameplayTag CueTag;
	UNiagaraSystem* Niagara = nullptr;
	USoundBase* Sound = nullptr;
	switch (Event)
	{
	case EBwayProjectilePresentationEvent::Spawn:
		CueTag = SpawnCueTag; Niagara = CachedSpawnNiagara; Sound = CachedSpawnSound;
		break;
	case EBwayProjectilePresentationEvent::Impact:
		CueTag = ImpactCueTag; Niagara = CachedImpactNiagara; Sound = CachedImpactSound;
		break;
	case EBwayProjectilePresentationEvent::Expire:
		CueTag = ExpireCueTag; Niagara = CachedExpireNiagara; Sound = CachedExpireSound;
		break;
	}
	const bool bImpact = Event == EBwayProjectilePresentationEvent::Impact;
	// Carry resolved event data so presentation does not depend on PresentData's
	// RepNotify ordering. Valid cue tags exclusively own their authored sound/VFX.
	if (CueTag.IsValid()) { Niagara = nullptr; Sound = nullptr; }
	if (!CueTag.IsValid() && !Niagara && !Sound && !(bImpact && CachedImpactCameraShake)) return;
	Multicast_PlayPresentationEvent(Event, CueTag, Niagara, Sound, Location, Normal.GetSafeNormal(),
		GetInstigator(), SourcePlayerState.IsValid() ? GetDamageInstigator() : GetInstigator(),
		bImpact ? CachedImpactCameraShake : TSubclassOf<UCameraShakeBase>(), CachedImpactCameraShakeScale);
}

void ABwayHeroProjectileBase::Multicast_PlayPresentationEvent_Implementation(EBwayProjectilePresentationEvent Event,
	FGameplayTag CueTag, UNiagaraSystem* Niagara, USoundBase* Sound, FVector_NetQuantize10 Location,
	FVector_NetQuantizeNormal Normal, APawn* FeedbackPawn, AActor* CueInstigator,
	TSubclassOf<UCameraShakeBase> CameraShake, float CameraShakeScale)
{
	const uint8 EventBit = 1 << uint8(Event);
	if ((ReceivedPresentationEvents & EventBit) != 0) return;
	ReceivedPresentationEvents |= EventBit;
	if (CueTag.IsValid())
	{
		ExecuteProjectileCue(CueTag, Location, Normal, FeedbackPawn, CueInstigator);
	}
	else
	{
		PlayLocalFeedback(Niagara, Sound, Location, Normal);
	}
	const bool bCameraStarted = Event == EBwayProjectilePresentationEvent::Impact
		&& PlayImpactCameraShake(FeedbackPawn, CameraShake, CameraShakeScale);
#if !UE_BUILD_SHIPPING
	if (CVarBwayProjectilePresentation.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogBwayProjectileCollision, Display, TEXT("BwayProjectilePresentation: Event projectile=%s event=%d net=%d cue=%s fallbackVfx=%d fallbackSound=%d cameraStarted=%d location=%s"),
			*GetName(), int32(Event), int32(GetNetMode()), *CueTag.ToString(), !CueTag.IsValid() && Niagara != nullptr,
			!CueTag.IsValid() && Sound != nullptr, bCameraStarted, *FVector(Location).ToCompactString());
	}
#endif
}

void ABwayHeroProjectileBase::ExecuteProjectileCue(
	FGameplayTag CueTag,
	const FVector& Location,
	const FVector& Normal,
	APawn* FeedbackPawn,
	AActor* CueInstigator) const
{
	if (!CueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters Params;
	Params.Location = FVector_NetQuantize10(Location);
	if (!Normal.IsNearlyZero())
	{
		Params.Normal = FVector_NetQuantizeNormal(Normal.GetSafeNormal());
	}
	Params.Instigator = CueInstigator;
	Params.EffectCauser = const_cast<ABwayHeroProjectileBase*>(this);
	Params.SourceObject = this;
	FGameplayEffectContextHandle Context(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
	Context.AddInstigator(CueInstigator, const_cast<ABwayHeroProjectileBase*>(this));
	Context.AddSourceObject(this);
	Params.EffectContext = Context;

	// This multicast is the sole transport. Executing through the ASC here would
	// multicast again on authority and can suppress a predicting owner's receipt.
	AActor* Target = FeedbackPawn ? static_cast<AActor*>(FeedbackPawn) : const_cast<ABwayHeroProjectileBase*>(this);
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(Target, CueTag, EGameplayCueEvent::Executed, Params);
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

bool ABwayHeroProjectileBase::PlayImpactCameraShake(APawn* FeedbackPawn, TSubclassOf<UCameraShakeBase> CameraShake, float Scale) const
{
	if (!CameraShake || !FeedbackPawn)
	{
		return false;
	}

	APlayerController* PC = Cast<APlayerController>(FeedbackPawn->GetController());
	if (PC && PC->IsLocalController() && PC->PlayerCameraManager)
	{
		const UBwayCombatReadabilityConfig* Config = UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(FeedbackPawn);
		if (!Config || !Config->bReduceCameraShake)
		{
			return PC->PlayerCameraManager->StartCameraShake(CameraShake, Scale) != nullptr;
		}
	}
	return false;
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
			ProjectileMovement->bInitialVelocityInLocalSpace = false;
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

bool ABwayHeroProjectileBase::IsSiblingProjectile(const AActor* OtherActor) const
{
	const auto* OtherProjectile = Cast<ABwayHeroProjectileBase>(OtherActor);
	// Never treat unrelated or unowned shots as siblings.
	return GetInstigator() && OtherProjectile && OtherProjectile != this
		&& OtherProjectile->GetInstigator() == GetInstigator();
}

void ABwayHeroProjectileBase::ClearSiblingCollisionIgnores()
{
	for (const auto& WeakSibling : IgnoredSiblingProjectiles)
	{
		if (ABwayHeroProjectileBase* Sibling = WeakSibling.Get())
		{
			if (CollisionSphere) CollisionSphere->IgnoreActorWhenMoving(Sibling, false);
			if (Sibling->CollisionSphere) Sibling->CollisionSphere->IgnoreActorWhenMoving(this, false);
			Sibling->IgnoredSiblingProjectiles.RemoveAll([this](const TWeakObjectPtr<ABwayHeroProjectileBase>& Entry)
			{
				return !Entry.IsValid() || Entry.Get() == this;
			});
		}
	}
	IgnoredSiblingProjectiles.Reset();
}

void ABwayHeroProjectileBase::IgnoreInstigatorCollision()
{
	ClearSiblingCollisionIgnores();
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

	if (!CollisionSphere || !GetInstigator() || !GetWorld()) return;
	// Timer catch-up can spawn multiple rounds before any projectile moves.
	// Preserve WorldDynamic collision; ignore only explicit same-pawn pairs.
	for (TActorIterator<ABwayHeroProjectileBase> It(GetWorld()); It; ++It)
	{
		ABwayHeroProjectileBase* Sibling = *It;
		if (!IsValid(Sibling) || Sibling->IsActorBeingDestroyed() || !Sibling->CollisionSphere
			|| !IsSiblingProjectile(Sibling)) continue;
		CollisionSphere->IgnoreActorWhenMoving(Sibling, true);
		Sibling->CollisionSphere->IgnoreActorWhenMoving(this, true);
		IgnoredSiblingProjectiles.AddUnique(Sibling);
		Sibling->IgnoredSiblingProjectiles.AddUnique(this);
	}
}
