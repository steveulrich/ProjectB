#include "Zones/BwayAlonaBlessingZone.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "LyraGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "System/LyraAssetManager.h"
#include "System/LyraGameData.h"
#include "Teams/LyraTeamSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAlonaBlessingZone)

ABwayAlonaBlessingZone::ABwayAlonaBlessingZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	HealSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HealSphere"));
	SetRootComponent(HealSphere);
	HealSphere->InitSphereRadius(HealRadius);
	HealSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HealSphere->SetCollisionObjectType(ECC_WorldDynamic);
	HealSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	HealSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HealSphere->SetGenerateOverlapEvents(true);
}

void ABwayAlonaBlessingZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwayAlonaBlessingZone, ReplicatedRadius);
}

void ABwayAlonaBlessingZone::ConfigureZone(
	ABwayCharacterWithAbilities* InCaster,
	float InRadius,
	float InDuration,
	float InInitialHeal,
	float InHealPerSecond)
{
	CasterPlayerState = InCaster ? InCaster->GetPlayerState<ABwayPlayerState>() : nullptr;
	CasterAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CasterPlayerState.Get());
	HealRadius = InRadius;
	ZoneDuration = InDuration;
	InitialHeal = InInitialHeal;
	HealPerSecond = InHealPerSecond;
	ReplicatedRadius = InRadius;

	if (InCaster)
	{
		// The cast belongs to the persistent player, including after pawn death.
		SetOwner(CasterPlayerState.Get());
		SetInstigator(InCaster);
	}

	if (HealSphere)
	{
		HealSphere->SetSphereRadius(HealRadius);
	}

	SetLifeSpan(ZoneDuration);
}

void ABwayAlonaBlessingZone::BeginPlay()
{
	Super::BeginPlay();

	if (HealSphere)
	{
		HealSphere->SetSphereRadius(ReplicatedRadius > 0.f ? ReplicatedRadius : HealRadius);
		HealSphere->OnComponentBeginOverlap.AddDynamic(this, &ABwayAlonaBlessingZone::OnSphereBeginOverlap);
		HealSphere->OnComponentEndOverlap.AddDynamic(this, &ABwayAlonaBlessingZone::OnSphereEndOverlap);
	}

	if (!HasAuthority())
	{
		return;
	}

	if (const ABwayGameState* GameState = GetWorld()->GetGameState<ABwayGameState>())
	{
		RoundManager = GameState->FindComponentByClass<UBwayRoundManagementComponent>();
		if (UBwayRoundManagementComponent* Manager = RoundManager.Get())
		{
			Manager->OnRoundStateChanged.AddDynamic(this, &ABwayAlonaBlessingZone::OnRoundStateChanged);
			RoundResetHandle = Manager->OnRoundResetStarting.AddUObject(this, &ABwayAlonaBlessingZone::RemoveForRoundTransition);
		}
	}

	ApplyInitialHeals();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealTickTimerHandle);
		World->GetTimerManager().SetTimer(
			HealTickTimerHandle,
			this,
			&ABwayAlonaBlessingZone::TickHeal,
			1.f,
			true);
	}
}

void ABwayAlonaBlessingZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBwayRoundManagementComponent* Manager = RoundManager.Get())
	{
		Manager->OnRoundStateChanged.RemoveDynamic(this, &ABwayAlonaBlessingZone::OnRoundStateChanged);
		Manager->OnRoundResetStarting.Remove(RoundResetHandle);
	}
	RoundResetHandle.Reset();
	RoundManager.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealTickTimerHandle);
	}

	AlliesInZone.Reset();
	Super::EndPlay(EndPlayReason);
}

void ABwayAlonaBlessingZone::OnRoundStateChanged(FName NewRoundState)
{
	if (NewRoundState != TEXT("RoundActive"))
	{
		RemoveForRoundTransition();
	}
}

void ABwayAlonaBlessingZone::RemoveForRoundTransition()
{
	if (HasAuthority())
	{
		Destroy();
	}
}

void ABwayAlonaBlessingZone::OnSphereBeginOverlap(
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
	if (!OtherCharacter || !IsAllyOfCaster(OtherCharacter))
	{
		return;
	}

	AlliesInZone.Add(OtherCharacter);
}

void ABwayAlonaBlessingZone::OnSphereEndOverlap(
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
		AlliesInZone.Remove(OtherCharacter);
	}
}

void ABwayAlonaBlessingZone::ApplyInitialHeals()
{
	UWorld* World = GetWorld();
	if (!World || !CasterPlayerState.IsValid())
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AlonaBlessingInitial), false, this);
	QueryParams.AddIgnoredActor(this);

	World->OverlapMultiByChannel(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(HealRadius),
		QueryParams);

	TSet<ABwayCharacterWithAbilities*> InitiallyHealed;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ABwayCharacterWithAbilities* Ally = Cast<ABwayCharacterWithAbilities>(Overlap.GetActor());
		if (!Ally || !IsAllyOfCaster(Ally) || InitiallyHealed.Contains(Ally))
		{
			continue;
		}

		InitiallyHealed.Add(Ally);
		AlliesInZone.Add(Ally);
		ApplyHealToAlly(Ally, InitialHeal);
	}
}

void ABwayAlonaBlessingZone::TickHeal()
{
	// Lifespan and periodic timers can share a deadline; never heal at/after expiry.
	if (!HasAuthority() || HealPerSecond <= 0.f || GetLifeSpan() <= 0.f)
	{
		return;
	}

	TArray<TWeakObjectPtr<ABwayCharacterWithAbilities>> Snapshot;
	Snapshot.Reserve(AlliesInZone.Num());
	for (const TObjectPtr<ABwayCharacterWithAbilities>& AllyPtr : AlliesInZone)
	{
		Snapshot.Add(AllyPtr.Get());
	}

	for (const TWeakObjectPtr<ABwayCharacterWithAbilities>& AllyPtr : Snapshot)
	{
		ABwayCharacterWithAbilities* Ally = AllyPtr.Get();
		if (IsValid(Ally) && IsAllyOfCaster(Ally))
		{
			ApplyHealToAlly(Ally, HealPerSecond);
		}
	}
}

void ABwayAlonaBlessingZone::ApplyHealToAlly(ABwayCharacterWithAbilities* Ally, float HealAmount)
{
	if (!IsValid(Ally) || HealAmount <= 0.f || !IsAllyOfCaster(Ally))
	{
		return;
	}

	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Ally);
	if (!Health || Health->IsDeadOrDying() || Health->GetHealth() <= 0.f)
	{
		return;
	}
	UAbilitySystemComponent* SourceASC = CasterAbilitySystem.Get();
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

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	EffectContext.AddInstigator(CasterPlayerState.Get(), this);

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

bool ABwayAlonaBlessingZone::IsAllyOfCaster(ABwayCharacterWithAbilities* OtherCharacter) const
{
	const ABwayPlayerState* SourcePlayer = CasterPlayerState.Get();
	if (!SourcePlayer || !IsValid(OtherCharacter))
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			return GameState->PlayerArray.Contains(SourcePlayer)
				&& GameState->AreOnSameTeam(SourcePlayer, OtherCharacter);
		}

		if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
		{
			return TeamSubsystem->CompareTeams(SourcePlayer, OtherCharacter) == ELyraTeamComparison::OnSameTeam;
		}
	}

	return false;
}
