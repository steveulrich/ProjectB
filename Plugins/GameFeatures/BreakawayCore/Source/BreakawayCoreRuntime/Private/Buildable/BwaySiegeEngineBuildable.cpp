#include "Buildable/BwaySiegeEngineBuildable.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystemGlobals.h"
#include "BwayGameState.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "LyraGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "System/LyraAssetManager.h"
#include "System/LyraGameData.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwaySiegeEngineBuildable)

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
	}

	if (DamageSphere)
	{
		DamageSphere->SetSphereRadius(DamageRadius);
	}
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
		StopRollAndExpire();
		return;
	}

	const FVector Delta = RollDirection * RollSpeed * DeltaTime;
	FHitResult MoveHit;
	AddActorWorldOffset(Delta, true, &MoveHit, ETeleportType::None);
	if (MoveHit.bBlockingHit)
	{
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

	if (OtherBuildable->GetTeamId() != FGenericTeamId::NoTeam && OtherBuildable->GetTeamId() != GetTeamId())
	{
		return true;
	}

	if (UWorld* World = GetWorld())
	{
		if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			return !GameState->AreOnSameTeam(this, OtherBuildable);
		}

		if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
		{
			return TeamSubsystem->CompareTeams(this, OtherBuildable) == ELyraTeamComparison::DifferentTeams;
		}
	}

	return false;
}

void ABwaySiegeEngineBuildable::ApplyDamageToBuildable(ABuildableActor* TargetBuildable, float DamageAmount)
{
	if (!TargetBuildable || DamageAmount <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetBuildable);
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	const ULyraGameData& GameData = ULyraGameData::Get();
	const TSubclassOf<UGameplayEffect> DamageEffectClass = ULyraAssetManager::GetSubclass(GameData.DamageGameplayEffect_SetByCaller);
	if (!DamageEffectClass)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	EffectContext.AddInstigator(GetOwner(), this);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage, DamageAmount);
	}

	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
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
			ApplyDamageToBuildable(Buildable, DamageThisTick);
		}
	}
}
