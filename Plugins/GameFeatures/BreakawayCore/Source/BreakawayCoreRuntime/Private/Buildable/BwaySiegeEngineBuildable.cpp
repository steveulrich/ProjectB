#include "Buildable/BwaySiegeEngineBuildable.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystemGlobals.h"
#include "BwayGameState.h"
#include "Combat/BwayDamageLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwaySiegeEngineBuildable)

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarSiegeDamageAudit(TEXT("bway.Debug.SiegeDamage"), 0,
	TEXT("Emit native Siege damage evidence for the owned development fixture."));
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
}

void ABwaySiegeEngineBuildable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABwaySiegeEngineBuildable, bIsRolling);
	DOREPLIFETIME(ABwaySiegeEngineBuildable, RollDirection);
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
}

void ABwaySiegeEngineBuildable::StopRollAndExpire()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsRolling = false;
	OnRep_IsRolling();
	Destroy();
}

void ABwaySiegeEngineBuildable::OnRep_IsRolling()
{
	if (DamageSphere)
	{
		DamageSphere->SetHiddenInGame(!bIsRolling);
	}
}

void ABwaySiegeEngineBuildable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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
		StopRollAndExpire();
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
	UBwayDamageLibrary::ApplyDamageFromSource(SourceASC, TargetBuildable, DamageAmount, this, GetOwner());
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
