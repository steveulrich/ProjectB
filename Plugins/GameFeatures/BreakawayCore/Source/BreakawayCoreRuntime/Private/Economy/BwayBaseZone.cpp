#include "Economy/BwayBaseZone.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "BwayGameState.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "LyraGameplayTags.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBaseZone)

const FName UBwayGameplayEffect_BaseHealing::HealingMagnitudeName(TEXT("BaseHealing"));

UBwayGameplayEffect_BaseHealing::UBwayGameplayEffect_BaseHealing()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FSetByCallerFloat Magnitude;
	Magnitude.DataName = HealingMagnitudeName;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = ULyraHealthSet::GetHealingAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
}

ABwayBaseZone::ABwayBaseZone()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	SetRootComponent(Bounds);
	Bounds->InitBoxExtent(FVector(500.0f, 500.0f, 300.0f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);
}

void ABwayBaseZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwayBaseZone, TeamIndex);
	DOREPLIFETIME(ABwayBaseZone, HealingPerSecond);
	DOREPLIFETIME(ABwayBaseZone, bEnabled);
}

bool ABwayBaseZone::CanUseBase(const APlayerState* Player) const
{
	if (!bEnabled || !IsValid(Player) || Player->GetWorld() != GetWorld() || TeamIndex < 0 || TeamIndex > 1) return false;
	const ABwayGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	const UBwayRoundManagementComponent* Rounds = GS ? GS->GetRoundManagement() : nullptr;
	if (!Rounds || GS->GetPlayerTeam(Player) != TeamIndex) return false;
	const EBwayMatchPhase Phase = Rounds->GetCurrentMatchPhase();
	if (Phase != EBwayMatchPhase::Playing && Phase != EBwayMatchPhase::Warmup) return false;
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player, true);
	const ULyraHealthSet* Health = ASC ? ASC->GetSet<ULyraHealthSet>() : nullptr;
	const APawn* Pawn = ASC ? Cast<APawn>(ASC->GetAvatarActor()) : nullptr;
	if (!IsValid(Pawn) || Pawn->GetPlayerState() != Player || !Health || !FMath::IsFinite(Health->GetHealth())
		|| Health->GetHealth() <= 0.0f || ASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death)) return false;
	const FVector Position = Bounds->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	return !Position.ContainsNaN() && FMath::Abs(Position.X) <= Extent.X
		&& FMath::Abs(Position.Y) <= Extent.Y && FMath::Abs(Position.Z) <= Extent.Z;
}

void ABwayBaseZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bEnabled || !FMath::IsFinite(HealingPerSecond) || HealingPerSecond <= 0.0f) return;
	const ABwayGameState* GS = GetWorld()->GetGameState<ABwayGameState>();
	if (!GS) return;
	// Bound catch-up after a hitch; never heal for time spent outside the zone.
	const float Amount = HealingPerSecond * FMath::Clamp(DeltaSeconds, 0.0f, 0.25f);
	if (!FMath::IsFinite(Amount) || Amount <= 0.0f) return;
	for (APlayerState* Player : GS->PlayerArray)
	{
		if (!CanUseBase(Player)) continue;
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player, true);
		const ULyraHealthSet* Health = ASC->GetSet<ULyraHealthSet>();
		if (Health->GetHealth() >= Health->GetMaxHealth()) continue;
		FGameplayEffectSpec Spec(GetDefault<UBwayGameplayEffect_BaseHealing>(), ASC->MakeEffectContext(), 1.0f);
		Spec.SetSetByCallerMagnitude(UBwayGameplayEffect_BaseHealing::HealingMagnitudeName, Amount);
		ASC->ApplyGameplayEffectSpecToSelf(Spec);
	}
}
