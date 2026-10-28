#include "Buildable/BwaySiegeEngineBuildable.h"
#include "Buildable/BwaySiegePresentationData.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystemGlobals.h"
#include "BwayGameState.h"
#include "Combat/BwayDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RHI.h"
#include "Sound/SoundBase.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwaySiegeEngineBuildable)

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarSiegeDamageAudit(TEXT("bway.Debug.SiegeDamage"), 0,
	TEXT("Emit native Siege damage evidence for the owned development fixture."));
static TAutoConsoleVariable<int32> CVarSiegePresentationAudit(TEXT("bway.Debug.SiegePresentation"), 0,
	TEXT("Log Siege presentation events, current rolling loops, and local teardown."));
#endif

ABwaySiegeEngineBuildable::ABwaySiegeEngineBuildable()
{
	bPersistsBetweenRounds = true;
	BuildTime = 0.f;

	DamageSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DamageSphere"));
	DamageSphere->SetupAttachment(RootComponent);
	DamageSphere->SetSphereRadius(DamageRadius);
	DamageSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DamageSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageSphere->SetGenerateOverlapEvents(false);

	PresentationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SiegePresentationMesh"));
	PresentationMesh->SetupAttachment(RootComponent);
	PresentationMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PresentationMesh->SetGenerateOverlapEvents(false);
	PresentationMesh->SetCanEverAffectNavigation(false);
	PresentationMesh->SetVisibility(false);
	RollingVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SiegeRollingVFX"));
	RollingVFX->SetupAttachment(RootComponent);
	RollingVFX->SetAutoActivate(false);
	RollingAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SiegeRollingAudio"));
	RollingAudio->SetupAttachment(RootComponent);
	RollingAudio->SetAutoActivate(false);
}

void ABwaySiegeEngineBuildable::BeginPlay()
{
	Super::BeginPlay();
	RefreshPresentation();
}

void ABwaySiegeEngineBuildable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABwaySiegeEngineBuildable, bIsRolling);
	DOREPLIFETIME(ABwaySiegeEngineBuildable, RollDirection);
	DOREPLIFETIME(ABwaySiegeEngineBuildable, PresentationData);
}

void ABwaySiegeEngineBuildable::InitializeAbilitySystem()
{
	Super::InitializeAbilitySystem();

	if (HealthSet)
	{
		HealthSet->InitMaxHealth(MaxHealth);
		HealthSet->InitHealth(MaxHealth);
		HealthSet->OnOutOfHealth.AddUObject(this, &ABwaySiegeEngineBuildable::HandleOutOfHealth);
	}

	if (DamageSphere)
	{
		DamageSphere->SetSphereRadius(DamageRadius);
	}
}

void ABwaySiegeEngineBuildable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Round/reset/relevance teardown stops loops; it is not a new impact or expiry event.
	StopPresentation();
	if (HealthSet) HealthSet->OnOutOfHealth.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

void ABwaySiegeEngineBuildable::HandleOutOfHealth(AActor*, AActor*, const FGameplayEffectSpec*, float, float, float)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: NativeStop name=%s reason=health elapsed=%.3f maximum=%.3f time=%.3f"),
		*GetName(), RollElapsed, MaxRollDuration, GetWorld()->GetTimeSeconds());
#endif
	StopRollAndExpire();
}

void ABwaySiegeEngineBuildable::FinishBuilding()
{
	Super::FinishBuilding();

	if (HasAuthority())
	{
		StartRoll();
	}
}

void ABwaySiegeEngineBuildable::StartRoll()
{
	if (bIsRolling || !HasAuthority())
	{
		return;
	}

	RollDirection = GetActorForwardVector().GetSafeNormal2D();
	if (RollDirection.IsNearlyZero())
	{
		RollDirection = FVector::ForwardVector;
	}

	bIsRolling = true;
	RollElapsed = 0.f;
	OnRep_IsRolling();
	DispatchPresentationEvent(EBwaySiegePresentationEvent::Launch, GetActorLocation(), RollDirection);
	ForceNetUpdate();
}

void ABwaySiegeEngineBuildable::StopRollAndExpire(bool bImpact, const FVector& ImpactLocation, const FVector& ImpactNormal)
{
	if (!HasAuthority())
	{
		return;
	}

	bIsRolling = false;
	OnRep_IsRolling();
	DispatchPresentationEvent(bImpact ? EBwaySiegePresentationEvent::TerminalImpact : EBwaySiegePresentationEvent::Expiry,
		bImpact ? ImpactLocation : GetActorLocation(), bImpact ? ImpactNormal : FVector::UpVector);
	Destroy();
}

void ABwaySiegeEngineBuildable::OnRep_IsRolling()
{
	if (DamageSphere)
	{
		DamageSphere->SetHiddenInGame(!bIsRolling);
	}
	RefreshPresentation();
}

void ABwaySiegeEngineBuildable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bRollingPresentationActive && AppliedPresentationData && PresentationMesh->IsVisible())
	{
		PresentationMesh->AddLocalRotation(AppliedPresentationData->RollingMeshRotationRate * DeltaTime);
	}

	if (!HasAuthority() || !bIsRolling)
	{
		return;
	}

	RollElapsed += DeltaTime;
	if (RollElapsed >= MaxRollDuration)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: NativeStop name=%s reason=duration elapsed=%.3f maximum=%.3f time=%.3f"), *GetName(), RollElapsed, MaxRollDuration, GetWorld()->GetTimeSeconds());
#endif
		StopRollAndExpire();
		return;
	}

	const FVector Delta = RollDirection * RollSpeed * DeltaTime;
	FHitResult MoveHit;
	// Sweep the authored gameplay volume; the skeletal root is presentation only.
	const FVector Start = AttackCollision->GetComponentLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SiegeMovement), false, this);
	GetWorld()->SweepSingleByChannel(MoveHit, Start, Start + Delta, AttackCollision->GetComponentQuat(),
		AttackCollision->GetCollisionObjectType(), FCollisionShape::MakeBox(AttackCollision->GetScaledBoxExtent()),
		Query, FCollisionResponseParams(AttackCollision->GetCollisionResponseToChannels()));
	AddActorWorldOffset(Delta * (MoveHit.bBlockingHit ? MoveHit.Time : 1.f), false);
	if (MoveHit.bBlockingHit)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: NativeStop name=%s reason=collision elapsed=%.3f maximum=%.3f time=%.3f obstacle=%s x=%.3f"),
			*GetName(), RollElapsed, MaxRollDuration, GetWorld()->GetTimeSeconds(), *GetNameSafe(MoveHit.GetActor()), GetActorLocation().X);
#endif
		StopRollAndExpire(true, MoveHit.ImpactPoint, MoveHit.ImpactNormal);
		return;
	}

	ApplyDamageToOverlappingEnemyBuildables(DeltaTime);
}

bool ABwaySiegeEngineBuildable::IsEnemyBuildable(const ABuildableActor* OtherBuildable) const
{
	if (!OtherBuildable || OtherBuildable == this)
	{
		return false;
	}

	// GameState resolves player teams, not buildable ownership. A same-team result
	// must never fall through to its unknown-actor fallback.
	return GetTeamId() != FGenericTeamId::NoTeam && OtherBuildable->GetTeamId() != FGenericTeamId::NoTeam
		&& GetTeamId().GetId() != 0 && OtherBuildable->GetTeamId().GetId() != 0
		&& OtherBuildable->GetTeamId() != GetTeamId();
}

void ABwaySiegeEngineBuildable::ApplyDamageToBuildable(ABuildableActor* TargetBuildable, float DamageAmount)
{
	if (!TargetBuildable || DamageAmount <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = TargetBuildable->GetAbilitySystemComponent();
	// OnOutOfHealth can synchronously destroy the target and unregister its ASC attributes.
	// The native attribute set remains alive for this synchronous call; capture it before damage
	// and read its post-clamp value directly instead of querying the now-destroyed component.
	const ULyraHealthSet* TargetHealth = TargetASC ? TargetASC->GetSet<ULyraHealthSet>() : nullptr;
	const float HealthBefore = TargetHealth ? TargetHealth->GetHealth() : 0.f;
	const FVector ImpactLocation = TargetBuildable->GetActorLocation();
	const bool bApplied = UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, TargetBuildable, DamageAmount, this, GetOwner());
	if (HasAuthority() && bApplied && TargetHealth && TargetHealth->GetHealth() < HealthBefore)
	{
		DispatchPresentationEvent(EBwaySiegePresentationEvent::BuildableImpact, ImpactLocation,
			(GetActorLocation() - ImpactLocation).GetSafeNormal(), TargetBuildable);
	}
}

void ABwaySiegeEngineBuildable::ApplyDamageToOverlappingEnemyBuildables(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || DamageRadius <= 0.f || BuildableDamagePerSecond <= 0.f)
	{
		return;
	}

	const ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	if (!GameState || !GameState->BuildableRegistryComponent)
	{
		return;
	}

	const float DamageThisTick = BuildableDamagePerSecond * DeltaTime;
	const FVector Origin = GetActorLocation();
	const float DamageRadiusSq = FMath::Square(DamageRadius);

	for (ABuildableActor* Buildable : GameState->BuildableRegistryComponent->GetAllBuildables())
	{
		if (!Buildable || !IsEnemyBuildable(Buildable))
		{
			continue;
		}

		if (FVector::DistSquared(Origin, Buildable->GetActorLocation()) <= DamageRadiusSq)
		{
#if !UE_BUILD_SHIPPING
			const auto* TargetASC = Buildable->GetAbilitySystemComponent();
			const bool bAudit = CVarSiegeDamageAudit.GetValueOnGameThread() != 0 && TargetASC;
			const float Before = bAudit ? TargetASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : 0;
#endif
			ApplyDamageToBuildable(Buildable, DamageThisTick);
#if !UE_BUILD_SHIPPING
			if (bAudit) UE_LOG(LogTemp, Display, TEXT("BwaySiegeCombat: Damage siege=%s target=%s delta=%.6f raw=%.6f before=%.6f after=%.6f rate=%.3f radius=%.3f"),
				*GetName(), *GetNameSafe(Buildable), DeltaTime, DamageThisTick, Before,
				TargetASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()), BuildableDamagePerSecond, DamageRadius);
#endif
		}
	}
}

bool ABwaySiegeEngineBuildable::CanRenderPresentation() const
{
	return GetWorld() && GetNetMode() != NM_DedicatedServer && FApp::CanEverRender() && !GUsingNullRHI;
}

void ABwaySiegeEngineBuildable::OnRep_PresentationData()
{
	RefreshPresentation();
}

void ABwaySiegeEngineBuildable::RefreshPresentation()
{
	if (!CanRenderPresentation() || !PresentationData)
	{
		StopPresentation();
		PresentationMesh->SetVisibility(false);
		if (bSavedRootVisibility) GetMesh()->SetVisibility(bRootWasVisible, false);
		AppliedPresentationData = nullptr;
		return;
	}
	if (AppliedPresentationData != PresentationData)
	{
		StopPresentation();
		if (!bSavedRootVisibility)
		{
			bSavedRootVisibility = true;
			bRootWasVisible = GetMesh()->IsVisible();
		}
		UStaticMesh* VisualMesh = PresentationData->StaticMesh.LoadSynchronous();
		PresentationMesh->SetStaticMesh(VisualMesh);
		PresentationMesh->SetRelativeTransform(PresentationData->MeshRelativeTransform);
		PresentationMesh->SetVisibility(VisualMesh != nullptr);
		GetMesh()->SetVisibility(VisualMesh ? false : bRootWasVisible, false);
		RollingVFX->SetAsset(PresentationData->RollingNiagara.LoadSynchronous());
		RollingVFX->SetRelativeTransform(PresentationData->RollingRelativeTransform);
		RollingAudio->SetSound(PresentationData->RollingSound.LoadSynchronous());
		RollingAudio->SetVolumeMultiplier(FMath::Max(0.f, PresentationData->RollingVolumeMultiplier));
		AppliedPresentationData = PresentationData;
	}
	if (bIsRolling && !bTerminalPresentationReceived && !bRollingPresentationActive)
	{
		bRollingPresentationActive = true;
		if (RollingVFX->GetAsset()) RollingVFX->Activate(true);
		if (RollingAudio->Sound) RollingAudio->Play();
#if !UE_BUILD_SHIPPING
		if (CVarSiegePresentationAudit.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Display, TEXT("BwaySiegePresentation: Loop name=%s active=1 net=%d niagara=%d audio=%d"),
				*GetName(), int32(GetNetMode()), RollingVFX->GetAsset() != nullptr, RollingAudio->Sound != nullptr);
		}
#endif
	}
	else if (!bIsRolling || bTerminalPresentationReceived)
	{
		StopPresentation();
	}
}

void ABwaySiegeEngineBuildable::StopPresentation()
{
	if (RollingVFX && RollingVFX->IsActive()) RollingVFX->DeactivateImmediate();
	if (RollingAudio && RollingAudio->IsPlaying()) RollingAudio->Stop();
#if !UE_BUILD_SHIPPING
	if (bRollingPresentationActive && CVarSiegePresentationAudit.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Display, TEXT("BwaySiegePresentation: Loop name=%s active=0 net=%d"), *GetName(), int32(GetNetMode()));
	}
#endif
	bRollingPresentationActive = false;
}

void ABwaySiegeEngineBuildable::DispatchPresentationEvent(EBwaySiegePresentationEvent Event,
	const FVector& Location, const FVector& Normal, const AActor* ContactTarget)
{
	if (!HasAuthority() || bTerminalPresentationDispatched) return;
	if (Event == EBwaySiegePresentationEvent::Launch)
	{
		if (bLaunchPresentationDispatched) return;
		bLaunchPresentationDispatched = true;
	}
	else if (Event == EBwaySiegePresentationEvent::BuildableImpact)
	{
		if (!ContactTarget) return;
		const TObjectKey<AActor> Key(ContactTarget);
		if (PresentedContactTargets.Contains(Key)) return;
		PresentedContactTargets.Add(Key);
	}
	else
	{
		bTerminalPresentationDispatched = true;
	}
	// A roll has one launch, at most one contact per encountered buildable, and one terminal
	// event. This bounded reliable stream is independent of per-tick damage and roll replication.
	Multicast_PlayPresentationEvent(++DispatchedPresentationSequence, Event, PresentationData, Location, Normal.GetSafeNormal());
}

void ABwaySiegeEngineBuildable::Multicast_PlayPresentationEvent_Implementation(uint16 Sequence,
	EBwaySiegePresentationEvent Event, UBwaySiegePresentationData* Data, FVector_NetQuantize10 Location,
	FVector_NetQuantizeNormal Normal)
{
	if (Sequence <= ReceivedPresentationSequence) return;
	ReceivedPresentationSequence = Sequence;
	// RepNotifies can follow this RPC in the same update. Terminal state must stop the loop
	// immediately, without depending on bIsRolling arriving before actor destruction.
	if (Event == EBwaySiegePresentationEvent::TerminalImpact || Event == EBwaySiegePresentationEvent::Expiry)
	{
		bTerminalPresentationReceived = true;
		StopPresentation();
	}
#if !UE_BUILD_SHIPPING
	if (CVarSiegePresentationAudit.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Display, TEXT("BwaySiegePresentation: Event name=%s sequence=%u event=%d net=%d render=%d data=%s location=%s"),
			*GetName(), Sequence, int32(Event), int32(GetNetMode()), CanRenderPresentation(), *GetNameSafe(Data), *FVector(Location).ToCompactString());
	}
#endif
	if (!Data || !CanRenderPresentation()) return;
	UNiagaraSystem* Niagara = nullptr;
	USoundBase* Sound = nullptr;
	switch (Event)
	{
	case EBwaySiegePresentationEvent::Launch:
		Niagara = Data->LaunchNiagara.LoadSynchronous(); Sound = Data->LaunchSound.LoadSynchronous(); break;
	case EBwaySiegePresentationEvent::BuildableImpact:
	case EBwaySiegePresentationEvent::TerminalImpact:
		Niagara = Data->ImpactNiagara.LoadSynchronous(); Sound = Data->ImpactSound.LoadSynchronous(); break;
	case EBwaySiegePresentationEvent::Expiry:
		Niagara = Data->ExpiryNiagara.LoadSynchronous(); Sound = Data->ExpirySound.LoadSynchronous(); break;
	}
	if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, FVector(Location), FMath::Max(0.f, Data->OneShotVolumeMultiplier));
	if (Niagara)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Niagara, FVector(Location), FVector(Normal).Rotation(),
			FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
	}
}
