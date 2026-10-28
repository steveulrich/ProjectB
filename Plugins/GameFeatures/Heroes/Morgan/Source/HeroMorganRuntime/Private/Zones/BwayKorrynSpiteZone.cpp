#include "Zones/BwayKorrynSpiteZone.h"

#include "Abilities/BwayGameplayEffect_KorrynEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "Combat/BwayCuePresentationComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayKorrynSpiteZone)

ABwayKorrynSpiteZone::ABwayKorrynSpiteZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);

	EffectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("EffectSphere"));
	SetRootComponent(EffectSphere);
	EffectSphere->InitSphereRadius(ReplicatedRadius);
	EffectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EffectSphere->SetCollisionObjectType(ECC_WorldDynamic);
	EffectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	EffectSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EffectSphere->SetGenerateOverlapEvents(true);
	Presentation = CreateDefaultSubobject<UBwayCuePresentationComponent>(TEXT("Presentation"));
	Presentation->bFollowOwnerLifetime = true;
	Presentation->Cue_Active = BwayGameplayCueTags::ZoneHarmful;
	Presentation->CueAttachComponent = EffectSphere;
}

void ABwayKorrynSpiteZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwayKorrynSpiteZone, ReplicatedRadius);
}

void ABwayKorrynSpiteZone::ConfigureZone(
	ABwayCharacterWithAbilities* InCaster,
	float InRadius,
	float InDuration,
	TSubclassOf<UGameplayEffect> InSlowEffectClass,
	TSubclassOf<UGameplayEffect> InDamageAmpEffectClass,
	float InSlowMultiplier,
	float InDamageAmpMultiplier)
{
	CasterPlayerState = InCaster ? InCaster->GetPlayerState<ABwayPlayerState>() : nullptr;
	CasterAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CasterPlayerState.Get());
	ReplicatedRadius = InRadius;
	SlowEffectClass = InSlowEffectClass;
	DamageAmpEffectClass = InDamageAmpEffectClass;
	SlowMultiplier = InSlowMultiplier;
	DamageAmpMultiplier = InDamageAmpMultiplier;

	if (InCaster)
	{
		SetOwner(InCaster);
		SetInstigator(InCaster);
	}

	if (EffectSphere)
	{
		EffectSphere->SetSphereRadius(InRadius);
	}

	SetLifeSpan(InDuration);
}

void ABwayKorrynSpiteZone::BeginPlay()
{
	Super::BeginPlay();

	if (EffectSphere)
	{
		EffectSphere->SetSphereRadius(ReplicatedRadius > 0.f ? ReplicatedRadius : 700.f);
		EffectSphere->OnComponentBeginOverlap.AddDynamic(this, &ABwayKorrynSpiteZone::OnSphereBeginOverlap);
		EffectSphere->OnComponentEndOverlap.AddDynamic(this, &ABwayKorrynSpiteZone::OnSphereEndOverlap);
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
			Manager->OnRoundStateChanged.AddDynamic(this, &ABwayKorrynSpiteZone::OnRoundStateChanged);
			RoundResetHandle = Manager->OnRoundResetStarting.AddUObject(this, &ABwayKorrynSpiteZone::RemoveForRoundTransition);
		}
	}

	// Catch enemies already overlapping at spawn.
	TArray<AActor*> Overlapping;
	EffectSphere->GetOverlappingActors(Overlapping, ABwayCharacterWithAbilities::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		if (ABwayCharacterWithAbilities* Character = Cast<ABwayCharacterWithAbilities>(Actor))
		{
			if (IsEnemyCharacter(Character))
			{
				ApplyZoneEffects(Character);
			}
		}
	}
}

void ABwayKorrynSpiteZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBwayRoundManagementComponent* Manager = RoundManager.Get())
	{
		Manager->OnRoundStateChanged.RemoveDynamic(this, &ABwayKorrynSpiteZone::OnRoundStateChanged);
		Manager->OnRoundResetStarting.Remove(RoundResetHandle);
	}
	RoundResetHandle.Reset();
	RoundManager.Reset();
	ClearAllZoneEffects();
	Super::EndPlay(EndPlayReason);
}

void ABwayKorrynSpiteZone::OnRoundStateChanged(FName NewRoundState)
{
	if (NewRoundState != TEXT("RoundActive"))
	{
		RemoveForRoundTransition();
	}
}

void ABwayKorrynSpiteZone::RemoveForRoundTransition()
{
	if (HasAuthority())
	{
		bRemovingForRoundTransition = true;
		Destroy();
	}
}

void ABwayKorrynSpiteZone::OnSphereBeginOverlap(
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

	if (!HasAuthority())
	{
		return;
	}

	ABwayCharacterWithAbilities* Character = Cast<ABwayCharacterWithAbilities>(OtherActor);
	if (Character && IsEnemyCharacter(Character))
	{
		ApplyZoneEffects(Character);
	}
}

void ABwayKorrynSpiteZone::OnSphereEndOverlap(
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

	ABwayCharacterWithAbilities* Character = Cast<ABwayCharacterWithAbilities>(OtherActor);
	if (Character)
	{
		RemoveZoneEffects(Character);
	}
}

bool ABwayKorrynSpiteZone::IsEnemyCharacter(ABwayCharacterWithAbilities* Other) const
{
	const ABwayPlayerState* SourcePlayer = CasterPlayerState.Get();
	if (!SourcePlayer || !IsValid(Other) || Other->IsActorBeingDestroyed())
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			const int32 SourceTeam = GameState->GetPlayerTeam(SourcePlayer);
			const int32 TargetTeam = GameState->GetPlayerTeam(Other->GetPlayerState());
			return GameState->PlayerArray.Contains(SourcePlayer)
				&& SourceTeam != INDEX_NONE && TargetTeam != INDEX_NONE && SourceTeam != TargetTeam;
		}
		if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
		{
			return TeamSubsystem->CompareTeams(SourcePlayer, Other) == ELyraTeamComparison::DifferentTeams;
		}
	}

	return false;
}

void ABwayKorrynSpiteZone::ApplyZoneEffects(ABwayCharacterWithAbilities* Enemy)
{
	if (!HasAuthority() || bRemovingForRoundTransition || IsActorBeingDestroyed()
		|| GetLifeSpan() <= 0.f || !IsEnemyCharacter(Enemy) || ActiveEnemyEffects.Contains(Enemy))
	{
		return;
	}

	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Enemy);
	if (!Health || Health->IsDeadOrDying() || Health->GetHealth() <= 0.f)
	{
		return;
	}
	UAbilitySystemComponent* SourceASC = CasterAbilitySystem.Get();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
	if (!SourceASC || SourceASC->GetOwnerActor() != CasterPlayerState.Get()
		|| !TargetASC || TargetASC->GetAvatarActor() != Enemy)
	{
		return;
	}

	FZoneEffectHandles Handles;
	// The ASC outlives its pawn. Keep the actual recipient for exact cleanup on exit/expiry/reset.
	Handles.TargetAbilitySystem = TargetASC;

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(this);
	Context.AddInstigator(CasterPlayerState.Get(), this);

	if (SlowEffectClass)
	{
		const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(SlowEffectClass, 1.f, Context);
		if (Spec.IsValid() && Spec.Data.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(TAG_SetByCaller_Korryn_MoveSpeedMultiplier, SlowMultiplier);
			Handles.SlowHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	if (bRemovingForRoundTransition || IsActorBeingDestroyed())
	{
		RemoveEffectHandles(Handles);
		return;
	}

	if (DamageAmpEffectClass)
	{
		const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageAmpEffectClass, 1.f, Context);
		if (Spec.IsValid() && Spec.Data.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(TAG_SetByCaller_Korryn_IncomingDamageMultiplier, DamageAmpMultiplier);
			Handles.AmpHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	if (bRemovingForRoundTransition || IsActorBeingDestroyed())
	{
		RemoveEffectHandles(Handles);
		return;
	}
	ActiveEnemyEffects.Add(Enemy, Handles);
}

void ABwayKorrynSpiteZone::RemoveZoneEffects(ABwayCharacterWithAbilities* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	FZoneEffectHandles* Handles = ActiveEnemyEffects.Find(Enemy);
	if (!Handles)
	{
		return;
	}

	const FZoneEffectHandles RemovedHandles = *Handles;
	ActiveEnemyEffects.Remove(Enemy);
	RemoveEffectHandles(RemovedHandles);
}

void ABwayKorrynSpiteZone::RemoveEffectHandles(const FZoneEffectHandles& Handles)
{
	if (UAbilitySystemComponent* TargetASC = Handles.TargetAbilitySystem.Get())
	{
		if (Handles.SlowHandle.IsValid())
		{
			TargetASC->RemoveActiveGameplayEffect(Handles.SlowHandle);
		}
		if (Handles.AmpHandle.IsValid())
		{
			TargetASC->RemoveActiveGameplayEffect(Handles.AmpHandle);
		}
	}
}

void ABwayKorrynSpiteZone::ClearAllZoneEffects()
{
	const TMap<TWeakObjectPtr<ABwayCharacterWithAbilities>, FZoneEffectHandles> RemovedEffects = MoveTemp(ActiveEnemyEffects);
	ActiveEnemyEffects.Reset();
	for (const auto& Entry : RemovedEffects)
	{
		RemoveEffectHandles(Entry.Value);
	}
}
