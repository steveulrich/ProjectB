#include "Development/BwayAllKitCombatProbe.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AIController.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BrainComponent.h"
#include "BwayCharacterMovementComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayHeroProjectileBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Containers/Ticker.h"
#include "Economy/BwayBaseZone.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeComponent.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "LyraGameplayTags.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/ObjectKey.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAllKitCombatProbe)

namespace BwayAllKitOwner
{
constexpr float AllKitObservedRowTolerance = 0.15f;
constexpr int32 Immunity = 1, Ethereal = 2, Slowed = 4, Amplified = 8, Silenced = 16, Shredded = 32;
struct FCase { const TCHAR* Hero; const TCHAR* Ability; float Duration; };
const FCase Cases[] = {
	{TEXT("Argus"), TEXT("BwayGameplayAbility_MeleePrimary"), .5f},
	{TEXT("Argus"), TEXT("BwayGameplayAbility_ArgusSlide"), .9f},
	{TEXT("Argus"), TEXT("BwayGameplayAbility_NoRetreat"), .9f},
	{TEXT("Argus"), TEXT("BwayGameplayAbility_ForGlory"), .5f},
	{TEXT("Argus"), TEXT("BwayGameplayAbility_Retribution"), .9f},
	{TEXT("Alona"), TEXT("BwayGameplayAbility_AlonaPrimary"), .6f},
	{TEXT("Alona"), TEXT("BwayGameplayAbility_AlonaSunsGrace"), .7f},
	{TEXT("Alona"), TEXT("BwayGameplayAbility_AlonaRadiance"), 1.35f},
	{TEXT("Alona"), TEXT("BwayGameplayAbility_AlonaSunBurst"), .5f},
	{TEXT("Alona"), TEXT("BwayGameplayAbility_AlonaBlessing"), 1.35f},
	{TEXT("Korryn"), TEXT("BwayGameplayAbility_KorrynPrimary"), .6f},
	{TEXT("Korryn"), TEXT("BwayGameplayAbility_KorrynFlock"), 1.9f},
	{TEXT("Korryn"), TEXT("BwayGameplayAbility_KorrynBurdenOfSin"), .5f},
	{TEXT("Korryn"), TEXT("BwayGameplayAbility_KorrynCircleOfSpite"), .5f},
	{TEXT("Korryn"), TEXT("BwayGameplayAbility_KorrynAuraOfSilence"), .5f},
	{TEXT("Rawlins"), TEXT("BwayGameplayAbility_RawlinsPrimary"), .6f},
	{TEXT("Rawlins"), TEXT("BwayGameplayAbility_RawlinsDoubleDown"), .9f},
	{TEXT("Rawlins"), TEXT("BwayGameplayAbility_RawlinsPowerShot"), .5f},
	{TEXT("Rawlins"), TEXT("BwayGameplayAbility_RawlinsSlideShot"), .9f},
	{TEXT("Rawlins"), TEXT("BwayGameplayAbility_RawlinsBlazingBarrage"), 1.8f}
};
ULyraAbilitySystemComponent* ASC(ABwayPlayerState* P) { return P ? P->GetLyraAbilitySystemComponent() : nullptr; }
ABwayCharacterWithAbilities* Pawn(ABwayPlayerState* P) { return P ? Cast<ABwayCharacterWithAbilities>(P->GetPawn()) : nullptr; }
bool Family(const UClass* C, FName Name) { for (; C; C = C->GetSuperClass()) if (C->GetFName() == Name) return true; return false; }
FGameplayAbilitySpec* Ability(ABwayPlayerState* P, FName Name)
{
	if (ASC(P)) for (FGameplayAbilitySpec& S : ASC(P)->GetActivatableAbilities()) if (S.Ability && Family(S.Ability->GetClass(), Name)) return &S;
	return nullptr;
}
TArray<ABwayPlayerState*> Players(UWorld* W)
{
	TArray<ABwayPlayerState*> Result;
	if (auto* GS = W ? W->GetGameState<ABwayGameState>() : nullptr)
		for (APlayerState* PS : GS->PlayerArray)
			if (auto* P = Cast<ABwayPlayerState>(PS); P && !P->IsABot() && ASC(P)
				&& ASC(P)->GetSet<ULyraHealthSet>() && ASC(P)->GetSet<UBwayHeroAttributeSet>()) Result.Add(P);
	Result.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	return Result;
}
ABwayPlayerState* Find(UWorld* W, int32 Id) { for (auto* P : Players(W)) if (P->GetPlayerId() == Id) return P; return nullptr; }
int32 ObservedCombatTags(ULyraAbilitySystemComponent* A)
{
	return (A->HasMatchingGameplayTag(TAG_Gameplay_DamageImmunity) ? Immunity : 0)
		| (A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Ethereal) ? Ethereal : 0)
		| (A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed) ? Slowed : 0)
		| (A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_DamageAmplified) ? Amplified : 0)
		| (A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Silenced) ? Silenced : 0)
		| (A->HasMatchingGameplayTag(BwayGameplayTags::State_Status_ArmorShred) ? Shredded : 0);
}
FBwayAllKitCombatRow Row(ABwayPlayerState* P)
{
	FBwayAllKitCombatRow R; auto* A = ASC(P); R.Player = P->GetPlayerId();
	R.Health = A->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
	R.Maximum = A->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute());
	R.Attack = A->GetNumericAttribute(UBwayHeroAttributeSet::GetAttackStrengthAttribute());
	R.Armor = A->GetNumericAttribute(UBwayHeroAttributeSet::GetArmorAttribute());
	R.Move = A->GetNumericAttribute(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute());
	R.Incoming = A->GetNumericAttribute(UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute());
	R.Leech = A->GetNumericAttribute(UBwayHeroAttributeSet::GetLifeLeechAttribute());
	R.Tags = ObservedCombatTags(A); R.Effects = A->GetActiveEffects(FGameplayEffectQuery()).Num();
	R.Kills = P->GetKills(); R.Deaths = P->GetDeaths(); R.Assists = P->GetAssists(); return R;
}
TArray<FBwayAllKitCombatRow> Rows(UWorld* W) { TArray<FBwayAllKitCombatRow> R; for (auto* P : Players(W)) R.Add(Row(P)); return R; }
bool Equal(const FBwayAllKitCombatRow& A, const FBwayAllKitCombatRow& B, bool bEffects)
{
	return A.Player == B.Player && FMath::IsNearlyEqual(A.Health, B.Health, AllKitObservedRowTolerance)
		&& FMath::IsNearlyEqual(A.Maximum, B.Maximum, .01f) && FMath::IsNearlyEqual(A.Attack, B.Attack, .01f)
		&& FMath::IsNearlyEqual(A.Armor, B.Armor, .01f) && FMath::IsNearlyEqual(A.Move, B.Move, .01f)
		&& FMath::IsNearlyEqual(A.Incoming, B.Incoming, .01f) && FMath::IsNearlyEqual(A.Leech, B.Leech, .001f)
		&& A.Tags == B.Tags && (!bEffects || A.Effects == B.Effects)
		&& A.Kills == B.Kills && A.Deaths == B.Deaths && A.Assists == B.Assists;
}
bool EqualRows(const TArray<FBwayAllKitCombatRow>& A, const TArray<FBwayAllKitCombatRow>& B, bool bEffects = false)
{
	if (A.Num() != 4 || B.Num() != 4) return false;
	for (int32 I = 0; I < 4; ++I) if (!Equal(A[I], B[I], bEffects)) return false;
	return true;
}
void PrintRows(const TCHAR* Phase, int32 Case, int32 Control, int32 Observer, const TArray<FBwayAllKitCombatRow>& R)
{
	for (const auto& V : R) UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: Row phase=%s case=%d control=%d observer=%d player=%d health=%.4f maximum=%.4f attack=%.4f armor=%.4f move=%.4f incoming=%.4f leech=%.4f tags=%d effects=%d kills=%d deaths=%d assists=%d"),
		Phase, Case, Control, Observer, V.Player, V.Health, V.Maximum, V.Attack, V.Armor, V.Move, V.Incoming, V.Leech, V.Tags, V.Effects, V.Kills, V.Deaths, V.Assists);
}
bool FixtureActor(AActor* A)
{
	return A && (A->IsA<ABwayHeroProjectileBase>() || A->GetClass()->GetFName() == TEXT("BwayAlonaBlessingZone") || A->GetClass()->GetFName() == TEXT("BwayKorrynSpiteZone"));
}
FString ObjectIdentity(const UObject* Object)
{
	FObjectKey Key(Object);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	Writer << Key;
	return BytesToHex(Bytes.GetData(), Bytes.Num());
}
TArray<FGameplayAttribute> Attributes()
{
	return {ULyraHealthSet::GetMaxHealthAttribute(), ULyraHealthSet::GetHealthAttribute(), UBwayHeroAttributeSet::GetArmorAttribute(),
		UBwayHeroAttributeSet::GetAttackStrengthAttribute(), UBwayHeroAttributeSet::GetLifeLeechAttribute(),
		UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute(),
		UBwayGoldAttributeSet::GetCurrentGoldAttribute()};
}
const FName StatFields[] = {TEXT("Kills"), TEXT("Deaths"), TEXT("Assists"), TEXT("DamageDealt"), TEXT("HealingDone"), TEXT("ForcedFumbles"), TEXT("Interceptions"), TEXT("DeathsInCurrentRound")};
void Telemetry(UWorld* W, int32 Player)
{
	FString Token; FParse::Value(FCommandLine::Get(), TEXT("AllKitOwnerToken="), Token);
	FIntPoint Size = FIntPoint::ZeroValue; if (auto* V = W->GetGameViewport(); V && V->Viewport) Size = V->Viewport->GetSizeXY();
	const auto* Cap = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
	const UNetDriver* Driver = W->GetNetDriver();
	if (!Driver) { UE_LOG(LogTemp, Error, TEXT("BwayAllKitOwner: FAIL telemetry-net-driver")); return; }
#if DO_ENABLE_NET_TEST
	const auto& N = Driver->PacketSimulationSettings;
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: Ready token=%s player=%d netMode=%d rhi=%s width=%d height=%d maxFPS=%.2f lag=%d lagMin=%d lagMax=%d incomingMin=%d incomingMax=%d loss=%d incomingLoss=%d jitter=%d variance=%d"),
		*Token, Player, int32(W->GetNetMode()), GDynamicRHI ? GDynamicRHI->GetName() : TEXT("None"), Size.X, Size.Y, Cap ? Cap->GetFloat() : -1,
		N.PktLag, N.PktLagMin, N.PktLagMax, N.PktIncomingLagMin, N.PktIncomingLagMax, N.PktLoss, N.PktIncomingLoss, N.PktJitter, N.PktLagVariance);
#else
	UE_LOG(LogTemp, Error, TEXT("BwayAllKitOwner: FAIL packet-simulation-unavailable token=%s"), *Token);
#endif
}
}
using namespace BwayAllKitOwner;

struct FBwayAllKitCombatState
{
	struct FCredit { TWeakObjectPtr<ABwayPlayerState> Player; double Time; int32 Team; };
	struct FSaved
	{
		TWeakObjectPtr<ABwayPlayerState> Player; TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
		FTransform Transform; FRotator Rotation; FVector Velocity, Pending; EMovementMode Mode; uint8 Custom;
		float Fly = 0; ECollisionResponse Collision = ECR_Block; TArray<float> Bases; TArray<int32> Stats;
		TArray<FActiveGameplayEffectHandle> Effects; TArray<FCredit> Credit;
		double GoldEarned = 0;
		TWeakObjectPtr<AActor> LegacyDamageInstigator;
		TWeakObjectPtr<ABwayPlayerState> LastDamage; int32 Round = INDEX_NONE, Team = INDEX_NONE, Consumed = INDEX_NONE;
		bool bLethal = false, bDeathConsumed = false;
	};
	TArray<FSaved> Saved;
	TSet<TWeakObjectPtr<AActor>> ExistingActors;
	TMap<TWeakObjectPtr<ABwayBaseZone>, float> Bases;
	TArray<TWeakObjectPtr<UBrainComponent>> Brains;
	TSet<int32> Acks;
	TArray<FBwayAllKitCombatRow> Expected;
	TArray<FBwayAllKitCombatRow> Baseline;
	TMap<int32, int32> Seen;
	TMap<int32, float> PeakSpeed;
	double Started = FPlatformTime::Seconds(), Changed = 0, ActivatedAt = 0;
	int32 Completed = 0, Controls = 0, Ally = INDEX_NONE, Round = INDEX_NONE;
	float Damage = 0, AllyHeal = 0, SourceHeal = 0, MinSourceSpeed = 0, MinTargetSpeed = 0;
	int32 SourceTags = 0; bool bSetup = false, bActivated = false, bFailed = false, bRestored = false, bPassed = false;
	bool bLethalPending = false; int32 LethalTarget = INDEX_NONE, LethalAssister = INDEX_NONE;
	TWeakObjectPtr<ABwayCharacterWithAbilities> DeadPawn;
	FObjectKey DeadPawnKey;
	FString DeadPawnIdentity, DeadPawnPath;
	TWeakObjectPtr<ULyraHealthComponent> DeathHealth;
	int32 DeathSerial = INDEX_NONE, DeathCallbacks = 0, DeathAssisters = INDEX_NONE, DeathContributors = INDEX_NONE;
	float DeathHealthValue = 1.f;
	bool bDeathObserverBound = false, bDeathObserved = false, bDeathIdentityMatches = false, bDeathStarted = false, bDeathConsumed = false;
	bool bSawFlockFlight = false;
};
struct FBwayAllKitCombatWitnessState
{
	int32 Serial = -1, ActivatedSerial = -1, AckSerial = -1, AckStage = -1, Seen = 0;
	int32 DoubleDownDiagnosticSamples = 0;
	float SourceSpeed = 0, TargetSpeed = 0; double StableAt = 0;
	FRotator OriginalAim; bool bSavedAim = false, bRestoredAim = false;
	bool bIgnoreMove = false, bIgnoreLook = false;
	TMap<int32, int32> InitialEffects;
	TSet<FName> InitialActiveAbilities;
	TMap<TWeakObjectPtr<ULyraAbilitySystemComponent>, FIntPoint> LooseTags;
	TMap<TWeakObjectPtr<ABwayCharacterWithAbilities>, float> FlySpeeds;
	float OriginalFly = 0;
	bool bSawPass = false;
};

ABwayAllKitCombatProbe::ABwayAllKitCombatProbe()
{
	bReplicates = true; bAlwaysRelevant = true; bNetLoadOnClient = false; SetReplicateMovement(true); SetNetUpdateFrequency(30.f);
	auto* Floor = CreateDefaultSubobject<UBoxComponent>(TEXT("FixtureFloor")); SetRootComponent(Floor);
	Floor->SetBoxExtent(FVector(6000, 6000, 50)); Floor->SetCollisionProfileName(TEXT("BlockAll"));
	PrimaryActorTick.bCanEverTick = !UE_BUILD_SHIPPING; PrimaryActorTick.TickInterval = .02f;
}
void ABwayAllKitCombatProbe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ABwayAllKitCombatProbe, Frame); }
void ABwayAllKitCombatProbe::BeginPlay() { Super::BeginPlay(); State = MakeShared<FBwayAllKitCombatState>(); }
void ABwayAllKitCombatProbe::SetStage(int32 Stage)
{
	Frame.Stage = Stage; State->Changed = FPlatformTime::Seconds(); State->Acks.Reset(); ForceNetUpdate();
}
void ABwayAllKitCombatProbe::Fail(const TCHAR* Reason)
{
	if (!State || State->bFailed) return;
	State->bFailed = true;
	UE_LOG(LogTemp, Error, TEXT("BwayAllKitOwner: FAIL case=%d control=%d stage=%d serial=%d reason=%s"), Frame.Case, Frame.Control, Frame.Stage, Frame.Serial, Reason);
	const bool bRestored = Restore(); Frame.Rows = Rows(GetWorld()); SetStage(9);
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: FailureCleanup proved=%d"), bRestored);
}

void ABwayAllKitCombatProbe::SnapshotCombat()
{
#if !UE_BUILD_SHIPPING
	for (auto& S : State->Saved) if (auto* P = S.Pawn.Get())
	{
		S.Credit.Reset(); for (const auto& Pair : P->CombatContributors) S.Credit.Add({Pair.Key, Pair.Value.LastDamageTime, Pair.Value.Team});
		S.LastDamage = P->LastDamagePlayerState; S.Round = P->CombatRound; S.Team = P->CombatVictimTeam;
		S.LegacyDamageInstigator = P->LastDamageInstigator;
		S.bLethal = P->bLastDamageWasLethal; S.bDeathConsumed = P->bCombatDeathConsumed; S.Consumed = P->ConsumedAssisterCountForAudit;
	}
#endif
}
void ABwayAllKitCombatProbe::RestoreCombat()
{
#if !UE_BUILD_SHIPPING
	for (auto& S : State->Saved) if (auto* P = Pawn(S.Player.Get()))
	{
		P->CombatContributors.Reset();
		for (const auto& V : S.Credit) P->CombatContributors.FindOrAdd(V.Player) = {V.Time, V.Team};
		P->LastDamagePlayerState = S.LastDamage; P->CombatRound = S.Round; P->CombatVictimTeam = S.Team;
		P->LastDamageInstigator = S.LegacyDamageInstigator;
		P->bLastDamageWasLethal = S.bLethal; P->bCombatDeathConsumed = S.bDeathConsumed; P->ConsumedAssisterCountForAudit = S.Consumed;
	}
#endif
}

bool ABwayAllKitCombatProbe::Setup()
{
	auto All = Players(GetWorld()); auto* GS = GetWorld()->GetGameState<ABwayGameState>(); auto* Round = GS ? GS->GetRoundManagement() : nullptr;
	if (All.Num() != 4 || !Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || Round->GetCurrentRoundState() != ERoundState::RoundActive) return false;
	for (auto* P : All)
	{
		if (!Pawn(P) || !P->GetPlayerController() || Row(P).Health <= 0 || ObservedCombatTags(ASC(P)) || !P->UpgradeComponent || !P->UpgradeComponent->GetOwnedUpgrades().IsEmpty()) return false;
		for (const auto& C : Cases) if (const auto* Spec = Ability(P, C.Ability); Spec && Spec->IsActive()) return false;
#if !UE_BUILD_SHIPPING
		// A clean disposable match prevents restoring another player's live combat history onto a new life.
		if (!Pawn(P)->CombatContributors.IsEmpty()) { Fail(TEXT("requires-clean-combat-baseline")); return false; }
#endif
	}
	State->Round = Round->GetCurrentRoundNumber();
	for (auto* P : All)
	{
		auto& S = State->Saved.AddDefaulted_GetRef(); S.Player = P; S.Pawn = Pawn(P); auto* M = Pawn(P)->GetCharacterMovement();
		S.Transform = Pawn(P)->GetActorTransform(); S.Rotation = P->GetPlayerController()->GetControlRotation();
		Frame.SavedAims.Add(S.Rotation);
		S.Velocity = M->Velocity; S.Pending = M->PendingLaunchVelocity; S.Mode = M->MovementMode; S.Custom = M->CustomMovementMode;
		S.Fly = M->MaxFlySpeed; S.Collision = Pawn(P)->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);
		for (const auto& A : Attributes()) S.Bases.Add(ASC(P)->GetNumericAttributeBase(A));
		for (const FName Name : StatFields) { const auto* Prop = FindFProperty<FIntProperty>(P->GetClass(), Name); S.Stats.Add(Prop ? Prop->GetPropertyValue_InContainer(P) : 0); }
		S.Effects = ASC(P)->GetActiveEffects(FGameplayEffectQuery());
		if (const auto* Prop = FindFProperty<FDoubleProperty>(P->GetClass(), TEXT("MatchGoldEarned"))) S.GoldEarned = Prop->GetPropertyValue_InContainer(P);
		FActorSpawnParameters Params; Params.Owner = P->GetPlayerController();
		auto* W = GetWorld()->SpawnActor<ABwayAllKitCombatWitness>(Params); if (!W) { Fail(TEXT("witness-spawn")); return false; }
		W->Probe = this; W->Player = P->GetPlayerId(); W->ForceNetUpdate(); Witnesses.Add(W);
	}
	SnapshotCombat();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (FixtureActor(*It)) State->ExistingActors.Add(*It);
	for (TActorIterator<ABwayBaseZone> It(GetWorld()); It; ++It) { State->Bases.Add(*It, It->HealingPerSecond); It->HealingPerSecond = 0; }
	for (TActorIterator<AAIController> It(GetWorld()); It; ++It) if (It->BrainComponent && It->BrainComponent->IsRunning()) { State->Brains.Add(It->BrainComponent); It->BrainComponent->StopLogic(TEXT("AllKitOwnerFixture")); }
	State->bSetup = true; UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: Setup players=4 actions=20 controls=20 creditCases=2 round=%d"), State->Round);
	return Prepare();
}

bool ABwayAllKitCombatProbe::CleanCase()
{
	if (!State || !HasAuthority()) return false;
	RemoveCreditDeathObserver();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (FixtureActor(*It) && !State->ExistingActors.Contains(*It)) It->Destroy();
	for (const auto& S : State->Saved) if (auto* P = S.Player.Get(); P && ASC(P))
	{
		if (auto* Spec = Ability(P, Frame.Action); Spec && Spec->IsActive()) ASC(P)->CancelAbilityHandle(Spec->Handle);
		for (const auto H : ASC(P)->GetActiveEffects(FGameplayEffectQuery())) if (!S.Effects.Contains(H)) ASC(P)->RemoveActiveGameplayEffect(H);
		ASC(P)->SetLooseGameplayTagCount(BwayGameplayTags::State_Status_Silenced, 0);
		ASC(P)->SetLooseGameplayTagCount(TAG_Gameplay_DamageImmunity, 0);
		if (auto* C = Pawn(P)) { C->GetCharacterMovement()->StopMovementImmediately(); C->GetCharacterMovement()->PendingLaunchVelocity = FVector::ZeroVector; }
	}
	return true;
}

bool ABwayAllKitCombatProbe::Prepare()
{
	const bool bAssist = Frame.Case == 20, bKill = Frame.Case == 21;
	CleanCase(); if (!bKill) RestoreCombat();
	auto All = Players(GetWorld()); auto* GS = GetWorld()->GetGameState<ABwayGameState>();
	if (All.Num() != 4 || !GS) { Fail(TEXT("roster-lost")); return false; }
	ABwayPlayerState* Source = nullptr;
	FName FamilyName = Frame.Case < 20 ? FName(Cases[Frame.Case].Ability) : FName(TEXT("BwayGameplayAbility_RawlinsSlideShot"));
	for (auto* P : All) if (Ability(P, FamilyName)) { Source = P; break; }
	if (!Source) { Fail(TEXT("required-production-grant-missing")); return false; }
	ABwayPlayerState* Target = nullptr; ABwayPlayerState* Ally = nullptr;
	for (auto* P : All) if (P != Source) { if (GS->GetPlayerTeam(P) == GS->GetPlayerTeam(Source)) Ally = P; else if (!Target) Target = P; }
	if (!Target || !Ally) { Fail(TEXT("requires-two-teams-of-two")); return false; }
	if (bAssist)
	{
		State->LethalTarget = Target->GetPlayerId(); State->LethalAssister = Ally->GetPlayerId(); Source = Ally;
		for (int32 I : {0, 5, 10, 15}) if (Ability(Source, Cases[I].Ability)) { FamilyName = Cases[I].Ability; break; }
	}
	if (bKill) Target = Find(GetWorld(), State->LethalTarget);
	if (!Target || !Pawn(Target)) { Fail(TEXT("credit-target-missing")); return false; }
	Frame.Source = Source->GetPlayerId(); Frame.Target = Target->GetPlayerId(); Frame.Action = FamilyName;
	State->Ally = Ally->GetPlayerId(); State->Damage = State->AllyHeal = State->SourceHeal = 0;
	State->MinSourceSpeed = State->MinTargetSpeed = 0; State->SourceTags = 0;
	State->bActivated = false; State->ActivatedAt = 0; State->Seen.Reset(); State->PeakSpeed.Reset(); State->bSawFlockFlight = false;
	Frame.Duration = Frame.Case < 20 ? Cases[Frame.Case].Duration : .9f;
	const bool bPrimary = FamilyName.ToString().Contains(TEXT("Primary"));
	Frame.bExpectActivation = Frame.Control == 0 || bPrimary;
	const auto* SourcePawn = Pawn(Source);
	if (!SourcePawn) { Fail(TEXT("pawn-missing")); return false; }
	const auto* FixtureFloor = CastChecked<UBoxComponent>(GetRootComponent());
	Frame.SourcePosition = GetActorLocation() + FVector(0, 0, FixtureFloor->GetScaledBoxExtent().Z
		+ SourcePawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
	for (auto* P : All)
	{
		auto* A = ASC(P); auto* C = Pawn(P); if (!C) { Fail(TEXT("pawn-missing")); return false; }
		if (!bKill)
		{
			A->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), 1000);
			A->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), (bAssist && P == Target) ? 60.f : 500.f);
			A->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), 0);
			A->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), 1);
			A->SetNumericAttributeBase(UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute(), 1);
			A->SetNumericAttributeBase(UBwayHeroAttributeSet::GetLifeLeechAttribute(), .1f);
			const auto* Saved = State->Saved.FindByPredicate([P](const auto& S) { return S.Player == P; });
			A->SetNumericAttributeBase(UBwayHeroAttributeSet::GetAttackStrengthAttribute(), Saved->Bases[3]);
		}
		FVector Offset = P == Source ? FVector::ZeroVector : (P == Target ? FVector(160, 0, 0) : FVector(0, 2000 + 500 * All.IndexOfByKey(P), 0));
		if (P == Ally && Frame.Case == 7) Offset = FVector(160, 0, 0);
		if (P == Ally && Frame.Case == 9) Offset = FVector(400, 0, 0);
		// Keep the damage target out of non-damaging directional movement/teleport corridors.
		if (P == Target && (Frame.Case == 1 || Frame.Case == 6 || Frame.Case == 16)) Offset = FVector(1500, 0, 0);
		C->SetActorLocationAndRotation(Frame.SourcePosition + Offset, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		C->GetCharacterMovement()->StopMovementImmediately(); C->GetCharacterMovement()->PendingLaunchVelocity = FVector::ZeroVector;
		C->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		P->GetPlayerController()->SetControlRotation(FRotator::ZeroRotator); P->GetPlayerController()->ClientSetRotation(FRotator::ZeroRotator, true); C->ForceNetUpdate();
	}
	if (Frame.Case == 7) ASC(Source)->SetNumericAttributeBase(UBwayHeroAttributeSet::GetAttackStrengthAttribute(), 78.f);
	if (Frame.Case == 11) Pawn(Source)->GetCharacterMovement()->MaxFlySpeed = 345.f;
	if (Frame.Case == 7 || Frame.Case == 9) Pawn(Target)->SetActorLocation(Frame.SourcePosition + FVector(160, 300, 0));
	if (Frame.Control)
	{
		if (bPrimary) ASC(Target)->AddLooseGameplayTag(TAG_Gameplay_DamageImmunity);
		else ASC(Source)->AddLooseGameplayTag(BwayGameplayTags::State_Status_Silenced);
		Frame.Duration = bPrimary ? .7f : .25f;
	}
	FGameplayAbilitySpec* Spec = Ability(Source, FamilyName);
	UGameplayAbility* Instance = Spec ? (Spec->GetPrimaryInstance() ? Spec->GetPrimaryInstance() : Spec->Ability.Get()) : nullptr;
	const auto* ConfigProp = Instance ? FindFProperty<FSoftObjectProperty>(Instance->GetClass(), TEXT("KitConfig")) : nullptr;
	UObject* Config = ConfigProp ? ConfigProp->GetPropertyValue_InContainer(Instance).LoadSynchronous() : nullptr;
	bool bConfigOk = Config != nullptr;
	auto F = [&](const TCHAR* Name) { const auto* P = Config ? FindFProperty<FFloatProperty>(Config->GetClass(), Name) : nullptr; if (!P) bConfigOk = false; return P ? P->GetPropertyValue_InContainer(Config) : 0.f; };
	if (Frame.Case == 6 && !Frame.Control)
	{
		const auto* Capsule = Pawn(Source)->GetCapsuleComponent();
		const float Distance = F(TEXT("SunsGraceTeleportDistance"));
		FCollisionQueryParams Query(SCENE_QUERY_STAT(AllKitTeleportCorridor), false, Pawn(Source));
		FHitResult Hit;
		const bool bBlocked = GetWorld()->SweepSingleByChannel(Hit, Frame.SourcePosition,
			Frame.SourcePosition + FVector(Distance, 0.f, 0.f), FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: TeleportCorridor case=6 clear=%d distance=%.3f blocker=%s"), !bBlocked, Distance, *GetPathNameSafe(Hit.GetActor()));
		if (bBlocked || Distance < 400.f) { Fail(TEXT("suns-grace-unobstructed-corridor-precondition")); return false; }
	}
	auto N = [&](const TCHAR* Name) { const auto* P = Config ? FindFProperty<FIntProperty>(Config->GetClass(), Name) : nullptr; if (!P) bConfigOk = false; return P ? P->GetPropertyValue_InContainer(Config) : 0; };
	const float Strength = ASC(Source)->GetNumericAttribute(UBwayHeroAttributeSet::GetAttackStrengthAttribute());
	int32 Kind = Frame.Case;
	if (bAssist) { for (int32 I : {0, 5, 10, 15}) if (FamilyName == Cases[I].Ability) Kind = I; }
	if (bKill) Kind = 18;
	if (!Frame.Control)
	{
		switch (Kind)
		{
		case 0: case 5: case 10: case 15: State->Damage = F(TEXT("PrimaryBaseDamage")) + F(TEXT("PrimaryDamageScaling")) * Strength; if (Kind == 15) State->Damage *= N(TEXT("PrimaryShotCount")); break;
		case 1: State->SourceTags = Immunity; State->MinSourceSpeed = F(TEXT("SlideDashDistance")) / F(TEXT("SlideDashDuration")) * .5f; break;
		case 2: State->Damage = F(TEXT("NoRetreatBaseDamage")) + F(TEXT("NoRetreatDamageScaling")) * Strength; State->MinTargetSpeed = 100; break;
		case 3: State->Damage = F(TEXT("ForGloryBaseDamage")) + F(TEXT("ForGloryDamageScaling")) * Strength; State->MinTargetSpeed = 100; break;
		case 4: State->Damage = F(TEXT("RetributionUppercutBaseDamage")) + F(TEXT("RetributionDownSmashBaseDamage")) + 2 * F(TEXT("RetributionDamageScaling")) * Strength; State->MinTargetSpeed = 100; break;
		case 6: State->SourceTags = Immunity; break;
		case 7: State->AllyHeal = 2 * F(TEXT("RadianceHealPerTick")) * FMath::Min(Strength / F(TEXT("RadianceStrengthDivisor")), F(TEXT("RadianceMaxHealMultiplier"))); break;
		case 8: State->Damage = F(TEXT("SunBurstBaseDamage")) + F(TEXT("SunBurstDamageScaling")) * Strength; State->MinTargetSpeed = 100; break;
		case 9: State->AllyHeal = State->SourceHeal = F(TEXT("BlessingInitialHeal")) + F(TEXT("BlessingHealPerSecond")); break;
		case 11: State->SourceTags = Immunity | Ethereal; break;
		case 12: State->Damage = F(TEXT("BurdenBaseDamage")) + F(TEXT("BurdenDamageScaling")) * Strength; break;
		case 13: break;
		case 14: State->Damage = F(TEXT("AuraBaseDamage")) + F(TEXT("AuraDamageScaling")) * Strength; break;
		case 16: State->SourceTags = Immunity; State->MinSourceSpeed = F(TEXT("DoubleDownDashDistance")) / F(TEXT("DoubleDownDashDuration")) * .5f; break;
		case 17: State->Damage = F(TEXT("PowerShotBaseDamage")) + F(TEXT("PowerShotDamageScaling")) * Strength; State->MinTargetSpeed = 100; break;
		case 18: State->Damage = F(TEXT("SlideShotBaseDamage")) + F(TEXT("SlideShotDamageScaling")) * Strength; if (!bKill) State->MinTargetSpeed = 100; break;
		case 19: State->Damage = (F(TEXT("BarrageBaseDamagePerBullet")) + F(TEXT("BarrageDamageScaling")) * Strength) * N(TEXT("BarrageShotCount")); break;
		}
	}
	// Raw Gameplay.DamageImmunity suppresses health damage, not this projectile's
	// independent armor-shred effect. Actual movement invulnerability is a separate gate.
	const float PlannedPrimaryArmorShred = Kind == 10 ? F(TEXT("ArmorShredPerHit")) : 0.f;
	if (!bConfigOk || !FMath::IsFinite(State->Damage + State->AllyHeal + State->SourceHeal + PlannedPrimaryArmorShred)) { Fail(TEXT("missing-or-invalid-production-config")); return false; }
	State->Baseline = Rows(GetWorld()); State->Expected = State->Baseline;
	State->Damage = FMath::Min(State->Damage, Row(Target).Health);
	for (auto& R : State->Expected)
	{
		if (R.Player == Frame.Source) R.Health += State->SourceHeal + .1f * State->Damage;
		if (R.Player == State->Ally) R.Health += State->AllyHeal;
		if (R.Player == Frame.Target)
		{
			R.Health -= State->Damage;
			if (Kind == 10) { R.Armor += PlannedPrimaryArmorShred; R.Tags |= Shredded; }
			if (!Frame.Control)
			{
				if (Kind == 12) { R.Move = F(TEXT("BurdenSlowMultiplier")); R.Tags |= Slowed; }
				if (Kind == 13) { R.Move = F(TEXT("CircleSlowMultiplier")); R.Incoming = F(TEXT("CircleIncomingDamageMultiplier")); R.Tags |= Slowed | Amplified; }
				if (Kind == 14) R.Tags |= Silenced;
			}
		}
		if (bKill) { if (R.Player == Frame.Source) ++R.Kills; if (R.Player == Frame.Target) ++R.Deaths; if (R.Player == State->LethalAssister) ++R.Assists; }
	}
	if (!bConfigOk) { Fail(TEXT("missing-status-config")); return false; }
	Frame.Rows = State->Baseline; Frame.Positions.Reset(); for (auto* P : All) Frame.Positions.Add(Pawn(P)->GetActorLocation());
	++Frame.Serial;
	if (bKill)
	{
		// Bind while the exact victim is alive, after the character's constructor
		// binding. This engine's ScriptDelegates.h appends and invokes forward, so
		// production RoundManagement consumes credit before our snapshot callback.
		auto* Victim = Pawn(Target);
		auto* Health = ULyraHealthComponent::FindHealthComponent(Victim);
		if (!Victim || !Health || Health->IsDeadOrDying()
			|| !Health->OnDeathStarted.Contains(Victim, FName(TEXT("OnDeathStarted"))))
		{ Fail(TEXT("credit-victim-death-binding-precondition")); return false; }
		State->DeadPawn = Victim; State->DeadPawnKey = FObjectKey(Victim);
		State->DeadPawnIdentity = ObjectIdentity(Victim); State->DeadPawnPath = Victim->GetPathName();
		State->DeathHealth = Health; State->DeathSerial = Frame.Serial;
		State->DeathCallbacks = 0; State->DeathAssisters = State->DeathContributors = INDEX_NONE;
		State->DeathHealthValue = Health->GetHealth();
		State->bDeathObserved = State->bDeathIdentityMatches = State->bDeathStarted = State->bDeathConsumed = false;
		Health->OnDeathStarted.AddUniqueDynamic(this, &ThisClass::OnCreditVictimDeathStarted);
		State->bDeathObserverBound = true;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: CreditBinding case=21 serial=%d victim=%d pawnKey=%s pawn=%s callbackBound=1 existingDeathBinding=1"),
			State->DeathSerial, State->LethalTarget, *State->DeadPawnIdentity, *State->DeadPawnPath);
	}
	SetStage(1); PrintRows(TEXT("Baseline"), Frame.Case, Frame.Control, -1, State->Baseline);
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: Plan case=%d control=%d serial=%d source=%d target=%d ally=%d action=%s config=%s strength=%.4f damage=%.4f leech=%.4f allyHeal=%.4f sourceHeal=%.4f armorShred=%.4f activation=%d"), Frame.Case, Frame.Control, Frame.Serial, Frame.Source, Frame.Target, State->Ally, *Frame.Action.ToString(), *GetPathNameSafe(Config), Strength, State->Damage, .1f * State->Damage, State->AllyHeal, State->SourceHeal, PlannedPrimaryArmorShred, Frame.bExpectActivation);
	return true;
}

bool ABwayAllKitCombatProbe::ValidateOutcome()
{
	auto* Source = Find(GetWorld(), Frame.Source); auto* Target = Find(GetWorld(), Frame.Target);
	if (!Source || !Target) { Fail(TEXT("outcome-roster")); return false; }
	const auto Observed = Rows(GetWorld());
	// The independently computed plan is the oracle. Observed health never supplies expected damage/healing.
	if (!EqualRows(Observed, State->Expected))
	{
		PrintRows(TEXT("Expected"), Frame.Case, Frame.Control, -1, State->Expected);
		PrintRows(TEXT("ActualFailure"), Frame.Case, Frame.Control, -1, Observed);
		Fail(TEXT("authoritative-outcome-mismatch")); return false;
	}
	if ((State->Seen.FindRef(Frame.Source) & State->SourceTags) != State->SourceTags
		|| State->PeakSpeed.FindRef(Frame.Source) < State->MinSourceSpeed
		|| State->PeakSpeed.FindRef(Frame.Target) < State->MinTargetSpeed)
	{ Fail(TEXT("missing-authoritative-movement-or-immunity")); return false; }
	if (!Frame.Control && Frame.Case == 6 && (!Pawn(Source) || Pawn(Source)->GetActorLocation().X - Frame.SourcePosition.X < 400.f))
	{ Fail(TEXT("suns-grace-did-not-teleport")); return false; }
	if (!Frame.Control && Frame.Case == 11 && (!State->bSawFlockFlight || !Pawn(Source)
		|| Pawn(Source)->GetCharacterMovement()->MovementMode == MOVE_Flying
		|| !FMath::IsNearlyEqual(Pawn(Source)->GetCharacterMovement()->MaxFlySpeed, 345.f)))
	{ Fail(TEXT("flock-flight-or-restoration")); return false; }
#if !UE_BUILD_SHIPPING
	if (State->Damage > 0 && Frame.Case != 21)
	{
		auto* Victim = Pawn(Target);
		if (!Victim || Victim->LastDamagePlayerState != Source || !Victim->CombatContributors.Contains(Source)
			|| Victim->bLastDamageWasLethal || Victim->bCombatDeathConsumed)
		{ Fail(TEXT("damage-missing-production-credit-attribution")); return false; }
		const auto& Hit = Victim->CombatContributors.FindChecked(Source);
		if (Hit.Team != GetWorld()->GetGameState<ABwayGameState>()->GetPlayerTeam(Source)
			|| Hit.LastDamageTime <= 0 || GetWorld()->GetTimeSeconds() - Hit.LastDamageTime > 4.f)
		{ Fail(TEXT("stale-or-wrong-team-credit")); return false; }
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: Attribution case=%d source=%d target=%d contributors=%d sourceTeam=%d lethal=0"), Frame.Case, Frame.Source, Frame.Target, Victim->CombatContributors.Num(), Hit.Team);
	}
	if (Frame.Control && Pawn(Target) && !Pawn(Target)->CombatContributors.IsEmpty())
	{ Fail(TEXT("negative-control-created-credit")); return false; }
#endif
	if (Frame.Case == 21)
	{
		if (!State->bDeathObserved || !State->bDeathIdentityMatches || !State->bDeathStarted || !State->bDeathConsumed
			|| State->DeathSerial != Frame.Serial || State->DeathCallbacks != 1 || State->DeathAssisters != 1
			|| State->DeathContributors != 0 || !FMath::IsFinite(State->DeathHealthValue) || State->DeathHealthValue > 0.f)
		{ Fail(TEXT("lethal-credit-not-consumed-on-bound-victim")); return false; }
		RemoveCreditDeathObserver();
		if (State->bDeathObserverBound) { Fail(TEXT("credit-death-observer-removal-failed")); return false; }
		State->bLethalPending = true;
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: CreditPASS killer=%d assister=%d victim=%d killsDelta=1 assistsDelta=1 deathsDelta=1"), Frame.Source, State->LethalAssister, Frame.Target);
	}
	// End multi-tick heal/zone production after the measured window. Preserve status effects for peer observation.
	if (auto* Spec = Ability(Source, Frame.Action); Spec && Spec->IsActive()) ASC(Source)->CancelAbilityHandle(Spec->Handle);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (FixtureActor(*It) && !State->ExistingActors.Contains(*It) && It->GetClass()->GetFName() == TEXT("BwayAlonaBlessingZone")) It->Destroy();
	for (auto* P : Players(GetWorld())) if (auto* C = Pawn(P))
	{
		C->GetCharacterMovement()->StopMovementImmediately(); C->GetCharacterMovement()->PendingLaunchVelocity = FVector::ZeroVector;
		C->GetCharacterMovement()->SetMovementMode(MOVE_Flying); C->ForceNetUpdate();
	}
	Frame.Rows = State->Expected;
	PrintRows(TEXT("Authority"), Frame.Case, Frame.Control, -1, Observed);
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: AuthorityPASS case=%d control=%d sourceTags=%d sourceSpeed=%.3f targetSpeed=%.3f"), Frame.Case, Frame.Control,
		State->Seen.FindRef(Frame.Source), State->PeakSpeed.FindRef(Frame.Source), State->PeakSpeed.FindRef(Frame.Target));
	SetStage(3); return true;
}

void ABwayAllKitCombatProbe::Activated(ABwayAllKitCombatWitness* Witness, int32 Serial, bool bActivated)
{
	if (!HasAuthority() || !State || State->bFailed || !Witnesses.Contains(Witness) || Witness->Player != Frame.Source
		|| Serial != Frame.Serial || Frame.Stage != 2 || State->bActivated) return;
	if (bActivated != Frame.bExpectActivation) { Fail(TEXT("owner-activation-expectation")); return; }
	State->bActivated = true; State->ActivatedAt = FPlatformTime::Seconds();
	if (bActivated) if (auto* S = Find(GetWorld(), Frame.Source))
	{
		State->Seen.FindOrAdd(Frame.Source) |= ObservedCombatTags(ASC(S));
		if (auto* C = Pawn(S))
		{
			auto* M = C->GetCharacterMovement();
			auto& Peak = State->PeakSpeed.FindOrAdd(Frame.Source);
			Peak = FMath::Max(Peak, FMath::Max(M->Velocity.Size(), M->PendingLaunchVelocity.Size()));
		}
	}
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: Activation case=%d control=%d owner=%d activated=%d expected=%d"), Frame.Case, Frame.Control, Witness->Player, bActivated, Frame.bExpectActivation);
}
void ABwayAllKitCombatProbe::OnCreditVictimDeathStarted(AActor* OwningActor)
{
	if (!HasAuthority() || !State || !State->bDeathObserverBound || State->bFailed) return;
	++State->DeathCallbacks;
	auto* Victim = Cast<ABwayCharacterWithAbilities>(OwningActor);
	auto* Health = State->DeathHealth.Get();
	State->bDeathObserved = true;
	State->bDeathIdentityMatches = Victim && Victim == State->DeadPawn.Get() && FObjectKey(Victim) == State->DeadPawnKey
		&& Victim->GetPathName() == State->DeadPawnPath && Health && Health->GetOwner() == Victim
		&& Frame.Case == 21 && Frame.Control == 0 && Frame.Stage == 2 && Frame.Serial == State->DeathSerial;
	State->bDeathStarted = Health && Health->IsDeadOrDying();
	State->DeathHealthValue = Health ? Health->GetHealth() : 1.f;
#if !UE_BUILD_SHIPPING
	State->bDeathConsumed = Victim && Victim->bCombatDeathConsumed;
	State->DeathAssisters = Victim ? Victim->ConsumedAssisterCountForAudit : INDEX_NONE;
	State->DeathContributors = Victim ? Victim->CombatContributors.Num() : INDEX_NONE;
#endif
	// Snapshot only: failure handling/restoration must not mutate the ongoing
	// production death delegate broadcast or keep the old pawn possessed/alive.
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: CreditDeath case=21 serial=%d victim=%d pawnKey=%s pawn=%s boundIdentity=%d deathStarted=%d consumed=%d assisters=%d contributors=%d health=%.4f callbacks=%d"),
		State->DeathSerial, State->LethalTarget, *State->DeadPawnIdentity, *State->DeadPawnPath,
		State->bDeathIdentityMatches, State->bDeathStarted, State->bDeathConsumed, State->DeathAssisters,
		State->DeathContributors, State->DeathHealthValue, State->DeathCallbacks);
}
void ABwayAllKitCombatProbe::RemoveCreditDeathObserver()
{
	if (!State || !State->bDeathObserverBound) return;
	auto* Health = State->DeathHealth.Get();
	if (Health) Health->OnDeathStarted.RemoveDynamic(this, &ThisClass::OnCreditVictimDeathStarted);
	const bool bStillBound = Health && Health->OnDeathStarted.Contains(this, FName(TEXT("OnCreditVictimDeathStarted")));
	State->bDeathObserverBound = bStillBound;
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: CreditObserverRemoved case=21 serial=%d victim=%d pawnKey=%s componentAlive=%d callbackBound=%d"),
		State->DeathSerial, State->LethalTarget, *State->DeadPawnIdentity, Health != nullptr, bStillBound);
	if (!bStillBound) State->DeathHealth.Reset();
}
void ABwayAllKitCombatProbe::Acknowledge(ABwayAllKitCombatWitness* W, int32 Serial, int32 Stage,
	const TArray<FBwayAllKitCombatRow>& Observed, int32 Seen, float SourceSpeed, float TargetSpeed)
{
	if (!HasAuthority() || !State || State->bFailed || !Witnesses.Contains(W) || Serial != Frame.Serial || Stage != Frame.Stage) return;
	const auto* PC = Cast<APlayerController>(W->GetOwner());
	if (!PC || !PC->PlayerState || PC->PlayerState->GetPlayerId() != W->Player || !EqualRows(Observed, Frame.Rows)) { Fail(TEXT("invalid-peer-observation")); return; }
	if (State->Acks.Contains(W->Player)) return;
	if (Stage == 3 && W->Player == Frame.Source
		&& ((Seen & State->SourceTags) != State->SourceTags || SourceSpeed < State->MinSourceSpeed))
	{
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitDiagnostic: OwnerLifecycleMismatch case=%d control=%d serial=%d owner=%d seen=%d requiredTags=%d sourceSpeed=%.6f minSourceSpeed=%.6f targetSpeed=%.6f"),
			Frame.Case, Frame.Control, Frame.Serial, W->Player, Seen, State->SourceTags, SourceSpeed, State->MinSourceSpeed, TargetSpeed);
		Fail(TEXT("owner-did-not-observe-predicted-lifecycle")); return;
	}
	State->Acks.Add(W->Player);
	if (Stage == 3)
	{
		PrintRows(TEXT("Peer"), Frame.Case, Frame.Control, W->Player, Observed);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: PeerPASS case=%d control=%d observer=%d sourceTags=%d sourceSpeed=%.3f targetSpeed=%.3f"), Frame.Case, Frame.Control, W->Player, Seen, SourceSpeed, TargetSpeed);
	}
	else if (Stage == 4 || Stage == 5) PrintRows(Stage == 5 ? TEXT("FinalCleanup") : TEXT("CaseCleanup"), Frame.Case, Frame.Control, W->Player, Observed);
}

bool ABwayAllKitCombatProbe::Restore()
{
	if (!State || !HasAuthority()) return false;
	if (State->bRestored) return true;
	CleanCase(); bool bOk = !State->bDeathObserverBound;
	const auto Attrs = Attributes();
	for (auto& S : State->Saved)
	{
		auto* P = S.Player.Get(); auto* C = Pawn(P);
		if (!P || !C || !ASC(P) || !P->GetPlayerController() || Row(P).Health <= 0) { bOk = false; continue; }
		for (int32 I = 0; I < Attrs.Num(); ++I) ASC(P)->SetNumericAttributeBase(Attrs[I], S.Bases[I]);
		for (int32 I = 0; I < UE_ARRAY_COUNT(StatFields); ++I) if (auto* Prop = FindFProperty<FIntProperty>(P->GetClass(), StatFields[I])) Prop->SetPropertyValue_InContainer(P, S.Stats[I]);
		if (auto* Prop = FindFProperty<FDoubleProperty>(P->GetClass(), TEXT("MatchGoldEarned"))) Prop->SetPropertyValue_InContainer(P, S.GoldEarned);
		auto* M = C->GetCharacterMovement(); M->StopMovementImmediately(); M->PendingLaunchVelocity = S.Pending;
		M->SetMovementMode(S.Mode, S.Custom); M->Velocity = S.Velocity; M->MaxFlySpeed = S.Fly;
		C->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, S.Collision);
		C->SetActorTransform(S.Transform, false, nullptr, ETeleportType::TeleportPhysics);
		P->GetPlayerController()->SetControlRotation(S.Rotation); P->GetPlayerController()->ClientSetRotation(S.Rotation, true);
		C->ForceNetUpdate(); P->ForceNetUpdate();
		for (int32 I = 0; I < Attrs.Num(); ++I) bOk &= FMath::IsNearlyEqual(ASC(P)->GetNumericAttributeBase(Attrs[I]), S.Bases[I], .001f);
		bOk &= ASC(P)->GetActiveEffects(FGameplayEffectQuery()).Num() == S.Effects.Num();
		for (int32 I = 0; I < UE_ARRAY_COUNT(StatFields); ++I) { const auto* Prop = FindFProperty<FIntProperty>(P->GetClass(), StatFields[I]); bOk &= Prop && Prop->GetPropertyValue_InContainer(P) == S.Stats[I]; }
		const auto* Gold = FindFProperty<FDoubleProperty>(P->GetClass(), TEXT("MatchGoldEarned")); bOk &= Gold && Gold->GetPropertyValue_InContainer(P) == S.GoldEarned;
		bOk &= C->GetActorTransform().Equals(S.Transform, .01f) && M->Velocity.Equals(S.Velocity, .01f)
			&& M->PendingLaunchVelocity.Equals(S.Pending, .01f) && M->MovementMode == S.Mode && M->CustomMovementMode == S.Custom
			&& FMath::IsNearlyEqual(M->MaxFlySpeed, S.Fly) && C->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == S.Collision
			&& P->GetPlayerController()->GetControlRotation().Equals(S.Rotation, .01f);
	}
	RestoreCombat();
#if !UE_BUILD_SHIPPING
	for (const auto& S : State->Saved) if (auto* C = Pawn(S.Player.Get()))
	{
		bOk &= C->CombatContributors.Num() == S.Credit.Num() && C->LastDamagePlayerState == S.LastDamage
			&& C->LastDamageInstigator == S.LegacyDamageInstigator && C->CombatRound == S.Round && C->CombatVictimTeam == S.Team
			&& C->bLastDamageWasLethal == S.bLethal && C->bCombatDeathConsumed == S.bDeathConsumed && C->ConsumedAssisterCountForAudit == S.Consumed;
		for (const auto& Credit : S.Credit) { const auto* Actual = C->CombatContributors.Find(Credit.Player); bOk &= Actual && Actual->LastDamageTime == Credit.Time && Actual->Team == Credit.Team; }
	}
#endif
	for (const auto& Pair : State->Bases) if (auto* B = Pair.Key.Get()) B->HealingPerSecond = Pair.Value;
	for (const auto& B : State->Brains) if (B.IsValid()) B->RestartLogic();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (FixtureActor(*It) && !It->IsActorBeingDestroyed() && !State->ExistingActors.Contains(*It)) bOk = false;
	State->bRestored = bOk;
	UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: CleanupAuthority proved=%d players=%d"), bOk, State->Saved.Num());
	return bOk;
}

void ABwayAllKitCombatProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	if (!HasAuthority() || !State || State->bFailed) return;
	const double Now = FPlatformTime::Seconds();
	if (State->bPassed) { if (Now - State->Changed >= 8) Destroy(); return; }
	if (Now - State->Started > 420) { Fail(TEXT("overall-timeout")); return; }
	if (!State->bSetup) { Setup(); return; }
	if (Frame.Stage != 6 && Now - State->Changed > 12) { Fail(TEXT("stage-timeout")); return; }
	auto* Round = GetWorld()->GetGameState<ABwayGameState>()->GetRoundManagement();
	if (Round->GetCurrentRoundNumber() != State->Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) { Fail(TEXT("round-changed-during-fixture")); return; }
	if (Frame.Stage == 1 && State->Acks.Num() == 4) { SetStage(2); return; }
	if (Frame.Stage == 2)
	{
		// Synthetic aiming follows the live juggle target; hit count/damage still come only from production projectiles.
		if (Frame.Case == 19 && !Frame.Control) if (auto* S = Find(GetWorld(), Frame.Source); S && Pawn(S))
			if (auto* T = Find(GetWorld(), Frame.Target); T && Pawn(T)) S->GetPlayerController()->SetControlRotation((Pawn(T)->GetActorLocation() - Pawn(S)->GetActorLocation()).Rotation());
		for (auto* P : Players(GetWorld()))
		{
			State->Seen.FindOrAdd(P->GetPlayerId()) |= ObservedCombatTags(ASC(P));
			if (auto* C = Pawn(P))
			{
				auto* M = C->GetCharacterMovement(); auto& Peak = State->PeakSpeed.FindOrAdd(P->GetPlayerId()); Peak = FMath::Max(Peak, FMath::Max(M->Velocity.Size(), M->PendingLaunchVelocity.Size()));
				if (P->GetPlayerId() == Frame.Source && Frame.Case == 11 && M->MovementMode == MOVE_Flying
					&& C->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore) State->bSawFlockFlight = true;
			}
		}
		if (State->bActivated && Now - State->ActivatedAt >= Frame.Duration) ValidateOutcome();
		return;
	}
	if (Frame.Stage == 3 && State->Acks.Num() == 4)
	{
		if (Frame.Case == 20) { UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: CaseComplete case=20 control=0 observers=4 cleanup=deferred-credit-chain")); Frame.Case = 21; Prepare(); return; }
		if (Frame.Case == 21) { CleanCase(); SetStage(6); return; }
		CleanCase(); RestoreCombat();
		for (auto* P : Players(GetWorld()))
		{
			ASC(P)->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 500);
			ASC(P)->SetNumericAttributeBase(UBwayHeroAttributeSet::GetArmorAttribute(), 0);
			ASC(P)->SetNumericAttributeBase(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), 1);
			ASC(P)->SetNumericAttributeBase(UBwayHeroAttributeSet::GetIncomingDamageMultiplierAttribute(), 1);
		}
		Frame.Rows = Rows(GetWorld()); SetStage(4); return;
	}
	if (Frame.Stage == 4 && State->Acks.Num() == 4)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: CaseComplete case=%d control=%d observers=4 cleanup=1"), Frame.Case, Frame.Control);
		if (Frame.Control) { ++State->Controls; Frame.Control = 0; ++Frame.Case; } else { ++State->Completed; Frame.Control = 1; }
		Prepare(); return;
	}
	if (Frame.Stage == 6)
	{
		auto* Target = Find(GetWorld(), State->LethalTarget);
		if (Now - State->Changed > 40) { Fail(TEXT("production-respawn-timeout")); return; }
		auto* Replacement = Pawn(Target);
		if (!Target || !Replacement || FObjectKey(Replacement) == State->DeadPawnKey || Row(Target).Health <= 0 || ObservedCombatTags(ASC(Target))) return;
		if (State->bDeathObserverBound || !State->bDeathObserved) { Fail(TEXT("respawn-before-death-observer-cleanup")); return; }
		if (auto* SavedTarget = State->Saved.FindByPredicate([Target](const auto& S) { return S.Player == Target; }))
		{
			if (ASC(Target)->GetActiveEffects(FGameplayEffectQuery()).Num() != SavedTarget->Effects.Num()) return;
			SavedTarget->Pawn = Replacement;
			SavedTarget->Effects = ASC(Target)->GetActiveEffects(FGameplayEffectQuery());
		}
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: CreditRespawn case=21 serial=%d victim=%d oldPawnKey=%s oldPawn=%s newPawnKey=%s newPawn=%s identityChanged=1 health=%.4f callbackBound=0"),
			State->DeathSerial, State->LethalTarget, *State->DeadPawnIdentity, *State->DeadPawnPath,
			*ObjectIdentity(Replacement), *Replacement->GetPathName(), Row(Target).Health);
		if (!Restore()) { Fail(TEXT("final-restoration-failed")); return; }
		Frame.Rows = Rows(GetWorld()); SetStage(5); return;
	}
	if (Frame.Stage == 5 && State->Acks.Num() == 4)
	{
		State->bPassed = true;
		SetStage(7);
		UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: PASS actions=%d controls=%d creditCases=2 observers=4 cleanup=1"), State->Completed, State->Controls);
	}
#endif
}
void ABwayAllKitCombatProbe::EndPlay(const EEndPlayReason::Type Reason)
{
	RemoveCreditDeathObserver();
	if (HasAuthority()) { Restore(); for (ABwayAllKitCombatWitness* W : Witnesses) if (IsValid(W)) W->Destroy(); }
	Super::EndPlay(Reason);
}

ABwayAllKitCombatWitness::ABwayAllKitCombatWitness()
{
	bReplicates = true; bAlwaysRelevant = true; bNetLoadOnClient = false; SetNetUpdateFrequency(30.f);
	PrimaryActorTick.bCanEverTick = !UE_BUILD_SHIPPING; PrimaryActorTick.TickInterval = .02f;
}
void ABwayAllKitCombatWitness::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ABwayAllKitCombatWitness, Probe); DOREPLIFETIME(ABwayAllKitCombatWitness, Player); }
void ABwayAllKitCombatWitness::BeginPlay() { Super::BeginPlay(); State = MakeShared<FBwayAllKitCombatWitnessState>(); }
void ABwayAllKitCombatWitness::ServerObserved_Implementation(int32 Serial, int32 Stage, const TArray<FBwayAllKitCombatRow>& R, int32 Seen, float SourceSpeed, float TargetSpeed)
{ if (Probe) Probe->Acknowledge(this, Serial, Stage, R, Seen, SourceSpeed, TargetSpeed); }
void ABwayAllKitCombatWitness::ServerActivated_Implementation(int32 Serial, bool bActivated) { if (Probe) Probe->Activated(this, Serial, bActivated); }
bool ABwayAllKitCombatWitness::RestoreLocal(bool bFinal)
{
	if (!State || !State->bSavedAim) return true;
	bool bOk = true;
	for (const auto& Pair : State->LooseTags) if (auto* A = Pair.Key.Get())
	{
		A->SetLooseGameplayTagCount(BwayGameplayTags::State_Status_Silenced, Pair.Value.X);
		A->SetLooseGameplayTagCount(TAG_Gameplay_DamageImmunity, Pair.Value.Y);
		bOk &= A->GetTagCount(BwayGameplayTags::State_Status_Silenced) == Pair.Value.X && A->GetTagCount(TAG_Gameplay_DamageImmunity) == Pair.Value.Y;
	}
	if (bFinal && !State->bRestoredAim)
	{
		for (const auto& Pair : State->FlySpeeds) if (auto* C = Pair.Key.Get()) { C->GetCharacterMovement()->MaxFlySpeed = Pair.Value; bOk &= FMath::IsNearlyEqual(C->GetCharacterMovement()->MaxFlySpeed, Pair.Value); }
		if (auto* PC = Cast<APlayerController>(GetOwner()); PC && PC->IsLocalController())
		{
			PC->SetControlRotation(State->OriginalAim); PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false);
			bOk &= PC->IsMoveInputIgnored() == State->bIgnoreMove && PC->IsLookInputIgnored() == State->bIgnoreLook;
		}
		State->bRestoredAim = true;
	}
	return bOk;
}
void ABwayAllKitCombatWitness::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!State || !Probe || !PC || !PC->IsLocalController() || !PC->PlayerState || PC->PlayerState->GetPlayerId() != Player) return;
	const auto& F = Probe->GetFrame(); if (F.Stage == 0) return;
	if (F.Stage == 9)
	{
		RestoreLocal(true);
		return;
	}
	if (!State->bSavedAim)
	{
		State->OriginalAim = PC->GetControlRotation(); State->bSavedAim = true;
		for (int32 I = 0; I < F.Rows.Num(); ++I) if (F.Rows[I].Player == Player && F.SavedAims.IsValidIndex(I)) State->OriginalAim = F.SavedAims[I];
		State->bIgnoreMove = PC->IsMoveInputIgnored(); State->bIgnoreLook = PC->IsLookInputIgnored();
		PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
		for (const auto& R : Rows(GetWorld())) State->InitialEffects.Add(R.Player, R.Effects);
		for (auto* P : Players(GetWorld())) State->LooseTags.Add(ASC(P), FIntPoint(ASC(P)->GetTagCount(BwayGameplayTags::State_Status_Silenced), ASC(P)->GetTagCount(TAG_Gameplay_DamageImmunity)));
		if (auto* LocalPawn = Pawn(Find(GetWorld(), Player))) State->OriginalFly = LocalPawn->GetCharacterMovement()->MaxFlySpeed;
		if (auto* Local = Find(GetWorld(), Player)) for (const auto& Spec : ASC(Local)->GetActivatableAbilities())
			if (Spec.IsActive() && Spec.Ability) State->InitialActiveAbilities.Add(Spec.Ability->GetClass()->GetFName());
		Telemetry(GetWorld(), Player);
	}
	if (State->Serial != F.Serial) { State->Serial = F.Serial; State->Seen = 0; State->SourceSpeed = State->TargetSpeed = 0; State->StableAt = 0; State->DoubleDownDiagnosticSamples = 0; }
	auto* Source = Find(GetWorld(), F.Source); auto* Target = Find(GetWorld(), F.Target);
	if (!Source || !Target) return;
	if (F.Control && F.Stage <= 3)
	{
		// These are explicit fixture preconditions, independently installed in each world, not replication assertions.
		if (F.bExpectActivation) ASC(Target)->SetLooseGameplayTagCount(TAG_Gameplay_DamageImmunity, 1);
		else ASC(Source)->SetLooseGameplayTagCount(BwayGameplayTags::State_Status_Silenced, 1);
	}
	if (F.Stage >= 4)
	{
		if (!RestoreLocal(F.Stage == 5 || F.Stage == 7)) return;
	}
	if (F.Stage == 2)
	{
		if (F.Case == 19 && !F.Control && F.Source == Player && Pawn(Source) && Pawn(Target))
			PC->SetControlRotation((Pawn(Target)->GetActorLocation() - Pawn(Source)->GetActorLocation()).Rotation());
		State->Seen |= ObservedCombatTags(ASC(Source));
		if (auto* C = Pawn(Source)) State->SourceSpeed = FMath::Max(State->SourceSpeed, FMath::Max(C->GetCharacterMovement()->Velocity.Size(), C->GetCharacterMovement()->PendingLaunchVelocity.Size()));
		if (auto* C = Pawn(Target)) State->TargetSpeed = FMath::Max(State->TargetSpeed, FMath::Max(C->GetCharacterMovement()->Velocity.Size(), C->GetCharacterMovement()->PendingLaunchVelocity.Size()));
		// Diagnostics do not modify the sampled state or the lifecycle gate.
		auto LogDoubleDownObservation = [&](const TCHAR* Moment)
		{
			if (F.Case != 16 || F.Control || F.Source != Player) return;
			auto* C = Pawn(Source); auto* T = Pawn(Target); if (!C || !T) return;
			auto* M = C->GetCharacterMovement(); const auto* S = Ability(Source, F.Action);
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitDiagnostic: DoubleDownOwner moment=%s case=%d control=%d serial=%d owner=%d netMode=%d world=%s pawn=%s target=%s worldSeconds=%.6f dt=%.6f sample=%d position=%s targetPosition=%s velocity=%s pending=%s mode=%d movementActive=%d abilityActive=%d tags=%d seen=%d peak=%.6f radius=%.6f targetRadius=%.6f pawnResponse=%d targetPawnResponse=%d"),
				Moment, F.Case, F.Control, F.Serial, Player, int32(GetNetMode()), *GetPathNameSafe(GetWorld()), *GetPathNameSafe(C), *GetPathNameSafe(T),
				GetWorld()->GetTimeSeconds(), DeltaSeconds, State->DoubleDownDiagnosticSamples, *C->GetActorLocation().ToCompactString(), *T->GetActorLocation().ToCompactString(),
				*M->Velocity.ToCompactString(), *M->PendingLaunchVelocity.ToCompactString(), int32(M->MovementMode), M->IsActive(), S && S->IsActive(),
				ObservedCombatTags(ASC(Source)), State->Seen, State->SourceSpeed, C->GetCapsuleComponent()->GetScaledCapsuleRadius(), T->GetCapsuleComponent()->GetScaledCapsuleRadius(),
				int32(C->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn)), int32(T->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn)));
		};
		if (F.Case == 16 && !F.Control && F.Source == Player && State->ActivatedSerial == F.Serial && State->DoubleDownDiagnosticSamples < 3)
		{
			++State->DoubleDownDiagnosticSamples;
			LogDoubleDownObservation(TEXT("subsequent-tick"));
		}
		if (F.Source == Player && State->ActivatedSerial != F.Serial)
		{
			auto* Spec = Ability(Source, F.Action); if (!Spec || !Pawn(Source)) return;
			PC->SetControlRotation(FRotator::ZeroRotator);
			if (F.Case == 11) { State->FlySpeeds.FindOrAdd(Pawn(Source), State->OriginalFly); Pawn(Source)->GetCharacterMovement()->MaxFlySpeed = 345; }
			State->ActivatedSerial = F.Serial;
			const bool bActivated = ASC(Source)->TryActivateAbility(Spec->Handle);
			State->Seen |= ObservedCombatTags(ASC(Source));
			// LaunchCharacter writes pending velocity synchronously; movement may
			// consume it before our next tick. Sample the same measured quantities
			// immediately, without deriving any observed speed from expected tuning.
			if (bActivated) if (auto* C = Pawn(Source))
				State->SourceSpeed = FMath::Max(State->SourceSpeed, FMath::Max(C->GetCharacterMovement()->Velocity.Size(), C->GetCharacterMovement()->PendingLaunchVelocity.Size()));
			LogDoubleDownObservation(TEXT("after-activation"));
			UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: OwnerInput case=%d control=%d owner=%d netMode=%d action=%s activated=%d"), F.Case, F.Control, Player, int32(GetNetMode()), *F.Action.ToString(), bActivated);
			ServerActivated(F.Serial, bActivated);
		}
		return;
	}
	if (F.Stage == 7) { State->bSawPass = true; return; }
	if (F.Stage == 6 || (State->AckSerial == F.Serial && State->AckStage == F.Stage)) return;
	const auto Observed = Rows(GetWorld());
	if (!EqualRows(Observed, F.Rows)) { State->StableAt = 0; return; }
	if (F.Stage == 1)
	{
		const auto All = Players(GetWorld()); if (All.Num() != F.Positions.Num()) return;
		for (int32 I = 0; I < All.Num(); ++I) if (!Pawn(All[I]) || FVector::Dist(Pawn(All[I])->GetActorLocation(), F.Positions[I]) > 30) return;
		if (F.Source == Player && Pawn(Source)->GetCharacterMovement()->MovementMode != MOVE_Walking) return;
	}
	if (F.Stage == 3 && F.Source == Player) if (auto* Spec = Ability(Source, F.Action); Spec && Spec->IsActive()) return;
	if (F.Stage == 4 || F.Stage == 5)
	{
		for (auto* P : Players(GetWorld()))
		{
			if (P->GetPlayerId() == Player && ASC(P)->GetActiveEffects(FGameplayEffectQuery()).Num() != State->InitialEffects.FindRef(Player)) return;
			if (P->GetPlayerId() == Player) for (const auto& Spec : ASC(P)->GetActivatableAbilities())
				if (Spec.IsActive() && Spec.Ability && !State->InitialActiveAbilities.Contains(Spec.Ability->GetClass()->GetFName())) return;
		}
	}
	if (State->StableAt == 0) State->StableAt = FPlatformTime::Seconds();
	if (FPlatformTime::Seconds() - State->StableAt < ((F.Stage == 4 || F.Stage == 5) ? .6 : .1)) return;
	State->AckSerial = F.Serial; State->AckStage = F.Stage; State->StableAt = 0;
	if (F.Stage == 1) PrintRows(TEXT("LocalBaseline"), F.Case, F.Control, Player, Observed);
	if (F.Stage == 3) PrintRows(TEXT("LocalPeer"), F.Case, F.Control, Player, Observed);
	if (F.Stage == 5) PrintRows(TEXT("LocalFinalCleanup"), F.Case, F.Control, Player, Observed);
	ServerObserved(F.Serial, F.Stage, Observed, State->Seen, State->SourceSpeed, State->TargetSpeed);
	if (F.Stage == 5 || F.Stage == 9)
	{
		if (F.Stage == 5) UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: PeerFinalPASS observer=%d cleanup=1"), Player);
	}
#endif
}
void ABwayAllKitCombatWitness::EndPlay(const EEndPlayReason::Type Reason)
{
	RestoreLocal(true);
	if (State && State->AckStage == 5 && State->bSawPass && !HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("AllKitOwnerAutoExit"))) FPlatformMisc::RequestExit(false);
	Super::EndPlay(Reason);
}

#if !UE_BUILD_SHIPPING
namespace BwayAllKitOwner
{
void Start(bool bClient)
{
	const double Began = FPlatformTime::Seconds(); const auto Selected = MakeShared<bool>(false);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([bClient, Began, Selected](float)
	{
		if (FPlatformTime::Seconds() - Began > 100) { UE_LOG(LogTemp, Error, TEXT("BwayAllKitOwner: FAIL startup-timeout")); return false; }
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* W = Context.World(); if (!W || !W->IsGameWorld() || (W->GetNetMode() == NM_Client) != bClient) continue;
			auto* GS = W->GetGameState<ABwayGameState>(); if (!GS) continue;
			if (bClient) { if (!W->GetFirstPlayerController() || !W->GetFirstPlayerController()->PlayerState) continue; UE_LOG(LogTemp, Display, TEXT("BwayAllKitOwner: StartupConnected role=client")); return false; }
			if (W->GetMapName().Contains(TEXT("HeroSelect_Staging")))
			{
				if (*Selected || !GS->HeroSelectionManager || !GS->HeroSelectionManager->IsSelectionActive()) continue;
				TArray<ABwayPlayerState*> Roster; for (APlayerState* P : GS->PlayerArray) if (auto* B = Cast<ABwayPlayerState>(P); B && !B->IsABot()) Roster.Add(B);
				if (Roster.Num() != 4) continue;
				Roster.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
				const TCHAR* Names[] = {TEXT("Argus"), TEXT("Rawlins"), TEXT("Alona"), TEXT("Korryn")};
				if (FParse::Param(FCommandLine::Get(), TEXT("AllKitRemoteArgus"))) Swap(Names[0], Names[1]);
				*Selected = true;
				for (int32 I = 0; I < 4; ++I) if (!GS->HeroSelectionManager->AssignHeroToPlayer(Roster[I], UBwayHeroRegistry::ResolveHeroIdByName(Names[I]), true)) { UE_LOG(LogTemp, Error, TEXT("BwayAllKitOwner: FAIL hero-selection")); return false; }
			}
			else if (W->GetMapName().Contains(TEXT("L_BW_Dorado")) && Players(W).Num() == 4)
			{
				for (TActorIterator<ABwayAllKitCombatProbe> It(W); It; ++It) { UE_LOG(LogTemp, Error, TEXT("BwayAllKitOwner: FAIL existing-probe")); return false; }
				W->SpawnActor<ABwayAllKitCombatProbe>(ABwayAllKitCombatProbe::StaticClass(), FVector(0, 0, 5000), FRotator::ZeroRotator); return false;
			}
		}
		return true;
	}), .1f);
}
FAutoConsoleCommand Host(TEXT("bway.Test.AllKitOwnerHost"), TEXT("Development20-action owning-client combat, controls, credit and cleanup fixture."), FConsoleCommandDelegate::CreateLambda([] { Start(false); }));
FAutoConsoleCommand Client(TEXT("bway.Test.AllKitOwnerClient"), TEXT("Development readiness for replicated per-player AllKit witnesses."), FConsoleCommandDelegate::CreateLambda([] { Start(true); }));
}
#endif
