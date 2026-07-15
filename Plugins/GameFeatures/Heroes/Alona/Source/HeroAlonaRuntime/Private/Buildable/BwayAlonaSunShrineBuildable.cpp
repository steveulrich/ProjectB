#include "Buildable/BwayAlonaSunShrineBuildable.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "Character/LyraHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "LyraGameplayTags.h"
#include "System/LyraAssetManager.h"
#include "System/LyraGameData.h"
#include "Teams/LyraTeamSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAlonaSunShrineBuildable)

ABwayAlonaSunShrineBuildable::ABwayAlonaSunShrineBuildable()
{
	bPersistsBetweenRounds = true;
	BuildTime = 0.f;
	PrimaryActorTick.bCanEverTick = false;

	HealSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HealSphere"));
	HealSphere->SetupAttachment(RootComponent);
	HealSphere->InitSphereRadius(HealRadius);
	HealSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HealSphere->SetCollisionObjectType(ECC_WorldDynamic);
	HealSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	HealSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HealSphere->SetGenerateOverlapEvents(true);
}

void ABwayAlonaSunShrineBuildable::InitializeAbilitySystem()
{
	Super::InitializeAbilitySystem();

	if (HealthSet)
	{
		HealthSet->InitMaxHealth(MaxHealth);
		HealthSet->InitHealth(MaxHealth);
		HealthSet->OnOutOfHealth.AddUObject(this, &ABwayAlonaSunShrineBuildable::HandleOutOfHealth);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.AddDynamic(this, &ABwayAlonaSunShrineBuildable::HandleDeathStarted);
	}

	if (HealSphere)
	{
		HealSphere->SetSphereRadius(HealRadius);
	}
}

void ABwayAlonaSunShrineBuildable::BeginPlay()
{
	Super::BeginPlay();

	if (HealSphere)
	{
		HealSphere->SetSphereRadius(HealRadius);
		HealSphere->OnComponentBeginOverlap.AddDynamic(this, &ABwayAlonaSunShrineBuildable::OnHealSphereBeginOverlap);
		HealSphere->OnComponentEndOverlap.AddDynamic(this, &ABwayAlonaSunShrineBuildable::OnHealSphereEndOverlap);
	}

	// BuildTime == 0 finishes in Super::BeginPlay → FinishBuilding → StartHealAura.
	// If already active (e.g. round persist with BuildTime already complete), ensure aura runs.
	if (HasAuthority() && bIsActive && !bHealAuraActive)
	{
		StartHealAura();
	}
}

void ABwayAlonaSunShrineBuildable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopHealAura();
	AlliesInRadius.Reset();

	if (HealthSet)
	{
		HealthSet->OnOutOfHealth.RemoveAll(this);
	}

	if (HealthComponent)
	{
		HealthComponent->OnDeathStarted.RemoveDynamic(this, &ABwayAlonaSunShrineBuildable::HandleDeathStarted);
	}

	Super::EndPlay(EndPlayReason);
}

void ABwayAlonaSunShrineBuildable::FinishBuilding()
{
	Super::FinishBuilding();

	if (HasAuthority())
	{
		StartHealAura();
	}
}

void ABwayAlonaSunShrineBuildable::StartHealAura()
{
	if (!HasAuthority() || bHealAuraActive || !bIsActive || bDeathHandled)
	{
		return;
	}

	bHealAuraActive = true;
	SeedAlliesAlreadyInRadius();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealTickTimerHandle);
		World->GetTimerManager().SetTimer(
			HealTickTimerHandle,
			this,
			&ABwayAlonaSunShrineBuildable::TickHeal,
			1.f,
			true);
	}
}

void ABwayAlonaSunShrineBuildable::StopHealAura()
{
	bHealAuraActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealTickTimerHandle);
	}
}

void ABwayAlonaSunShrineBuildable::SeedAlliesAlreadyInRadius()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AlonaSunShrineSeed), false, this);
	QueryParams.AddIgnoredActor(this);

	World->OverlapMultiByChannel(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(HealRadius),
		QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ABwayCharacterWithAbilities* Ally = Cast<ABwayCharacterWithAbilities>(Overlap.GetActor());
		if (Ally && IsAllyOfShrine(Ally) && IsAliveAlly(Ally))
		{
			AlliesInRadius.Add(Ally);
		}
	}
}

void ABwayAlonaSunShrineBuildable::OnHealSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	ABwayCharacterWithAbilities* OtherCharacter = Cast<ABwayCharacterWithAbilities>(OtherActor);
	if (!OtherCharacter || !IsAllyOfShrine(OtherCharacter) || !IsAliveAlly(OtherCharacter))
	{
		return;
	}

	AlliesInRadius.Add(OtherCharacter);
}

void ABwayAlonaSunShrineBuildable::OnHealSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ABwayCharacterWithAbilities* OtherCharacter = Cast<ABwayCharacterWithAbilities>(OtherActor))
	{
		AlliesInRadius.Remove(OtherCharacter);
	}
}

void ABwayAlonaSunShrineBuildable::TickHeal()
{
	if (!HasAuthority() || !bHealAuraActive || !bIsActive || HealPerSecond <= 0.f || bDeathHandled)
	{
		return;
	}

	TArray<TWeakObjectPtr<ABwayCharacterWithAbilities>> Snapshot;
	Snapshot.Reserve(AlliesInRadius.Num());
	for (const TObjectPtr<ABwayCharacterWithAbilities>& AllyPtr : AlliesInRadius)
	{
		Snapshot.Add(AllyPtr.Get());
	}

	for (const TWeakObjectPtr<ABwayCharacterWithAbilities>& AllyPtr : Snapshot)
	{
		ABwayCharacterWithAbilities* Ally = AllyPtr.Get();
		if (!IsValid(Ally) || !IsAllyOfShrine(Ally) || !IsAliveAlly(Ally))
		{
			AlliesInRadius.Remove(Ally);
			continue;
		}

		if (ShouldThisShrineHealAlly(Ally))
		{
			ApplyHealToAlly(Ally, HealPerSecond);
		}
	}
}

void ABwayAlonaSunShrineBuildable::ApplyHealToAlly(ABwayCharacterWithAbilities* Ally, float HealAmount)
{
	if (!Ally || HealAmount <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Ally);
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	const ULyraGameData& GameData = ULyraGameData::Get();
	TSubclassOf<UGameplayEffect> HealEffectClass = ULyraAssetManager::GetSubclass(GameData.HealGameplayEffect_SetByCaller);
	if (!HealEffectClass)
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

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	EffectContext.AddInstigator(EffectInstigator, this);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(HealEffectClass, 1.f, EffectContext);
	if (SpecHandle.IsValid())
	{
		if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
		{
			Spec->SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Heal, HealAmount);
		}
		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

bool ABwayAlonaSunShrineBuildable::IsAllyOfShrine(ABwayCharacterWithAbilities* OtherCharacter) const
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

	// Buildables are not Lyra team agents — compare via owner pawn / instigator (BlessingZone pattern).
	// Do not use IGenericTeamAgentInterface / FGenericTeamId::NoTeam here: HeroAlonaRuntime does not link AIModule (BF-017).
	AActor* TeamProxy = nullptr;
	if (AController* OwningController = Cast<AController>(GetOwner()))
	{
		TeamProxy = OwningController->GetPawn();
	}
	if (!TeamProxy)
	{
		TeamProxy = GetInstigator();
	}
	if (!TeamProxy)
	{
		return false;
	}

	// Owner pawn itself is always an ally of its shrine.
	if (TeamProxy == OtherCharacter)
	{
		return true;
	}

	if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
	{
		return GameState->AreOnSameTeam(TeamProxy, OtherCharacter);
	}

	if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
	{
		return TeamSubsystem->CompareTeams(TeamProxy, OtherCharacter) == ELyraTeamComparison::OnSameTeam;
	}

	return false;
}

bool ABwayAlonaSunShrineBuildable::IsAliveAlly(ABwayCharacterWithAbilities* Ally) const
{
	if (!Ally)
	{
		return false;
	}

	if (const ULyraHealthComponent* AllyHealth = ULyraHealthComponent::FindHealthComponent(Ally))
	{
		return !AllyHealth->IsDeadOrDying();
	}

	return true;
}

bool ABwayAlonaSunShrineBuildable::IsActiveSunShrine() const
{
	return IsValid(this)
		&& !bDeathHandled
		&& bIsActive
		&& !IsActorBeingDestroyed();
}

bool ABwayAlonaSunShrineBuildable::ShouldThisShrineHealAlly(ABwayCharacterWithAbilities* Ally) const
{
	if (!Ally || !IsActiveSunShrine())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	const FVector AllyLocation = Ally->GetActorLocation();
	const float MyDistSq = FVector::DistSquared(GetActorLocation(), AllyLocation);
	const float HealRadiusSq = FMath::Square(HealRadius);

	if (MyDistSq > HealRadiusSq)
	{
		return false;
	}

	// Closest active friendly Sun Shrine that covers the ally owns the heal (non-stack).
	for (TActorIterator<ABwayAlonaSunShrineBuildable> It(World); It; ++It)
	{
		const ABwayAlonaSunShrineBuildable* OtherShrine = *It;
		if (!OtherShrine || OtherShrine == this || !OtherShrine->IsActiveSunShrine())
		{
			continue;
		}

		if (OtherShrine->GetTeamId() != GetTeamId())
		{
			continue;
		}

		const float OtherDistSq = FVector::DistSquared(OtherShrine->GetActorLocation(), AllyLocation);
		if (OtherDistSq > HealRadiusSq)
		{
			continue;
		}

		if (OtherDistSq < MyDistSq)
		{
			return false;
		}

		// Tie-break: lower pointer address wins so exactly one shrine applies.
		if (FMath::IsNearlyEqual(OtherDistSq, MyDistSq) && OtherShrine < this)
		{
			return false;
		}
	}

	return true;
}

void ABwayAlonaSunShrineBuildable::HandleOutOfHealth(
	AActor* EffectInstigator,
	AActor* EffectCauser,
	const FGameplayEffectSpec* EffectSpec,
	float EffectMagnitude,
	float OldValue,
	float NewValue)
{
	HandleDeathStarted(this);
}

void ABwayAlonaSunShrineBuildable::HandleDeathStarted(AActor* OwningActor)
{
	if (!HasAuthority() || bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;
	StopHealAura();
	AlliesInRadius.Reset();
	Destroy();
}
