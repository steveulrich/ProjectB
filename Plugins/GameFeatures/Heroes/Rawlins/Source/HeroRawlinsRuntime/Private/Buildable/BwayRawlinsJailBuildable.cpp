#include "Buildable/BwayRawlinsJailBuildable.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayRawlinsKitConfig.h"
#include "Buildable/BwayRawlinsJailCage.h"
#include "Character/LyraHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRawlinsJailBuildable)

ABwayRawlinsJailBuildable::ABwayRawlinsJailBuildable()
{
	bPersistsBetweenRounds = true;
	bDestroyOnTrigger = false; // We destroy explicitly after spawning cages.
	BuildTime = 0.f;
	PrimaryActorTick.bCanEverTick = false;

	KitConfig = TSoftObjectPtr<UBwayRawlinsKitConfig>(
		FSoftObjectPath(TEXT("/Hero_Rawlins/Kit/DA_BW_RawlinsKitConfig.DA_BW_RawlinsKitConfig")));

	JailCageClass = ABwayRawlinsJailCage::StaticClass();

	EffectRadius = TriggerRadius;
	if (TriggerVolume)
	{
		TriggerVolume->InitSphereRadius(TriggerRadius);
		TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TriggerVolume->SetCollisionObjectType(ECC_WorldDynamic);
		TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
		TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		TriggerVolume->SetGenerateOverlapEvents(true);
	}
}

void ABwayRawlinsJailBuildable::ApplyKitConfigOverrides()
{
	if (KitConfig.IsNull())
	{
		return;
	}

	if (const UBwayRawlinsKitConfig* Config = KitConfig.LoadSynchronous())
	{
		TriggerRadius = Config->JailTriggerRadius;
		CaptureRadius = Config->JailCaptureRadius;
		EffectRadius = TriggerRadius;
	}
}

void ABwayRawlinsJailBuildable::InitializeAbilitySystem()
{
	ApplyKitConfigOverrides();

	Super::InitializeAbilitySystem();

	// Floor trap stays invulnerable while armed — cages absorb damage after trigger.
	SetCanBeDamaged(false);

	if (HealthSet)
	{
		// Keep a nominal health pool for ASC completeness; trap is not damagable while armed.
		HealthSet->InitMaxHealth(1.f);
		HealthSet->InitHealth(1.f);
		HealthSet->OnOutOfHealth.AddUObject(this, &ABwayRawlinsJailBuildable::HandleOutOfHealth);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.AddDynamic(this, &ABwayRawlinsJailBuildable::HandleDeathStarted);
	}

	if (TriggerVolume)
	{
		TriggerVolume->SetSphereRadius(TriggerRadius);
	}
}

void ABwayRawlinsJailBuildable::BeginPlay()
{
	Super::BeginPlay();

	ApplyKitConfigOverrides();
	if (TriggerVolume)
	{
		TriggerVolume->SetSphereRadius(TriggerRadius);
	}

	if (HasAuthority() && bIsActive && !bHasTriggered)
	{
		ArmTrap();
	}
}

void ABwayRawlinsJailBuildable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HealthSet)
	{
		HealthSet->OnOutOfHealth.RemoveAll(this);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.RemoveDynamic(this, &ABwayRawlinsJailBuildable::HandleDeathStarted);
	}

	Super::EndPlay(EndPlayReason);
}

void ABwayRawlinsJailBuildable::FinishBuilding()
{
	Super::FinishBuilding();
	ArmTrap();
}

void ABwayRawlinsJailBuildable::ArmTrap()
{
	if (!HasAuthority() || bHasTriggered || bDeathHandled)
	{
		return;
	}

	SetCanBeDamaged(false);
	if (TriggerVolume)
	{
		TriggerVolume->SetSphereRadius(TriggerRadius);
		TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void ABwayRawlinsJailBuildable::OnOverlapBegin(
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

	if (!HasAuthority() || !bIsActive || bIsDestroyed || bHasTriggered || bDeathHandled)
	{
		return;
	}

	ABwayCharacterWithAbilities* OtherCharacter = Cast<ABwayCharacterWithAbilities>(OtherActor);
	if (!OtherCharacter || !IsEnemyOfTrap(OtherCharacter) || !IsAliveEnemy(OtherCharacter))
	{
		return;
	}

	TriggerJailCapture(OtherCharacter);
}

void ABwayRawlinsJailBuildable::TriggerJailCapture(ABwayCharacterWithAbilities* TriggeringEnemy)
{
	if (!HasAuthority() || bHasTriggered || !TriggeringEnemy)
	{
		return;
	}

	bHasTriggered = true;

	if (TriggerVolume)
	{
		TriggerVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	TArray<ABwayCharacterWithAbilities*> CapturedEnemies;
	CollectEnemiesInCaptureRadius(CapturedEnemies);

	// Guarantee the triggering enemy is included even if the overlap query misses them.
	CapturedEnemies.AddUnique(TriggeringEnemy);

	for (ABwayCharacterWithAbilities* Victim : CapturedEnemies)
	{
		if (Victim && IsAliveEnemy(Victim) && IsEnemyOfTrap(Victim))
		{
			SpawnCageForVictim(Victim);
		}
	}

	Destroy();
}

void ABwayRawlinsJailBuildable::CollectEnemiesInCaptureRadius(TArray<ABwayCharacterWithAbilities*>& OutEnemies) const
{
	OutEnemies.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RawlinsJailCapture), false, this);
	QueryParams.AddIgnoredActor(this);

	World->OverlapMultiByChannel(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(CaptureRadius),
		QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ABwayCharacterWithAbilities* Enemy = Cast<ABwayCharacterWithAbilities>(Overlap.GetActor());
		if (Enemy && IsEnemyOfTrap(Enemy) && IsAliveEnemy(Enemy))
		{
			OutEnemies.AddUnique(Enemy);
		}
	}
}

ABwayRawlinsJailCage* ABwayRawlinsJailBuildable::SpawnCageForVictim(ABwayCharacterWithAbilities* Victim) const
{
	UWorld* World = GetWorld();
	if (!World || !Victim || !JailCageClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	// Intentionally NOT owned by the placer PC — registry counts Owner == PlayerController.
	// Also NOT owned by this trap — UE destroys owned actors when the owner is destroyed,
	// and the floor trap is consumed immediately after spawning cages.
	SpawnParams.Owner = nullptr;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform SpawnTransform(Victim->GetActorRotation(), Victim->GetActorLocation());
	ABwayRawlinsJailCage* Cage = World->SpawnActor<ABwayRawlinsJailCage>(JailCageClass, SpawnTransform, SpawnParams);
	if (!Cage)
	{
		return nullptr;
	}

	Cage->SetTeamId(GetTeamId());
	if (!Cage->GetInstigator())
	{
		Cage->SetInstigator(GetInstigator());
	}

	Cage->CaptureVictim(Victim);
	return Cage;
}

AActor* ABwayRawlinsJailBuildable::ResolveTeamProxy() const
{
	if (AController* OwningController = Cast<AController>(GetOwner()))
	{
		if (APawn* OwnedPawn = OwningController->GetPawn())
		{
			return OwnedPawn;
		}
	}

	if (AActor* InstigatorActor = GetInstigator())
	{
		return InstigatorActor;
	}

	return nullptr;
}

bool ABwayRawlinsJailBuildable::IsEnemyOfTrap(ABwayCharacterWithAbilities* OtherCharacter) const
{
	if (!OtherCharacter)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AActor* TeamProxy = ResolveTeamProxy();
	if (TeamProxy)
	{
		if (TeamProxy == OtherCharacter)
		{
			return false;
		}

		if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			return !GameState->AreOnSameTeam(TeamProxy, OtherCharacter);
		}

		if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
		{
			return TeamSubsystem->CompareTeams(TeamProxy, OtherCharacter) == ELyraTeamComparison::DifferentTeams;
		}

		return false;
	}

	if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
	{
		const int32 TrapTeam = static_cast<int32>(GetTeamId().GetId());
		const int32 OtherTeam = TeamSubsystem->FindTeamFromObject(OtherCharacter);
		return OtherTeam != INDEX_NONE && TrapTeam != 0 && OtherTeam != TrapTeam;
	}

	return false;
}

bool ABwayRawlinsJailBuildable::IsAliveEnemy(ABwayCharacterWithAbilities* Enemy) const
{
	if (!Enemy)
	{
		return false;
	}

	if (const ULyraHealthComponent* EnemyHealth = ULyraHealthComponent::FindHealthComponent(Enemy))
	{
		return !EnemyHealth->IsDeadOrDying();
	}

	return true;
}

void ABwayRawlinsJailBuildable::HandleOutOfHealth(
	AActor* EffectInstigator,
	AActor* EffectCauser,
	const FGameplayEffectSpec* EffectSpec,
	float EffectMagnitude,
	float OldValue,
	float NewValue)
{
	(void)EffectInstigator;
	(void)EffectCauser;
	(void)EffectSpec;
	(void)EffectMagnitude;
	(void)OldValue;
	(void)NewValue;

	// Floor trap should remain invulnerable while armed; ignore accidental OOH.
}

void ABwayRawlinsJailBuildable::HandleDeathStarted(AActor* OwningActor)
{
	(void)OwningActor;
	// Floor trap is not meant to die while armed.
}
