#include "Buildable/BwayKorrynCursedWardBuildable.h"

#include "Abilities/BwayGameplayEffect_KorrynEffects.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayKorrynKitConfig.h"
#include "Character/LyraHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayKorrynCursedWardBuildable)

ABwayKorrynCursedWardBuildable::ABwayKorrynCursedWardBuildable()
{
	bPersistsBetweenRounds = true;
	BuildTime = 0.f;
	PrimaryActorTick.bCanEverTick = false;

	KitConfig = TSoftObjectPtr<UBwayKorrynKitConfig>(
		FSoftObjectPath(TEXT("/Hero_Morgan/Kit/DA_BW_KorrynKitConfig.DA_BW_KorrynKitConfig")));

	SlowSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SlowSphere"));
	SlowSphere->SetupAttachment(RootComponent);
	SlowSphere->InitSphereRadius(SlowRadius);
	SlowSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SlowSphere->SetCollisionObjectType(ECC_WorldDynamic);
	SlowSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	SlowSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SlowSphere->SetGenerateOverlapEvents(true);

	SlowEffectClass = UGE_Bway_KorrynCursedWardSlow::StaticClass();
}

void ABwayKorrynCursedWardBuildable::ApplyKitConfigOverrides()
{
	if (KitConfig.IsNull())
	{
		return;
	}

	if (const UBwayKorrynKitConfig* Config = KitConfig.LoadSynchronous())
	{
		MaxHealth = Config->WardMaxHealth;
		SlowRadius = Config->WardSlowRadius;
		SlowMultiplier = Config->WardSlowMultiplier;
	}
}

void ABwayKorrynCursedWardBuildable::InitializeAbilitySystem()
{
	ApplyKitConfigOverrides();

	Super::InitializeAbilitySystem();

	if (HealthSet)
	{
		HealthSet->InitMaxHealth(MaxHealth);
		HealthSet->InitHealth(MaxHealth);
		HealthSet->OnOutOfHealth.AddUObject(this, &ABwayKorrynCursedWardBuildable::HandleOutOfHealth);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.AddDynamic(this, &ABwayKorrynCursedWardBuildable::HandleDeathStarted);
	}

	if (SlowSphere)
	{
		SlowSphere->SetSphereRadius(SlowRadius);
	}
}

void ABwayKorrynCursedWardBuildable::BeginPlay()
{
	Super::BeginPlay();

	if (SlowSphere)
	{
		SlowSphere->SetSphereRadius(SlowRadius);
		SlowSphere->OnComponentBeginOverlap.AddDynamic(this, &ABwayKorrynCursedWardBuildable::OnSlowSphereBeginOverlap);
		SlowSphere->OnComponentEndOverlap.AddDynamic(this, &ABwayKorrynCursedWardBuildable::OnSlowSphereEndOverlap);
	}

	// BuildTime == 0 finishes in Super::BeginPlay → FinishBuilding → StartSlowAura.
	// If already active (e.g. round persist), ensure aura runs.
	if (HasAuthority() && bIsActive && !bSlowAuraActive)
	{
		StartSlowAura();
	}
}

void ABwayKorrynCursedWardBuildable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSlowAura();
	ClearAllSlowEffects();

	if (HealthSet)
	{
		HealthSet->OnOutOfHealth.RemoveAll(this);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.RemoveDynamic(this, &ABwayKorrynCursedWardBuildable::HandleDeathStarted);
	}

	Super::EndPlay(EndPlayReason);
}

void ABwayKorrynCursedWardBuildable::FinishBuilding()
{
	Super::FinishBuilding();

	if (HasAuthority())
	{
		StartSlowAura();
	}
}

void ABwayKorrynCursedWardBuildable::StartSlowAura()
{
	if (!HasAuthority() || bSlowAuraActive || !bIsActive || bDeathHandled)
	{
		return;
	}

	bSlowAuraActive = true;
	SeedEnemiesAlreadyInRadius();
}

void ABwayKorrynCursedWardBuildable::StopSlowAura()
{
	bSlowAuraActive = false;
}

void ABwayKorrynCursedWardBuildable::SeedEnemiesAlreadyInRadius()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(KorrynCursedWardSeed), false, this);
	QueryParams.AddIgnoredActor(this);

	World->OverlapMultiByChannel(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(SlowRadius),
		QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ABwayCharacterWithAbilities* Enemy = Cast<ABwayCharacterWithAbilities>(Overlap.GetActor());
		if (Enemy && IsEnemyOfWard(Enemy) && IsAliveEnemy(Enemy))
		{
			ApplySlowToEnemy(Enemy);
		}
	}
}

void ABwayKorrynCursedWardBuildable::OnSlowSphereBeginOverlap(
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

	if (!HasAuthority() || !bSlowAuraActive || bDeathHandled)
	{
		return;
	}

	ABwayCharacterWithAbilities* OtherCharacter = Cast<ABwayCharacterWithAbilities>(OtherActor);
	if (OtherCharacter && IsEnemyOfWard(OtherCharacter) && IsAliveEnemy(OtherCharacter))
	{
		ApplySlowToEnemy(OtherCharacter);
	}
}

void ABwayKorrynCursedWardBuildable::OnSlowSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	if (!HasAuthority())
	{
		return;
	}

	if (ABwayCharacterWithAbilities* OtherCharacter = Cast<ABwayCharacterWithAbilities>(OtherActor))
	{
		RemoveSlowFromEnemy(OtherCharacter);
	}
}

void ABwayKorrynCursedWardBuildable::ApplySlowToEnemy(ABwayCharacterWithAbilities* Enemy)
{
	if (!Enemy || ActiveEnemySlowHandles.Contains(Enemy) || !SlowEffectClass)
	{
		return;
	}

	// Use the ward's own ASC so the aura survives owner-pawn replacement between rounds.
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	AActor* EffectInstigator = GetInstigator();
	if (!EffectInstigator)
	{
		if (AController* OwningController = Cast<AController>(GetOwner()))
		{
			EffectInstigator = OwningController->GetPawn();
		}
	}
	if (!EffectInstigator)
	{
		EffectInstigator = this;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(this);
	Context.AddInstigator(EffectInstigator, this);

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(SlowEffectClass, 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(TAG_SetByCaller_Korryn_MoveSpeedMultiplier, SlowMultiplier);
	const FActiveGameplayEffectHandle Handle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	if (Handle.IsValid())
	{
		ActiveEnemySlowHandles.Add(Enemy, Handle);
	}
}

void ABwayKorrynCursedWardBuildable::RemoveSlowFromEnemy(ABwayCharacterWithAbilities* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	FActiveGameplayEffectHandle* Handle = ActiveEnemySlowHandles.Find(Enemy);
	if (!Handle)
	{
		return;
	}

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy))
	{
		if (Handle->IsValid())
		{
			TargetASC->RemoveActiveGameplayEffect(*Handle);
		}
	}

	ActiveEnemySlowHandles.Remove(Enemy);
}

void ABwayKorrynCursedWardBuildable::ClearAllSlowEffects()
{
	TArray<TWeakObjectPtr<ABwayCharacterWithAbilities>> Enemies;
	ActiveEnemySlowHandles.GetKeys(Enemies);
	for (const TWeakObjectPtr<ABwayCharacterWithAbilities>& WeakEnemy : Enemies)
	{
		if (ABwayCharacterWithAbilities* Enemy = WeakEnemy.Get())
		{
			RemoveSlowFromEnemy(Enemy);
		}
	}
	ActiveEnemySlowHandles.Reset();
}

AActor* ABwayKorrynCursedWardBuildable::ResolveTeamProxy() const
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

bool ABwayKorrynCursedWardBuildable::IsEnemyOfWard(ABwayCharacterWithAbilities* OtherCharacter) const
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

	// Prefer owner pawn / instigator (Alona shrine pattern). When the placer pawn is
	// replaced between rounds, fall back to the ward's stored TeamId so the aura keeps working.
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
		const int32 WardTeam = static_cast<int32>(GetTeamId().GetId());
		const int32 OtherTeam = TeamSubsystem->FindTeamFromObject(OtherCharacter);
		return OtherTeam != INDEX_NONE && WardTeam != 0 && OtherTeam != WardTeam;
	}

	return false;
}

bool ABwayKorrynCursedWardBuildable::IsAliveEnemy(ABwayCharacterWithAbilities* Enemy) const
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

void ABwayKorrynCursedWardBuildable::HandleOutOfHealth(
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

	HandleDeathStarted(this);
}

void ABwayKorrynCursedWardBuildable::HandleDeathStarted(AActor* OwningActor)
{
	(void)OwningActor;

	if (!HasAuthority() || bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;
	StopSlowAura();
	ClearAllSlowEffects();
	Destroy();
}
