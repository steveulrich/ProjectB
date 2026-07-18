#include "Zones/BwayKorrynSpiteZone.h"

#include "Abilities/BwayGameplayEffect_KorrynEffects.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "Components/SphereComponent.h"
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
	CasterCharacter = InCaster;
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
	ClearAllZoneEffects();
	Super::EndPlay(EndPlayReason);
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
	if (!CasterCharacter || !Other || Other == CasterCharacter)
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		if (const ABwayGameState* GameState = World->GetGameState<ABwayGameState>())
		{
			return !GameState->AreOnSameTeam(CasterCharacter, Other);
		}
		if (const ULyraTeamSubsystem* TeamSubsystem = World->GetSubsystem<ULyraTeamSubsystem>())
		{
			return TeamSubsystem->CompareTeams(CasterCharacter, Other) == ELyraTeamComparison::DifferentTeams;
		}
	}

	return false;
}

void ABwayKorrynSpiteZone::ApplyZoneEffects(ABwayCharacterWithAbilities* Enemy)
{
	if (!Enemy || ActiveEnemyEffects.Contains(Enemy))
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CasterCharacter);
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy);
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	FZoneEffectHandles Handles;

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(this);
	Context.AddInstigator(CasterCharacter, this);

	if (SlowEffectClass)
	{
		const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(SlowEffectClass, 1.f, Context);
		if (Spec.IsValid() && Spec.Data.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(TAG_SetByCaller_Korryn_MoveSpeedMultiplier, SlowMultiplier);
			Handles.SlowHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
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

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Enemy))
	{
		if (Handles->SlowHandle.IsValid())
		{
			TargetASC->RemoveActiveGameplayEffect(Handles->SlowHandle);
		}
		if (Handles->AmpHandle.IsValid())
		{
			TargetASC->RemoveActiveGameplayEffect(Handles->AmpHandle);
		}
	}

	ActiveEnemyEffects.Remove(Enemy);
}

void ABwayKorrynSpiteZone::ClearAllZoneEffects()
{
	TArray<TWeakObjectPtr<ABwayCharacterWithAbilities>> Enemies;
	ActiveEnemyEffects.GetKeys(Enemies);
	for (const TWeakObjectPtr<ABwayCharacterWithAbilities>& WeakEnemy : Enemies)
	{
		if (ABwayCharacterWithAbilities* Enemy = WeakEnemy.Get())
		{
			RemoveZoneEffects(Enemy);
		}
	}
	ActiveEnemyEffects.Reset();
}
