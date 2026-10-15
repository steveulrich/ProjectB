#include "Development/BwayBarrageTransportProbe.h"

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBarrageTransportProbe)

bool FBwayBarrageTransportFrame::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	Ar << Serial << Stage << Witness << Source << Round << Lag << Count << Interval << Life << Cooldown << Speed << Offset << FrameQuantum << KitPath << Players << Teams << Health << Maximum;
	bOutSuccess = !Ar.IsError();
	return true;
}

namespace BwayBarrageTransport
{
enum EStage { Waiting = 0, Ready = 1, CastOne = 2, Cooling = 3, CastTwo = 4, Cleanup = 5, Complete = 6, Failed = 7 };
const TCHAR* FamilyPath = TEXT("/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage");
const TCHAR* BulletPath = TEXT("/Script/HeroRawlinsRuntime.BwayRawlinsBulletProjectile");
const TCHAR* MontagePath = TEXT("/BreakawayCore/Animations/AM_BW_Rawlins_Barrage_Placeholder.AM_BW_Rawlins_Barrage_Placeholder");
const TCHAR* Heroes[] = { TEXT("Argus"), TEXT("Rawlins"), TEXT("Alona"), TEXT("Korryn") };
FGameplayTag CooldownTag() { return FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Rawlins.BlazingBarrage"), false); }
struct FPlayer
{
	TWeakObjectPtr<ABwayPlayerState> State;
	TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;
	TWeakObjectPtr<APlayerController> Controller;
	FDelegateHandle HealthDelegate;
	int32 Id = INDEX_NONE, Team = INDEX_NONE;
	float Health = 0, Maximum = 0;
};
float Health(const FPlayer& P) { return P.ASC.IsValid() ? P.ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1.f; }
bool Capture(ABwayGameState* GS, ABwayPlayerState* PS, FPlayer& Out)
{
	auto* Pawn = PS ? Cast<ABwayCharacterWithAbilities>(PS->GetPawn()) : nullptr;
	auto* ASC = PS ? PS->GetLyraAbilitySystemComponent() : nullptr;
	const auto* H = Pawn ? ULyraHealthComponent::FindHealthComponent(Pawn) : nullptr;
	if (!GS || !PS || PS->IsABot() || !Pawn || !ASC || !H || H->IsDeadOrDying() || ASC->GetOwnerActor() != PS
		|| ASC->GetAvatarActor() != Pawn || Pawn->GetLyraAbilitySystemComponent() != ASC || !ASC->GetSet<ULyraHealthSet>()
		|| GS->GetPlayerTeam(PS) == INDEX_NONE) return false;
	Out.State = PS; Out.Pawn = Pawn; Out.ASC = ASC; Out.Id = PS->GetPlayerId(); Out.Team = GS->GetPlayerTeam(PS);
	Out.Controller = Cast<APlayerController>(Pawn->GetController());
	Out.Health = Health(Out); Out.Maximum = ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute());
	return FMath::IsFinite(Out.Health) && Out.Health > 0 && FMath::IsNearlyEqual(Out.Health, Out.Maximum, .01f);
}
bool PlayerLive(const FPlayer& P, ABwayGameState* GS)
{
	const auto* H = P.Pawn.IsValid() ? ULyraHealthComponent::FindHealthComponent(P.Pawn.Get()) : nullptr;
	return GS && P.State.IsValid() && P.Pawn.IsValid() && P.ASC.IsValid() && H && !H->IsDeadOrDying()
		&& P.State->GetPlayerId() == P.Id && GS->PlayerArray.Contains(P.State.Get()) && GS->GetPlayerTeam(P.State.Get()) == P.Team
		&& P.State->GetPawn() == P.Pawn.Get() && P.Pawn->GetPlayerState<ABwayPlayerState>() == P.State.Get()
		&& P.State->GetLyraAbilitySystemComponent() == P.ASC.Get() && P.Pawn->GetLyraAbilitySystemComponent() == P.ASC.Get()
		&& P.ASC->GetOwnerActor() == P.State.Get() && P.ASC->GetAvatarActor() == P.Pawn.Get()
		&& (P.Controller.IsExplicitlyNull() || (P.Controller.IsValid() && P.Pawn->GetController() == P.Controller.Get()
			&& P.Controller->GetPawn() == P.Pawn.Get() && P.Controller->PlayerState == P.State.Get()))
		&& FMath::IsNearlyEqual(Health(P), P.Health, .01f)
		&& FMath::IsNearlyEqual(P.ASC->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute()), P.Maximum, .01f);
}
FGameplayAbilitySpec* UniqueSpec(ULyraAbilitySystemComponent* ASC)
{
	UClass* Family = FindObject<UClass>(nullptr, FamilyPath);
	FGameplayAbilitySpec* Result = nullptr;
	if (!ASC || !Family) return nullptr;
	for (auto& S : ASC->GetActivatableAbilities()) if (S.Ability && S.Ability->IsA(Family))
	{
		if (Result) return nullptr;
		Result = &S;
	}
	return Result;
}
bool Float(const UObject* O, const TCHAR* Name, float& V)
{
	const auto* F = O ? FindFProperty<FFloatProperty>(O->GetClass(), Name) : nullptr;
	if (!F) return false;
	V = F->GetPropertyValue_InContainer(O);
	return FMath::IsFinite(V);
}
const UGameplayAbility* AbilityObject(const FGameplayAbilitySpec* Spec)
{
	return Spec ? (Spec->GetPrimaryInstance() ? Spec->GetPrimaryInstance() : Spec->Ability.Get()) : nullptr;
}
bool ConfiguredKit(const UGameplayAbility* A, FString& Path, const UObject*& Config)
{
	const auto* P = A ? FindFProperty<FSoftObjectProperty>(A->GetClass(), TEXT("KitConfig")) : nullptr;
	if (!P) return false;
	const FSoftObjectPtr Reference = P->GetPropertyValue_InContainer(A);
	Path = Reference.ToSoftObjectPath().ToString();
	Config = Reference.Get(); // Observe production residency; never force a load.
	return !Reference.IsNull() && Reference.ToSoftObjectPath().IsValid();
}
bool Tuning(const UGameplayAbility* A, FBwayBarrageTransportFrame& Frame, const UObject*& Config)
{
	if (!ConfiguredKit(A, Frame.KitPath, Config) || !Config) return false;
	const auto* Count = Config ? FindFProperty<FIntProperty>(Config->GetClass(), TEXT("BarrageShotCount")) : nullptr;
	if (!Count || !Float(Config, TEXT("BarrageShotInterval"), Frame.Interval) || !Float(Config, TEXT("BarrageProjectileLifeSpan"), Frame.Life)
		|| !Float(Config, TEXT("BarrageCooldown"), Frame.Cooldown) || !Float(Config, TEXT("BarrageProjectileSpeed"), Frame.Speed)
		|| !Float(Config, TEXT("BarrageSpawnForwardOffset"), Frame.Offset)) return false;
	Frame.Count = Count->GetPropertyValue_InContainer(Config);
	return true;
}
UClass* ResidentCueClass(UGameplayCueManager* Manager)
{
	if (const UGameplayCueSet* Set = Manager ? Manager->GetRuntimeCueSet() : nullptr)
		for (const auto& Entry : Set->GameplayCueData) if (Entry.GameplayCueTag == BwayGameplayCueTags::RawlinsBarrageFire)
		{
			if (Manager->IsAsyncLoadingGameplayCueNotifyClass(Entry.GameplayCueNotifyObj)) return nullptr;
			return Entry.LoadedGameplayCueClass ? Entry.LoadedGameplayCueClass.Get() : Cast<UClass>(Entry.GameplayCueNotifyObj.ResolveObject());
		}
	return nullptr;
}
bool CueReady(UGameplayCueManager* Manager) { return ResidentCueClass(Manager) != nullptr; }
uint64 Guid(const AActor* Actor)
{
	const UNetDriver* D = Actor && Actor->GetWorld() ? Actor->GetWorld()->GetNetDriver() : nullptr;
	return D && D->GuidCache ? D->GuidCache->GetNetGUID(Actor).ObjectId : 0;
}
int32 OwnedBullets(UWorld* World, const APawn* Pawn)
{
	UClass* Family = FindObject<UClass>(nullptr, BulletPath);
	if (!World || !Pawn || !Family) return INDEX_NONE;
	int32 Count = 0;
	for (TActorIterator<AActor> It(World, Family); It; ++It) if (It->GetOwner() == Pawn) ++Count;
	return Count;
}
bool SlotsSettled(const UAnimInstance* Anim)
{
	if (!Anim) return false;
	const float Upper = Anim->GetSlotMontageGlobalWeight(TEXT("UpperBody"));
	const float Full = Anim->GetSlotMontageGlobalWeight(TEXT("FullBody"));
	return FMath::IsFinite(Upper) && FMath::IsFinite(Full) && FMath::Abs(Upper) <= .01f && FMath::Abs(Full) <= .01f;
}
}
using namespace BwayBarrageTransport;

struct FBwayBarrageTransportState
{
	TArray<FPlayer> Roster;
	int32 SourceIndex = INDEX_NONE;
	TWeakObjectPtr<APlayerController> LocalController;
	TWeakObjectPtr<ABwayPlayerState> LocalState;
	TWeakObjectPtr<UBwayRoundManagementComponent> Round;
	TWeakObjectPtr<UClass> AbilityClass;
	TWeakObjectPtr<ULyraGameplayAbility> Instance;
	TWeakObjectPtr<UGameplayCueManager> CueManager;
	TWeakObjectPtr<UAnimMontage> Montage;
	FGameplayAbilitySpecHandle Handle;
	FPredictionKey Key;
	FDelegateHandle ActivationDelegate, EndDelegate, CueDelegate, SpawnDelegate, RemovalDelegate, OwnerEffectRemovalDelegate;
	int32 OwnerEffectRemovals = 0, OwnerReplicatedEffectRemovals = 0;
	FActiveGameplayEffectHandle FirstEffect, SecondEffect;
	TArray<TWeakObjectPtr<ABwayBarrageTransportProbe>> Witnesses;
	TMap<int32, int32> Acks;
	struct FShot
	{
		TWeakObjectPtr<AActor> Actor;
		FString Path;
		int32 Cycle = 0, Index = 0;
		double At = 0, WorldAt = 0;
		FVector Origin = FVector::ZeroVector, Start = FVector::ZeroVector, Direction = FVector::ZeroVector;
		bool bFinalized = false, bMotionObserved = false;
	};
	TArray<FShot> Shots;
	double Started = FPlatformTime::Seconds(), StageAt = 0, LastCueAt = 0, LastSample = -1, FailAt = 0, LastWait = 0;
	float FirstEffectStart = 0, FirstEffectDuration = 0;
	float LaneRadius = 0, ObservedFramePeak = 0;
	float ObservedIdleFramePeak = 0, LastObservedWorldDelta = -1;
	double LastObservedWorldTime = -1;
	uint64 LastFrameObserved = MAX_uint64;
	int32 FrameSamples = 0, IdleFrameSamples = 0, FlightBoundCycle = 0;
	int32 StateObservations = 0;
	int32 CastSettledSamples = 0;
	double CastSettlementStarted = -1, CastSettlementWorld = -1;
	uint64 CastSettlementFrame = MAX_uint64;
	int32 CleanupSettledSamples = 0;
	double CleanupSettlementStarted = -1, CleanupSettlementWorld = -1;
	uint64 CleanupSettlementFrame = MAX_uint64;
	int32 SeenSerial = 0, ReportedSerial = 0, Activations = 0, Ends = 0, Cues = 0, HealthChanges = 0, Removed = 0, TunedActivation = 0;
	bool bCoordinator = false, bLocalReady = false, bBound = false, bOwner = false, bEndpoint = false;
	bool bEarlyRejected = false, bNaturalExpiry = false, bSawCooldown = false, bCleaned = false, bComplete = false, bReportedFailure = false;
	bool bFlightBoundArmed = false;
	bool bChangingAim = false, bAimInputQueued = false, bAimInputConsumed = false;
	FVector AimOrigin = FVector::ZeroVector;
	FRotator AimControlBefore = FRotator::ZeroRotator;
	uint64 AimInputCycles = 0;
	FBwayBarrageStartupReady StartupSample, StartupReceived;
	double StartupLastWall = -1, StartupWindowWall = 0, StartupWindowWorld = 0, StartupLastSend = -1, StartupReceivedAt = -1;
	double StartupAuthorityAt = -1, StartupAuthorityWorld = 0;
	uint64 StartupAuthorityFrame = 0;
	float StartupOriginalTickInterval = .05f;
	int32 StartupObservations = 0, StartupHeartbeats = 0, StartupGeneration = 1;
	int32 StartupPendingObservations = 0;
	bool bStartupTickOverride = false, bStartupAdmitted = false, bStartupHandoff = false;
	FString Failure, WaitingFor;
	FPlayer* Source() { return Roster.IsValidIndex(SourceIndex) ? &Roster[SourceIndex] : nullptr; }
	const FPlayer* Source() const { return Roster.IsValidIndex(SourceIndex) ? &Roster[SourceIndex] : nullptr; }
};

namespace BwayBarrageTransport
{
constexpr float ChangingAimYaw = 12.f;
constexpr double ChangingAimStationaryTolerance = 1.; // cm displacement and cm/s horizontal speed.
bool ChangingAimSafe(const FBwayBarrageTransportState& S, const FBwayBarrageTransportFrame& F, double& Clearance)
{
	const auto* Source = S.Source();
	if (!Source || !Source->Pawn.IsValid() || S.AimOrigin.ContainsNaN() || !FMath::IsFinite(S.LaneRadius) || S.LaneRadius <= 0) return false;
	const FVector Position = Source->Pawn->GetActorLocation(), Velocity = Source->Pawn->GetVelocity();
	if (Position.ContainsNaN() || Velocity.ContainsNaN() || (Position - S.AimOrigin).Size2D() > ChangingAimStationaryTolerance
		|| Velocity.Size2D() > ChangingAimStationaryTolerance) return false;
	const double Range = F.Offset + F.Speed * (F.Life + 2 * F.FrameQuantum);
	if (!FMath::IsFinite(Range) || Range <= 0) return false;
	Clearance = TNumericLimits<double>::Max();
	int32 Enemies = 0;
	for (const auto& P : S.Roster) if (P.Team != Source->Team)
	{
		if (!P.Pawn.IsValid() || !P.Pawn->GetCapsuleComponent()) return false;
		const FVector EnemyPosition = P.Pawn->GetActorLocation();
		const float CapsuleBound = P.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (EnemyPosition.ContainsNaN() || !FMath::IsFinite(CapsuleBound) || CapsuleBound <= 0) return false;
		// The full finite reach sphere covers every yaw and intermediate camera
		// rotation. This additional admission never relaxes the original SafeLane.
		Clearance = FMath::Min(Clearance, FVector::Distance(EnemyPosition, Position) - Range - CapsuleBound - S.LaneRadius);
		++Enemies;
	}
	return Enemies == 2 && FMath::IsFinite(Clearance) && Clearance > 0;
}
}

ABwayBarrageTransportProbe::ABwayBarrageTransportProbe()
{
	bReplicates = true; bAlwaysRelevant = true; bNetLoadOnClient = false;
	SetReplicateMovement(false); SetNetUpdateFrequency(20.f);
	PrimaryActorTick.bCanEverTick = !UE_BUILD_SHIPPING; PrimaryActorTick.TickInterval = .05f;
}
void ABwayBarrageTransportProbe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABwayBarrageTransportProbe, Frame);
}
void ABwayBarrageTransportProbe::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	State = MakeShared<FBwayBarrageTransportState>();
	State->bChangingAim = FParse::Param(FCommandLine::Get(), TEXT("BwayBarrageTransportChangingAim"));
#endif
}

bool ABwayBarrageTransportProbe::WaitFor(const TCHAR* Reason)
{
	if (State && State->WaitingFor != Reason)
	{
		State->WaitingFor = Reason;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: SetupWait witness=%d source=%d stage=%d reason=%s elapsed=%.6f"),
			Frame.Witness, Frame.Source, Frame.Stage, Reason, FPlatformTime::Seconds() - State->Started);
	}
	return false;
}

bool ABwayBarrageTransportProbe::SetupAuthority()
{
#if !UE_BUILD_SHIPPING
	auto* GS = GetWorld()->GetGameState<ABwayGameState>();
	auto* Round = GS ? GS->GetRoundManagement() : nullptr;
	if (!GS) return WaitFor(TEXT("authority-game-state-missing"));
	if (GS->PlayerArray.Num() != 4) return WaitFor(*FString::Printf(TEXT("authority-player-count-%d"), GS->PlayerArray.Num()));
	if (!Round) return WaitFor(TEXT("authority-round-component-missing"));
	if (Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return WaitFor(TEXT("authority-match-not-playing"));
	if (Round->GetCurrentRoundState() != ERoundState::RoundActive) return WaitFor(TEXT("authority-round-not-active"));
	TArray<ABwayPlayerState*> Players;
	for (APlayerState* PS : GS->PlayerArray) if (auto* P = Cast<ABwayPlayerState>(PS)) Players.Add(P);
	Players.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	if (Players.Num() != 4) return WaitFor(TEXT("authority-player-state-class-count"));
	TArray<FPlayer> Captured;
	APlayerController* Host = nullptr;
	int32 SourceId = INDEX_NONE;
	for (ABwayPlayerState* PS : Players)
	{
		FPlayer P;
		if (!Capture(GS, PS, P)) return WaitFor(*FString::Printf(TEXT("authority-player-%d-full-health-chain"), PS->GetPlayerId()));
		auto* PC = Cast<APlayerController>(P.Pawn->GetController());
		if (!PC || PC->PlayerState != PS || PC->GetPawn() != P.Pawn.Get()) return WaitFor(*FString::Printf(TEXT("authority-player-%d-controller-chain"), PS->GetPlayerId()));
		if (PC->IsLocalController()) { if (Host) return WaitFor(TEXT("authority-multiple-local-controllers")); Host = PC; }
		if (PS->GetSelectedHeroId() == UBwayHeroRegistry::ResolveHeroIdByName(TEXT("Rawlins")))
		{
			if (SourceId != INDEX_NONE) return WaitFor(TEXT("authority-multiple-rawlins"));
			if (PC->IsLocalController()) return WaitFor(TEXT("authority-rawlins-is-local"));
			SourceId = P.Id;
		}
		Captured.Add(P);
	}
	if (!Host) return WaitFor(TEXT("authority-listen-host-missing"));
	if (SourceId == INDEX_NONE) return WaitFor(TEXT("authority-remote-rawlins-missing"));
	const int32 SourceIndex = Captured.IndexOfByPredicate([SourceId](const FPlayer& P) { return P.Id == SourceId; });
	const FPlayer& Source = Captured[SourceIndex];
	const auto* Spec = UniqueSpec(Source.ASC.Get());
	const UObject* Config = nullptr;
	const auto* Stats = Source.ASC->GetSet<UBwayHeroAttributeSet>();
	if (!Spec) return WaitFor(TEXT("authority-unique-barrage-spec-missing"));
	if (!ConfiguredKit(AbilityObject(Spec), Frame.KitPath, Config)) return WaitFor(TEXT("authority-configured-kit-reference-invalid"));
	if (!Stats) return WaitFor(TEXT("authority-hero-attributes-missing"));
	if (!FMath::IsNearlyZero(Stats->GetCooldownReduction(), .001f)) return WaitFor(*FString::Printf(TEXT("authority-cooldown-reduction-%.6f"), Stats->GetCooldownReduction()));
	// These are acceptance expectations, not a claim that the cold kit is loaded.
	// Validate the actual instance's resident kit only after production resolves it.
	Frame.Count = 12; Frame.Interval = .1f; Frame.Life = 1.5f; Frame.Cooldown = 25.f;
	Frame.Speed = 3600.f; Frame.Offset = 80.f; Frame.FrameQuantum = .2f;
	const UNetDriver* Driver = GetWorld()->GetNetDriver();
	if (!Driver) return WaitFor(TEXT("authority-net-driver-missing"));
	Frame.Source = SourceId; Frame.Round = Round->GetCurrentRoundNumber(); Frame.Lag = Driver->PacketSimulationSettings.PktLag;
	for (const auto& P : Captured) { Frame.Players.Add(P.Id); Frame.Teams.Add(P.Team); Frame.Health.Add(P.Health); Frame.Maximum.Add(P.Maximum); }
	State->Roster = Captured; State->SourceIndex = SourceIndex;
	State->bCoordinator = true; State->StageAt = FPlatformTime::Seconds(); State->LocalController = Host; State->LocalState = Cast<ABwayPlayerState>(Host->PlayerState); SetOwner(Host);
	Coordinator = this; Frame.Witness = Host->PlayerState->GetPlayerId(); State->Witnesses.Add(this);
	for (const auto& P : Captured)
	{
		auto* PC = Cast<APlayerController>(P.Pawn->GetController());
		if (PC == Host) continue;
		FActorSpawnParameters Params; Params.Owner = PC;
		auto* Witness = GetWorld()->SpawnActor<ABwayBarrageTransportProbe>(StaticClass(), FTransform::Identity, Params);
		if (!Witness) { Fail(TEXT("witness-spawn-failed")); return false; }
		Witness->Coordinator = this; Witness->Frame = Frame; Witness->Frame.Witness = P.Id;
		Witness->State->LocalController = PC; Witness->State->LocalState = P.State;
		State->Witnesses.Add(Witness);
	}
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Plan source=%d config=%s kitPath=%s residentBefore=%d tuningSource=expected-contract tuningVerified=0 count=%d interval=%.6f life=%.6f cooldown=%.6f speed=%.6f offset=%.6f verticalOffset=0 frameQuantum=%.6f timerQuanta=2 range=%.6f cooldownReduction=%.6f witnesses=%d timeout=90 guard=%d"),
		Frame.Source, *GetPathNameSafe(Config), *Frame.KitPath, Config != nullptr, Frame.Count, Frame.Interval, Frame.Life, Frame.Cooldown, Frame.Speed, Frame.Offset, Frame.FrameQuantum, Frame.Offset + Frame.Speed * (Frame.Life + 2 * Frame.FrameQuantum), Stats->GetCooldownReduction(), State->Witnesses.Num(), GAllowActorScriptExecutionInEditor);
	Publish(Ready);
	return true;
#else
	return false;
#endif
}

bool ABwayBarrageTransportProbe::SetupLocal()
{
#if !UE_BUILD_SHIPPING
	auto* PC = Cast<APlayerController>(GetOwner());
	auto* GS = GetWorld()->GetGameState<ABwayGameState>();
	auto* Round = GS ? GS->GetRoundManagement() : nullptr;
	if (!PC || !PC->IsLocalController() || !PC->PlayerState || PC->PlayerState->GetPlayerId() != Frame.Witness) return WaitFor(TEXT("local-witness-owner-chain"));
	if (!GS || GS->PlayerArray.Num() != 4) return WaitFor(TEXT("local-four-player-game-state"));
	if (Frame.Players.Num() != 4 || Frame.Teams.Num() != 4 || Frame.Health.Num() != 4 || Frame.Maximum.Num() != 4 || Frame.KitPath.IsEmpty()) return WaitFor(TEXT("local-replicated-frame-incomplete"));
	if (!Round) return WaitFor(TEXT("local-round-component-missing"));
	if (Round->GetCurrentRoundNumber() != Frame.Round) return WaitFor(TEXT("local-round-number-not-converged"));
	if (Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return WaitFor(TEXT("local-match-not-playing"));
	if (Round->GetCurrentRoundState() != ERoundState::RoundActive) return WaitFor(TEXT("local-round-not-active"));
	if (GAllowActorScriptExecutionInEditor) return WaitFor(TEXT("local-script-guard-active"));
	TArray<FPlayer> Captured;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		ABwayPlayerState* Found = nullptr;
		for (APlayerState* P : GS->PlayerArray) if (P && P->GetPlayerId() == Frame.Players[Index]) { if (Found) return WaitFor(TEXT("local-duplicate-player-id")); Found = Cast<ABwayPlayerState>(P); }
		FPlayer P;
		if (!Capture(GS, Found, P)) return WaitFor(*FString::Printf(TEXT("local-player-%d-full-health-chain"), Frame.Players[Index]));
		if (P.Team != Frame.Teams[Index]) return WaitFor(*FString::Printf(TEXT("local-player-%d-team-%d-expected-%d"), P.Id, P.Team, Frame.Teams[Index]));
		if (!FMath::IsNearlyEqual(P.Health, Frame.Health[Index], .01f) || !FMath::IsNearlyEqual(P.Maximum, Frame.Maximum[Index], .01f))
			return WaitFor(*FString::Printf(TEXT("local-player-%d-health-%.6f-max-%.6f-expected-%.6f-%.6f"), P.Id, P.Health, P.Maximum, Frame.Health[Index], Frame.Maximum[Index]));
		Captured.Add(P);
	}
	const int32 SourceIndex = Captured.IndexOfByPredicate([this](const FPlayer& P) { return P.Id == Frame.Source; });
	if (SourceIndex == INDEX_NONE) return WaitFor(TEXT("local-source-player-missing"));
	const auto& Source = Captured[SourceIndex];
	auto* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	auto* Montage = FindObject<UAnimMontage>(nullptr, MontagePath);
	auto* Anim = Source.Pawn->GetMesh() ? Source.Pawn->GetMesh()->GetAnimInstance() : nullptr;
	if (!CueReady(Manager)) return WaitFor(TEXT("local-cue-handler-not-resident"));
	if (!Montage) return WaitFor(TEXT("local-montage-not-resident"));
	if (!Anim) return WaitFor(TEXT("local-animation-instance-missing"));
	if (Anim->Montage_IsActive(Montage)) return WaitFor(TEXT("local-prior-montage-active"));
	if (Guid(Source.Pawn.Get()) == 0) return WaitFor(TEXT("local-source-guid-unavailable"));
	if (!SlotsSettled(Anim)) return WaitFor(TEXT("local-montage-slots-not-settled"));
	if (OwnedBullets(GetWorld(), Source.Pawn.Get()) != 0) return WaitFor(TEXT("local-prior-owned-bullets"));
	if (Source.ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)) return WaitFor(TEXT("local-movement-stopped-tag-present"));
	if (Source.ASC->HasMatchingGameplayTag(CooldownTag())) return WaitFor(TEXT("local-cooldown-tag-present"));
	const bool bOwner = Source.Pawn->IsLocallyControlled();
	const bool bEndpoint = HasAuthority() || bOwner;
	FGameplayAbilitySpecHandle Handle;
	if (bEndpoint)
	{
		auto* S = UniqueSpec(Source.ASC.Get());
		auto* A = S ? Cast<ULyraGameplayAbility>(S->Ability.Get()) : nullptr;
		const UObject* Config = nullptr; FString KitPath;
		const auto* Stats = Source.ASC->GetSet<UBwayHeroAttributeSet>();
		if (!A) return WaitFor(TEXT("local-unique-barrage-spec-missing"));
		if (S->IsActive()) return WaitFor(TEXT("local-barrage-already-active"));
		if (!A->CheckCooldown(S->Handle, Source.ASC->AbilityActorInfo.Get())) return WaitFor(TEXT("local-barrage-cooldown-not-ready"));
		if (!ConfiguredKit(AbilityObject(S), KitPath, Config)) return WaitFor(TEXT("local-configured-kit-reference-invalid"));
		if (KitPath != Frame.KitPath) return WaitFor(*FString::Printf(TEXT("local-configured-kit-mismatch-%s"), *KitPath));
		if (!Stats) return WaitFor(TEXT("local-hero-attributes-missing"));
		if (!FMath::IsNearlyZero(Stats->GetCooldownReduction(), .001f)) return WaitFor(*FString::Printf(TEXT("local-cooldown-reduction-%.6f"), Stats->GetCooldownReduction()));
		if (A->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerActor) return WaitFor(TEXT("local-instancing-policy-unsupported"));
		if (A->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::LocalPredicted) return WaitFor(TEXT("local-execution-policy-unsupported"));
		if (A->GetNetSecurityPolicy() != EGameplayAbilityNetSecurityPolicy::ClientOrServer) return WaitFor(TEXT("local-security-policy-unsupported"));
		Handle = S->Handle; State->AbilityClass = A->GetClass();
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Policy witness=%d source=%d endpoint=%s handle=%s class=%s config=%s kitPath=%s residentBefore=%d tuningSource=expected-contract tuningVerified=0 count=%d interval=%.6f life=%.6f cooldown=%.6f speed=%.6f offset=%.6f verticalOffset=0 frameQuantum=%.6f timerQuanta=2 cooldownReduction=%.6f instancing=%d execution=%d security=%d active=%d inputPressed=%d cooldownReady=1"),
			Frame.Witness, Frame.Source, HasAuthority() ? TEXT("authority") : TEXT("owner"), *Handle.ToString(), *A->GetClass()->GetPathName(), *GetPathNameSafe(Config), *KitPath, Config != nullptr, Frame.Count, Frame.Interval, Frame.Life, Frame.Cooldown, Frame.Speed, Frame.Offset, Frame.FrameQuantum, Stats->GetCooldownReduction(), int32(A->GetInstancingPolicy()), int32(A->GetNetExecutionPolicy()), int32(A->GetNetSecurityPolicy()), S->IsActive(), S->InputPressed);
	}
	const UNetDriver* D = GetWorld()->GetNetDriver();
	if (!D) return WaitFor(TEXT("local-net-driver-missing"));
	const auto& N = D->PacketSimulationSettings;
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Network witness=%d source=%d world=%s driver=%s net=%d lag=%d lagMin=%d lagMax=%d incomingMin=%d incomingMax=%d loss=%d incomingLoss=%d jitter=%d variance=%d"),
		Frame.Witness, Frame.Source, *GetWorld()->GetPathName(), *D->GetPathName(), int32(GetNetMode()), N.PktLag, N.PktLagMin, N.PktLagMax, N.PktIncomingLagMin, N.PktIncomingLagMax, N.PktLoss, N.PktIncomingLoss, N.PktJitter, N.PktLagVariance);
	if ((N.PktLag != 0 && N.PktLag != 100) || N.PktLag != Frame.Lag || N.PktLagMin || N.PktLagMax || N.PktIncomingLagMin
		|| N.PktIncomingLagMax || N.PktLoss || N.PktIncomingLoss || N.PktJitter || N.PktLagVariance) { Fail(TEXT("unsupported-network-settings")); return false; }
	State->Roster = MoveTemp(Captured); State->SourceIndex = SourceIndex; State->LocalController = PC; State->LocalState = Cast<ABwayPlayerState>(PC->PlayerState);
	State->Round = Round; State->Handle = Handle; State->bOwner = bOwner; State->bEndpoint = bEndpoint; State->CueManager = Manager; State->Montage = Montage;
	State->bLocalReady = true; State->Started = FPlatformTime::Seconds();
	if (bEndpoint && !SafeLane()) { Fail(TEXT("unsafe-observed-enemy-lane")); return false; }
	State->StartupOriginalTickInterval = PrimaryActorTick.TickInterval;
	if (!FMath::IsNearlyEqual(State->StartupOriginalTickInterval, .05f)) { Fail(TEXT("startup-unexpected-probe-tick-interval")); return false; }
	State->bStartupTickOverride = true; SetActorTickInterval(0.f);
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: StartupTick witness=%d source=%d enabled=1 original=%.6f current=%.6f observations=0 elapsed=%.6f reason=local-ready-observation"),
		Frame.Witness, Frame.Source, State->StartupOriginalTickInterval, PrimaryActorTick.TickInterval, FPlatformTime::Seconds() - State->Started);
	for (const auto& P : State->Roster)
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Roster witness=%d source=%d player=%d pawn=%s guid=%llu state=%s asc=%s team=%d round=%d health=%.6f maximum=%.6f net=%d role=%d local=%d controller=%s"),
			Frame.Witness, Frame.Source, P.Id, *P.Pawn->GetPathName(), Guid(P.Pawn.Get()), *P.State->GetPathName(), *P.ASC->GetPathName(), P.Team, Frame.Round, P.Health, P.Maximum, int32(P.Pawn->GetNetMode()), int32(P.Pawn->GetLocalRole()), P.Pawn->IsLocallyControlled(), *GetPathNameSafe(P.Controller.Get()));
	Bind();
	// These Windows counters share the host QPC domain; UTC log prefixes are asynchronous.
	const double SecondsPerCycle = FPlatformTime::GetSecondsPerCycle64();
	if (!FMath::IsFinite(SecondsPerCycle) || SecondsPerCycle <= 0.) { Fail(TEXT("invalid-event-clock-frequency")); return false; }
#if PLATFORM_WINDOWS
	const TCHAR* ClockBackend = TEXT("WindowsQPC");
#else
	const TCHAR* ClockBackend = TEXT("unsupported");
#endif
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Clock witness=%d source=%d clockSchema=1 process=%u backend=%s scope=same-host-only secondsPerCycle=%.17g frequency=%.9f eventCycles=%llu"),
		Frame.Witness, Frame.Source, FPlatformProcess::GetCurrentProcessId(), ClockBackend, SecondsPerCycle, 1. / SecondsPerCycle, FPlatformTime::Cycles64());
	if (!CheckFrameQuantum()) return false;
	if (State->bChangingAim)
	{
		State->AimOrigin = State->Source()->Pawn->GetActorLocation();
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: AimMode witness=%d source=%d enabled=1 aimSchema=1 process=%u endpoint=%s pawn=%s guid=%llu yawStep=%.6f triggerCueMin=3 triggerCueMax=6 stationaryXY=%.6f stationarySpeedXY=%.6f range=%.6f x=%.6f y=%.6f z=%.6f eventCycles=%llu"),
			Frame.Witness, Frame.Source, FPlatformProcess::GetCurrentProcessId(), HasAuthority() ? TEXT("authority") : State->bOwner ? TEXT("owner") : TEXT("proxy"), *State->Source()->Pawn->GetPathName(), Guid(State->Source()->Pawn.Get()), ChangingAimYaw,
			ChangingAimStationaryTolerance, ChangingAimStationaryTolerance, Frame.Offset + Frame.Speed * (Frame.Life + 2 * Frame.FrameQuantum), State->AimOrigin.X, State->AimOrigin.Y, State->AimOrigin.Z, FPlatformTime::Cycles64());
		if (State->bEndpoint && !ObserveChangingAim(false)) return false;
	}
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Ready witness=%d source=%d serial=%d pawn=%s guid=%llu owner=%d authority=%d cueResident=1 cueTag=%s cueClass=%s montage=%s montageLength=%.6f animation=%s baselineCues=%d guard=%d"),
		Frame.Witness, Frame.Source, Frame.Serial, *State->Source()->Pawn->GetPathName(), Guid(State->Source()->Pawn.Get()), bOwner, HasAuthority(), *BwayGameplayCueTags::RawlinsBarrageFire.GetTag().ToString(), *GetPathNameSafe(ResidentCueClass(Manager)), *Montage->GetPathName(), Montage->GetPlayLength(), *Anim->GetPathName(), State->Cues, GAllowActorScriptExecutionInEditor);
	return true;
#else
	return false;
#endif
}

bool ABwayBarrageTransportProbe::Live() const
{
	if (!State || !State->bLocalReady || !State->LocalController.IsValid() || !State->LocalState.IsValid()
		|| GetOwner() != State->LocalController.Get() || State->LocalController->PlayerState != State->LocalState.Get()
		|| State->LocalState->GetPlayerId() != Frame.Witness || !State->LocalController->IsLocalController() || !State->Round.IsValid()) return false;
	auto* GS = GetWorld()->GetGameState<ABwayGameState>();
	if (!GS || GS->PlayerArray.Num() != 4 || GS->GetRoundManagement() != State->Round.Get() || State->Round->GetCurrentRoundNumber() != Frame.Round
		|| State->Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || State->Round->GetCurrentRoundState() != ERoundState::RoundActive) return false;
	for (const auto& P : State->Roster) if (!PlayerLive(P, GS)) return false;
	if (State->bEndpoint)
	{
		const auto* Source = State->Source();
		const auto* S = Source ? Source->ASC->FindAbilitySpecFromHandle(State->Handle) : nullptr;
		if (!S || !S->Ability || S->Ability->GetClass() != State->AbilityClass.Get() || UniqueSpec(Source->ASC.Get()) != S) return false;
	}
	return true;
}

bool ABwayBarrageTransportProbe::SafeLane()
{
	const auto* Source = State->Source();
	const auto* Spec = Source && Source->ASC.IsValid() ? Source->ASC->FindAbilitySpecFromHandle(State->Handle) : nullptr;
	const auto* A = AbilityObject(Spec);
	const UObject* Config = nullptr; FString KitPath;
	if (!ConfiguredKit(A, KitPath, Config) || KitPath != Frame.KitPath) return false;
	float LoadedSpeed = -1, LoadedOffset = -1, LoadedLife = -1;
	if (Config) { Float(Config, TEXT("BarrageProjectileSpeed"), LoadedSpeed); Float(Config, TEXT("BarrageSpawnForwardOffset"), LoadedOffset); Float(Config, TEXT("BarrageProjectileLifeSpan"), LoadedLife); }
	const APlayerController* PC = Source ? Source->Controller.Get() : nullptr;
	const auto* ClassProperty = A ? FindFProperty<FClassProperty>(A->GetClass(), TEXT("ProjectileClass")) : nullptr;
	UClass* ProjectileClass = ClassProperty ? Cast<UClass>(ClassProperty->GetPropertyValue_InContainer(A)) : nullptr;
	const auto* ProjectileDefault = ProjectileClass ? Cast<AActor>(ProjectileClass->GetDefaultObject(false)) : nullptr;
	const auto* Sphere = ProjectileDefault ? ProjectileDefault->FindComponentByClass<USphereComponent>() : nullptr;
	const float Radius = Sphere ? Sphere->GetScaledSphereRadius() : 0.f;
	if (!PC || !FMath::IsFinite(Radius) || Radius <= 0 || !Live() || Frame.Speed != 3600.f || Frame.Offset != 80.f
		|| Frame.Life != 1.5f || Frame.FrameQuantum != .2f) return false;
	FVector ViewLocation; FRotator View; PC->GetPlayerViewPoint(ViewLocation, View);
	const FVector Direction = View.Vector();
	const FVector Start = Source->Pawn->GetActorLocation();
	if (Start.ContainsNaN() || Direction.ContainsNaN()) return false;
	const double Range = Frame.Offset + Frame.Speed * (Frame.Life + 2 * Frame.FrameQuantum);
	int32 EnemyRisk = 0, FriendlyRisk = 0;
	double Clearance = TNumericLimits<double>::Max();
	// Production spawns from the actor center with a forward offset only. A finite
	// segment starting at the center is conservative, including that offset. Two
	// bounded world-frame quanta cover pending lifespan activation and expiry.
	// These expected values remain unverified until production loads the kit.
	for (const auto& P : State->Roster) if (P.Id != Frame.Source)
	{
		const FVector Position = P.Pawn->GetActorLocation();
		const float CapsuleBound = P.Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (Position.ContainsNaN() || !FMath::IsFinite(CapsuleBound) || CapsuleBound <= 0) return false;
		const double Along = FMath::Clamp(FVector::DotProduct(Position - Start, Direction), 0., Range);
		const double Margin = FVector::Distance(Position, Start + Direction * Along) - CapsuleBound - Radius;
		if (P.Team != Source->Team) { Clearance = FMath::Min(Clearance, Margin); if (Margin <= 0) ++EnemyRisk; }
		else if (Margin <= 0) ++FriendlyRisk;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: LaneTarget witness=%d source=%d stage=%d cycle=%d shot=%d player=%d pawn=%s enemy=%d x=%.6f y=%.6f z=%.6f capsuleBound=%.6f along=%.6f margin=%.6f"),
			Frame.Witness, Frame.Source, Frame.Stage, State->Activations, State->Shots.Num(), P.Id, *P.Pawn->GetPathName(), P.Team != Source->Team, Position.X, Position.Y, Position.Z, CapsuleBound, Along, Margin);
	}
	State->LaneRadius = Radius;
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Lane witness=%d source=%d endpoint=%s stage=%d cycle=%d shot=%d pawn=%s projectileClass=%s radius=%.6f speed=%.6f life=%.6f offset=%.6f verticalOffset=0 frameQuantum=%.6f timerQuanta=2 range=%.6f projectionScope=finite-expected-contract resident=%d loadedSpeed=%.6f loadedLife=%.6f loadedOffset=%.6f viewYaw=%.6f viewPitch=%.6f x=%.6f y=%.6f z=%.6f dx=%.6f dy=%.6f dz=%.6f enemyRisk=%d friendlyRisk=%d clearance=%.6f safe=%d"),
		Frame.Witness, Frame.Source, HasAuthority() ? TEXT("authority") : TEXT("owner"), Frame.Stage, State->Activations, State->Shots.Num(), *Source->Pawn->GetPathName(), *GetPathNameSafe(ProjectileClass), Radius, Frame.Speed, Frame.Life, Frame.Offset, Frame.FrameQuantum, Range, Config != nullptr, LoadedSpeed, LoadedLife, LoadedOffset, View.Yaw, View.Pitch, Start.X, Start.Y, Start.Z, Direction.X, Direction.Y, Direction.Z, EnemyRisk, FriendlyRisk, Clearance, EnemyRisk == 0);
	return EnemyRisk == 0;
}

bool ABwayBarrageTransportProbe::ObserveChangingAim(bool bAllowInput)
{
#if !UE_BUILD_SHIPPING
	if (!State || !State->bChangingAim || !State->bEndpoint) return true;
	const uint64 ClockBegin = FPlatformTime::Cycles64();
	const auto* Source = State->Source();
	auto* PC = Source ? Source->Controller.Get() : nullptr;
	auto* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
	const auto* Spec = Source && Source->ASC.IsValid() ? Source->ASC->FindAbilitySpecFromHandle(State->Handle) : nullptr;
	double Clearance = 0;
	if (!Live() || !PC || !Camera || !ChangingAimSafe(*State, Frame, Clearance)) { Fail(TEXT("changing-aim-source-or-stationary-reach-guard")); return false; }
	const FVector Position = Source->Pawn->GetActorLocation(), Velocity = Source->Pawn->GetVelocity();
	const FVector PendingMove = Source->Pawn->GetPendingMovementInputVector(), LastMove = Source->Pawn->GetLastMovementInputVector();
	const FRotator Control = PC->GetControlRotation(), Pending = PC->RotationInput;
	FVector ViewLocation; FRotator View; PC->GetPlayerViewPoint(ViewLocation, View);
	const FMinimalViewInfo Cache = Camera->GetCameraCacheView(), PreviousCache = Camera->GetLastFrameCameraCacheView();
	const float CacheTime = Camera->GetCameraCacheTime(), PreviousCacheTime = Camera->GetLastFrameCameraCacheTime();
	const bool Legacy = GetDefault<UInputSettings>()->bEnableLegacyInputScales;
	const float Scale = Legacy ? PC->InputYawScale_DEPRECATED : 1.f;
	const uint64 ClockEnd = FPlatformTime::Cycles64();
	if (Control.ContainsNaN() || Pending.ContainsNaN() || View.ContainsNaN() || ViewLocation.ContainsNaN()
		|| Cache.Rotation.ContainsNaN() || Cache.Location.ContainsNaN() || PreviousCache.Rotation.ContainsNaN()
		|| PendingMove.ContainsNaN() || LastMove.ContainsNaN() || !FMath::IsFinite(CacheTime) || !FMath::IsFinite(PreviousCacheTime) || !FMath::IsFinite(Scale))
	{ Fail(TEXT("changing-aim-nonfinite-readback")); return false; }
	if (!bAllowInput)
	{
		const auto* Network = GetDefault<AGameNetworkManager>();
		const auto* StationaryRotation = IConsoleManager::Get().FindConsoleVariable(TEXT("p.NetStationaryRotationTolerance"));
		float CameraTimeout = -1.f;
		const bool TimeoutAvailable = Float(Camera, TEXT("ServerUpdateCameraTimeout"), CameraTimeout);
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: AimPolicy witness=%d source=%d endpoint=%s controller=%s camera=%s legacyScale=%d yawScale=%.6f ignoreLook=%d yawMin=%.6f yawMax=%.6f clientCamera=%d debugCamera=%d cameraMinInterval=%.6f cameraPositionLimit=%.6f cameraTimeout=%.6f cameraTimeoutAvailable=%d moveInterval=%.6f moveThrottledInterval=%.6f moveStationaryInterval=%.6f stationaryRotationTolerance=%.6f stationaryRotationAvailable=%d eventCycles=%llu"),
			Frame.Witness, Frame.Source, HasAuthority() ? TEXT("authority") : TEXT("owner"), *PC->GetPathName(), *Camera->GetPathName(), Legacy, Scale, PC->IsLookInputIgnored(), Camera->ViewYawMin, Camera->ViewYawMax, Camera->bUseClientSideCameraUpdates, Camera->bDebugClientSideCamera,
			Network->ClientNetCamUpdateDeltaTime, Network->ClientNetCamUpdatePositionLimit, CameraTimeout, TimeoutAvailable, Network->ClientNetSendMoveDeltaTime, Network->ClientNetSendMoveDeltaTimeThrottled, Network->ClientNetSendMoveDeltaTimeStationary,
			StationaryRotation ? StationaryRotation->GetFloat() : -1.f, StationaryRotation != nullptr, FPlatformTime::Cycles64());
	}
	const double ControlDelta = FRotator::NormalizeAxis(Control.Yaw - State->AimControlBefore.Yaw);
	if (State->bOwner && State->bAimInputQueued && Pending.IsNearlyZero(.001f)
		&& FMath::IsNearlyEqual(ControlDelta, double(ChangingAimYaw), .01)) State->bAimInputConsumed = true;
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: AimSample witness=%d source=%d endpoint=%s serial=%d stage=%d cycle=%d observation=%d cues=%d pawn=%s guid=%llu controller=%s camera=%s instance=%s handle=%s key=%d active=%d movementTag=%d local=%d legacyScale=%d yawScale=%.6f ignoreLook=%d inputQueued=%d inputConsumed=%d inputCycles=%llu pendingPitch=%.6f pendingYaw=%.6f pendingRoll=%.6f controlPitch=%.6f controlYaw=%.6f controlRoll=%.6f controlDelta=%.6f viewPitch=%.6f viewYaw=%.6f viewRoll=%.6f viewX=%.6f viewY=%.6f viewZ=%.6f cachePitch=%.6f cacheYaw=%.6f cacheRoll=%.6f cacheX=%.6f cacheY=%.6f cacheZ=%.6f cacheTime=%.6f previousCachePitch=%.6f previousCacheYaw=%.6f previousCacheRoll=%.6f previousCacheTime=%.6f clientCamera=%d sendRequested=%d x=%.6f y=%.6f z=%.6f vx=%.6f vy=%.6f vz=%.6f pendingMoveX=%.6f pendingMoveY=%.6f pendingMoveZ=%.6f lastMoveX=%.6f lastMoveY=%.6f lastMoveZ=%.6f displacementXY=%.6f speedXY=%.6f reachClearance=%.6f clockBegin=%llu clockEnd=%llu frame=%llu elapsed=%.6f worldTime=%.6f"),
		Frame.Witness, Frame.Source, HasAuthority() ? TEXT("authority") : TEXT("owner"), Frame.Serial, Frame.Stage, State->Activations, State->StateObservations, State->Cues, *Source->Pawn->GetPathName(), Guid(Source->Pawn.Get()), *PC->GetPathName(), *Camera->GetPathName(), *GetPathNameSafe(State->Instance.Get()), *State->Handle.ToString(), State->Key.Current,
		Spec ? int32(Spec->IsActive()) : -1, Source->ASC->GetTagCount(TAG_Gameplay_MovementStopped), PC->IsLocalController(), Legacy, Scale, PC->IsLookInputIgnored(), State->bAimInputQueued, State->bAimInputConsumed, State->AimInputCycles,
		Pending.Pitch, Pending.Yaw, Pending.Roll, Control.Pitch, Control.Yaw, Control.Roll, ControlDelta, View.Pitch, View.Yaw, View.Roll, ViewLocation.X, ViewLocation.Y, ViewLocation.Z, Cache.Rotation.Pitch, Cache.Rotation.Yaw, Cache.Rotation.Roll, Cache.Location.X, Cache.Location.Y, Cache.Location.Z, CacheTime, PreviousCache.Rotation.Pitch, PreviousCache.Rotation.Yaw, PreviousCache.Rotation.Roll, PreviousCacheTime, Camera->bUseClientSideCameraUpdates, Camera->bShouldSendClientSideCameraUpdate,
		Position.X, Position.Y, Position.Z, Velocity.X, Velocity.Y, Velocity.Z, PendingMove.X, PendingMove.Y, PendingMove.Z, LastMove.X, LastMove.Y, LastMove.Z, (Position - State->AimOrigin).Size2D(), Velocity.Size2D(), Clearance, ClockBegin, ClockEnd, GFrameCounter, FPlatformTime::Seconds() - State->Started, GetWorld()->GetTimeSeconds());
	if (bAllowInput && State->bOwner && !HasAuthority() && Frame.Stage == CastOne && !State->bAimInputQueued && State->Cues >= 3)
	{
		const float RawInput = FMath::Abs(Scale) > SMALL_NUMBER ? ChangingAimYaw / Scale : 0.f;
		const bool Exact = Frame.Witness == Frame.Source && State->LocalController.Get() == PC && PC->IsLocalController()
			&& Source->Pawn->IsLocallyControlled() && Spec && Spec->IsActive() && Spec->GetPrimaryInstance() == State->Instance.Get() && State->Instance.IsValid()
			&& State->Instance->GetCurrentAbilitySpecHandle() == State->Handle && State->Instance->GetCurrentActivationInfo().GetActivationPredictionKey() == State->Key
			&& State->Activations == 1 && State->Ends == 0 && State->TunedActivation == 1 && State->Cues <= 6
			&& Source->ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped) && !PC->IsLookInputIgnored() && Pending.IsNearlyZero(.001f)
			&& PendingMove.IsNearlyZero(.001f) && LastMove.IsNearlyZero(.001f)
			&& FMath::IsFinite(RawInput) && FMath::Abs(RawInput) > SMALL_NUMBER && FMath::Abs(RawInput) <= 120.f;
		if (!Exact) { Fail(TEXT("changing-aim-input-admission")); return false; }
		State->AimControlBefore = Control; State->bAimInputQueued = true;
		State->AimInputCycles = FPlatformTime::Cycles64();
		Source->Pawn->AddControllerYawInput(RawInput); // Exactly one normal, transient input impulse.
		const FRotator After = PC->RotationInput;
		const uint64 InputEnd = FPlatformTime::Cycles64();
		const bool Enqueued = !After.ContainsNaN() && FMath::IsNearlyEqual(After.Yaw - Pending.Yaw, double(ChangingAimYaw), .001)
			&& After.Pitch == Pending.Pitch && After.Roll == Pending.Roll;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: AimInput witness=%d source=%d endpoint=owner serial=%d stage=%d cycle=%d pawn=%s guid=%llu controller=%s instance=%s handle=%s key=%d cues=%d exact=%d rawInput=%.6f legacyScale=%d yawScale=%.6f intendedYaw=%.6f beforePitch=%.6f beforeYaw=%.6f beforeRoll=%.6f afterPitch=%.6f afterYaw=%.6f afterRoll=%.6f controlPitch=%.6f controlYaw=%.6f controlRoll=%.6f viewPitch=%.6f viewYaw=%.6f cachePitch=%.6f cacheYaw=%.6f cacheTime=%.6f enqueued=%d readBegin=%llu eventCycles=%llu clockEnd=%llu frame=%llu elapsed=%.6f worldTime=%.6f"),
			Frame.Witness, Frame.Source, Frame.Serial, Frame.Stage, State->Activations, *Source->Pawn->GetPathName(), Guid(Source->Pawn.Get()), *PC->GetPathName(), *GetPathNameSafe(State->Instance.Get()), *State->Handle.ToString(), State->Key.Current, State->Cues, Exact, RawInput, Legacy, Scale, ChangingAimYaw,
			Pending.Pitch, Pending.Yaw, Pending.Roll, After.Pitch, After.Yaw, After.Roll, Control.Pitch, Control.Yaw, Control.Roll, View.Pitch, View.Yaw, Cache.Rotation.Pitch, Cache.Rotation.Yaw, CacheTime, Enqueued, ClockBegin, State->AimInputCycles, InputEnd, GFrameCounter, FPlatformTime::Seconds() - State->Started, GetWorld()->GetTimeSeconds());
		if (!Enqueued) { Fail(TEXT("changing-aim-input-not-enqueued")); return false; }
	}
#endif
	return true;
}

bool ABwayBarrageTransportProbe::CheckFrameQuantum()
{
	if (!HasAuthority() || !State || !State->bBound || State->bCleaned) return true;
	if (!State->Failure.IsEmpty()) return false;
	const float Delta = GetWorld()->GetDeltaSeconds();
	const double WorldTime = GetWorld()->GetTimeSeconds();
	// A network activation can run before this world's clock advances, in the
	// same GFrameCounter as the later actor tick. Never reuse that earlier delta.
	if (State->LastFrameObserved == GFrameCounter && State->LastObservedWorldTime == WorldTime && State->LastObservedWorldDelta == Delta) return true;
	State->LastFrameObserved = GFrameCounter; State->LastObservedWorldTime = WorldTime; State->LastObservedWorldDelta = Delta;
	// The enabled .05s tick can skip smaller frames, but a >.20s world delta
	// necessarily expires its cooldown. This is an observed peak, not every frame.
	const bool Valid = FMath::IsFinite(Delta) && Delta >= 0 && (!State->bFlightBoundArmed || Delta <= Frame.FrameQuantum)
		&& IsActorTickEnabled() && GetActorTickInterval() <= .05f && Frame.FrameQuantum == .2f;
	if (State->bFlightBoundArmed)
	{
		State->ObservedFramePeak = FMath::Max(State->ObservedFramePeak, Delta); ++State->FrameSamples;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: FrameBound witness=%d source=%d stage=%d cycle=%d armed=1 frame=%llu delta=%.6f limit=%.6f observedPeak=%.6f samples=%d tickInterval=%.6f enabled=%d valid=%d ownedBullets=%d elapsed=%.6f worldTime=%.6f"),
			Frame.Witness, Frame.Source, Frame.Stage, State->FlightBoundCycle, GFrameCounter, Delta, Frame.FrameQuantum, State->ObservedFramePeak, State->FrameSamples, GetActorTickInterval(), IsActorTickEnabled(), Valid, OwnedBullets(GetWorld(), State->Source() ? State->Source()->Pawn.Get() : nullptr), FPlatformTime::Seconds() - State->Started, WorldTime);
	}
	else
	{
		State->ObservedIdleFramePeak = FMath::Max(State->ObservedIdleFramePeak, Delta); ++State->IdleFrameSamples;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: FrameIdle witness=%d source=%d stage=%d cycle=%d armed=0 limitApplied=0 frame=%llu delta=%.6f observedPeak=%.6f samples=%d tickInterval=%.6f enabled=%d valid=%d ownedBullets=%d elapsed=%.6f worldTime=%.6f"),
			Frame.Witness, Frame.Source, Frame.Stage, State->FlightBoundCycle, GFrameCounter, Delta, State->ObservedIdleFramePeak, State->IdleFrameSamples, GetActorTickInterval(), IsActorTickEnabled(), Valid, OwnedBullets(GetWorld(), State->Source() ? State->Source()->Pawn.Get() : nullptr), FPlatformTime::Seconds() - State->Started, WorldTime);
	}
	if (!Valid) { Fail(TEXT("authority-world-frame-outside-lane-bound")); return false; }
	return true;
}

bool ABwayBarrageTransportProbe::VerifyRuntimeTuning()
{
	if (State->Activations > 0 && State->TunedActivation == State->Activations) return true;
	const auto* Source = State->Source();
	const auto* Spec = Source && Source->ASC.IsValid() ? Source->ASC->FindAbilitySpecFromHandle(State->Handle) : nullptr;
	const auto* Instance = State->Instance.Get();
	const bool Exact = Live() && Spec && Instance && Spec->GetPrimaryInstance() == Instance && Instance->IsActive()
		&& Instance->GetCurrentAbilitySpecHandle() == State->Handle && Instance->GetCurrentActivationInfo().GetActivationPredictionKey() == State->Key;
	FBwayBarrageTransportFrame Loaded; const UObject* Config = nullptr;
	const bool Resolved = Exact && Tuning(Instance, Loaded, Config);
	const auto* Stats = Source && Source->ASC.IsValid() ? Source->ASC->GetSet<UBwayHeroAttributeSet>() : nullptr;
	const bool Matches = Resolved && Loaded.KitPath == Frame.KitPath && Config->GetPathName() == Frame.KitPath
		&& Loaded.Count == Frame.Count && FMath::IsNearlyEqual(Loaded.Interval, Frame.Interval, .000001f)
		&& FMath::IsNearlyEqual(Loaded.Life, Frame.Life, .000001f) && FMath::IsNearlyEqual(Loaded.Cooldown, Frame.Cooldown, .000001f)
		&& FMath::IsNearlyEqual(Loaded.Speed, Frame.Speed, .001f) && FMath::IsNearlyEqual(Loaded.Offset, Frame.Offset, .000001f)
		&& Stats && FMath::IsNearlyZero(Stats->GetCooldownReduction(), .001f);
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: TuningVerified witness=%d source=%d endpoint=%s cycle=%d instance=%s handle=%s key=%d exact=%d config=%s kitPath=%s expectedKitPath=%s resident=%d count=%d interval=%.6f life=%.6f cooldown=%.6f speed=%.6f offset=%.6f verticalOffset=0 expectedSpeed=%.6f expectedOffset=%.6f cooldownReduction=%.6f matched=%d productionResolved=%d elapsed=%.6f worldTime=%.6f"),
		Frame.Witness, Frame.Source, HasAuthority() ? TEXT("authority") : TEXT("owner"), State->Activations, *GetPathNameSafe(Instance), *State->Handle.ToString(), State->Key.Current, Exact, *GetPathNameSafe(Config), *Loaded.KitPath, *Frame.KitPath, Config != nullptr,
		Loaded.Count, Loaded.Interval, Loaded.Life, Loaded.Cooldown, Loaded.Speed, Loaded.Offset, Frame.Speed, Frame.Offset, Stats ? Stats->GetCooldownReduction() : -1.f, Matches, Resolved, FPlatformTime::Seconds() - State->Started, GetWorld()->GetTimeSeconds());
	if (!Exact) { Fail(TEXT("runtime-tuning-activation-identity")); return false; }
	if (!Resolved) { Fail(TEXT("production-kit-not-resolved-or-fields-missing")); return false; }
	if (!Matches) { Fail(TEXT("production-kit-values-or-path-do-not-match-contract")); return false; }
	State->TunedActivation = State->Activations;
	return true;
}

void ABwayBarrageTransportProbe::FinalizeShot()
{
	if (!State || State->Shots.IsEmpty() || !CheckFrameQuantum()) return;
	const uint64 AimClockBegin = State->bChangingAim ? FPlatformTime::Cycles64() : 0;
	auto& Shot = State->Shots.Last();
	if (Shot.bFinalized || Shot.Index != State->Cues) { Fail(TEXT("post-cue-projectile-order")); return; }
	Shot.bFinalized = true;
	const AActor* Projectile = Shot.Actor.Get();
	const auto* Move = Projectile ? Projectile->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
	const auto* Sphere = Projectile ? Projectile->FindComponentByClass<USphereComponent>() : nullptr;
	const auto* Source = State->Source();
	FVector ViewLocation; FRotator View = FRotator::ZeroRotator;
	if (Source && Source->Controller.IsValid()) Source->Controller->GetPlayerViewPoint(ViewLocation, View);
	Shot.Origin = Source && Source->Pawn.IsValid() ? Source->Pawn->GetActorLocation() : FVector::ZeroVector;
	Shot.Direction = View.Vector();
	Shot.Start = Projectile ? Projectile->GetActorLocation() : FVector::ZeroVector;
	const FVector Velocity = Move ? Move->Velocity : FVector::ZeroVector;
	const float Radius = Sphere ? Sphere->GetScaledSphereRadius() : -1.f;
	const float LifeRemaining = Projectile ? Projectile->GetLifeSpan() : -1.f;
	const double SpawnError = FVector::Distance(Shot.Start, Shot.Origin + Shot.Direction * Frame.Offset);
	const double VelocityError = FVector::Distance(Velocity, Shot.Direction * Frame.Speed);
	const bool Exact = Live() && Source && Projectile && Projectile->GetOwner() == Source->Pawn.Get()
		&& Projectile->GetInstigator() == Source->Pawn.Get() && Shot.Cycle == State->TunedActivation;
	// This callback follows ConfigureProjectile and FinishSpawning. It does not
	// misinterpret the deferred spawn callback's uninitialized movement as proof.
	const bool Matches = Exact && Move && Sphere && !Shot.Start.ContainsNaN() && !Velocity.ContainsNaN()
		&& FMath::IsNearlyEqual(Move->InitialSpeed, Frame.Speed, .01f) && FMath::IsNearlyEqual(Move->MaxSpeed, Frame.Speed, .01f)
		&& FMath::IsNearlyZero(Move->ProjectileGravityScale, .000001f) && !Move->bIsHomingProjectile && !Move->bShouldBounce
		&& Move->bSimulationEnabled && !Move->bInitialVelocityInLocalSpace && !Sphere->IsSimulatingPhysics()
		&& FMath::IsNearlyEqual(Projectile->CustomTimeDilation, 1.f, .000001f)
		&& FMath::IsFinite(Radius) && Radius > 0 && Radius <= State->LaneRadius + .001f
		&& FMath::IsNearlyEqual(LifeRemaining, Frame.Life, .001f) && SpawnError <= .01 && VelocityError <= .01;
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: ShotFinalized witness=%d source=%d cycle=%d index=%d projectile=%s present=%d movement=%d initialized=post-cue exact=%d matched=%d vx=%.6f vy=%.6f vz=%.6f speed=%.6f initialSpeed=%.6f maxSpeed=%.6f gravity=%.6f radius=%.6f expectedRadius=%.6f lifeRemaining=%.6f timeDilation=%.6f homing=%d bounce=%d simulation=%d localVelocity=%d physics=%d x=%.6f y=%.6f z=%.6f originX=%.6f originY=%.6f originZ=%.6f dx=%.6f dy=%.6f dz=%.6f viewYaw=%.6f viewPitch=%.6f spawnError=%.6f velocityError=%.6f elapsed=%.6f age=%.6f worldTime=%.6f worldAge=%.6f frame=%llu"),
		Frame.Witness, Frame.Source, Shot.Cycle, Shot.Index, *Shot.Path, Projectile != nullptr, Move != nullptr, Exact, Matches,
		Velocity.X, Velocity.Y, Velocity.Z, Velocity.Size(), Move ? Move->InitialSpeed : -1.f, Move ? Move->MaxSpeed : -1.f, Move ? Move->ProjectileGravityScale : -1.f,
		Radius, State->LaneRadius, LifeRemaining, Projectile ? Projectile->CustomTimeDilation : -1.f, Move ? int32(Move->bIsHomingProjectile) : -1, Move ? int32(Move->bShouldBounce) : -1,
		Move ? int32(Move->bSimulationEnabled) : -1, Move ? int32(Move->bInitialVelocityInLocalSpace) : -1, Sphere ? int32(Sphere->IsSimulatingPhysics()) : -1,
		Shot.Start.X, Shot.Start.Y, Shot.Start.Z, Shot.Origin.X, Shot.Origin.Y, Shot.Origin.Z, Shot.Direction.X, Shot.Direction.Y, Shot.Direction.Z, View.Yaw, View.Pitch,
		SpawnError, VelocityError, FPlatformTime::Seconds() - State->Started, FPlatformTime::Seconds() - Shot.At, GetWorld()->GetTimeSeconds(), GetWorld()->GetTimeSeconds() - Shot.WorldAt, GFrameCounter);
	if (State->bChangingAim)
	{
		double Clearance = 0;
		const bool AimSafe = ChangingAimSafe(*State, Frame, Clearance);
		const auto* PC = Source ? Source->Controller.Get() : nullptr;
		const auto* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
		const FRotator Control = PC ? PC->GetControlRotation() : FRotator::ZeroRotator;
		const FRotator Cache = Camera ? Camera->GetCameraCacheView().Rotation : FRotator::ZeroRotator;
		const float CacheTime = Camera ? Camera->GetCameraCacheTime() : -1.f;
		const uint64 AimClockEnd = FPlatformTime::Cycles64();
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: AimShot witness=%d source=%d endpoint=authority serial=%d stage=%d cycle=%d index=%d projectile=%s pawn=%s guid=%llu controller=%s instance=%s handle=%s key=%d exact=%d matched=%d stationarySafe=%d reachClearance=%.6f controlPitch=%.6f controlYaw=%.6f viewPitch=%.6f viewYaw=%.6f cachePitch=%.6f cacheYaw=%.6f cacheTime=%.6f dx=%.6f dy=%.6f dz=%.6f vx=%.6f vy=%.6f vz=%.6f x=%.6f y=%.6f z=%.6f clockBegin=%llu clockEnd=%llu frame=%llu elapsed=%.6f worldTime=%.6f"),
			Frame.Witness, Frame.Source, Frame.Serial, Frame.Stage, Shot.Cycle, Shot.Index, *Shot.Path, *GetPathNameSafe(Source ? Source->Pawn.Get() : nullptr), Guid(Source ? Source->Pawn.Get() : nullptr), *GetPathNameSafe(PC), *GetPathNameSafe(State->Instance.Get()), *State->Handle.ToString(), State->Key.Current, Exact, Matches, AimSafe, Clearance,
			Control.Pitch, Control.Yaw, View.Pitch, View.Yaw, Cache.Pitch, Cache.Yaw, CacheTime, Shot.Direction.X, Shot.Direction.Y, Shot.Direction.Z, Velocity.X, Velocity.Y, Velocity.Z, Shot.Origin.X, Shot.Origin.Y, Shot.Origin.Z, AimClockBegin, AimClockEnd, GFrameCounter, FPlatformTime::Seconds() - State->Started, GetWorld()->GetTimeSeconds());
		if (!AimSafe || !PC || !Camera) { Fail(TEXT("changing-aim-shot-stationary-reach-guard")); return; }
	}
	if (!Matches) Fail(TEXT("initialized-projectile-outside-expected-lane-contract"));
}

void ABwayBarrageTransportProbe::Bind()
{
#if !UE_BUILD_SHIPPING
	if (State->bBound) return;
	State->bBound = true;
	const TWeakObjectPtr<ABwayBarrageTransportProbe> Weak(this);
	for (int32 I = 0; I < State->Roster.Num(); ++I)
		State->Roster[I].HealthDelegate = State->Roster[I].ASC->GetGameplayAttributeValueChangeDelegate(ULyraHealthSet::GetHealthAttribute()).AddLambda([Weak, I](const FOnAttributeChangeData& D)
		{
			ABwayBarrageTransportProbe* P = Weak.Get(); if (!P || !P->State || P->State->bCleaned) return;
			const bool Changed = !FMath::IsFinite(D.OldValue) || !FMath::IsFinite(D.NewValue) || D.OldValue != D.NewValue;
			if (Changed) ++P->State->HealthChanges;
			UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Health witness=%d player=%d before=%.6f after=%.6f changed=%d elapsed=%.6f"), P->Frame.Witness, P->State->Roster[I].Id, D.OldValue, D.NewValue, Changed, FPlatformTime::Seconds() - P->State->Started);
			if (Changed) P->Fail(TEXT("actual-health-change"));
		});
	State->CueDelegate = State->CueManager->OnGameplayCueRouted().AddLambda([Weak](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event, const FGameplayCueParameters& Params, EGameplayCueExecutionOptions)
	{
		ABwayBarrageTransportProbe* P = Weak.Get(); if (!P || !P->State || P->State->bCleaned || Tag != BwayGameplayCueTags::RawlinsBarrageFire) return;
		auto& S = *P->State;
		if (!S.Source() || Target != S.Source()->Pawn.Get()) return;
		if (!S.Cues && !P->HandoffStartup(TEXT("first-cue"), false)) return;
		++S.Cues; S.LastCueAt = FPlatformTime::Seconds();
		const bool Exact = P->Live() && Event == EGameplayCueEvent::Executed && Params.Instigator.Get() == Target
			&& Params.EffectCauser.Get() == Target && CueReady(S.CueManager.Get());
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Cue witness=%d source=%d index=%d cycle=%d pawn=%s guid=%llu event=%d instigator=%s causer=%s exact=%d owner=%d net=%d elapsed=%.6f worldTime=%.6f"),
			P->Frame.Witness, P->Frame.Source, S.Cues, (S.Cues - 1) / P->Frame.Count + 1, *GetPathNameSafe(Target), Guid(Target), int32(Event), *GetPathNameSafe(Params.Instigator.Get()), *GetPathNameSafe(Params.EffectCauser.Get()), Exact, S.bOwner, int32(P->GetNetMode()), S.LastCueAt - S.Started, P->GetWorld()->GetTimeSeconds());
		if (!Exact || S.Cues > P->Frame.Count * 2) P->Fail(TEXT("unexpected-cue"));
		else if (P->HasAuthority()) P->FinalizeShot();
	});
	if (State->bEndpoint)
	{
		auto* ASC = State->Source()->ASC.Get();
		if (State->bOwner && !HasAuthority())
		{
			// Bind once before either cast. The replicated effect can be added and removed
			// between polls; the ASC removal event retains its actual effect identity.
			const TWeakObjectPtr<ULyraAbilitySystemComponent> CapturedASC(ASC);
			State->OwnerEffectRemovalDelegate = ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda([Weak, CapturedASC](const FActiveGameplayEffect& E)
			{
				const uint64 EventCycles = FPlatformTime::Cycles64();
				auto* P = Weak.Get(); if (!P || !P->State || P->State->bCleaned) return;
				FGameplayTagContainer GrantedTags; E.Spec.GetAllGrantedTags(GrantedTags);
				if (!GrantedTags.HasTagExact(CooldownTag())) return;
				auto& S = *P->State; auto* C = CapturedASC.Get();
				const auto* Source = S.Source(); const auto* Spec = C ? C->FindAbilitySpecFromHandle(S.Handle) : nullptr;
				const bool Exact = P->Live() && Source && C && Source->ASC.Get() == C && S.Instance.IsValid()
					&& Spec && Spec->GetPrimaryInstance() == S.Instance.Get() && S.Instance->GetCurrentAbilitySpecHandle() == S.Handle
					&& S.Activations >= 1 && S.Activations <= 2 && E.Spec.Def && E.PredictionKey == S.Key
					&& E.Handle.IsValid() && E.IsPendingRemove && E.GetDuration() == P->Frame.Cooldown;
				++S.OwnerEffectRemovals;
				if (E.bPostPredictObject) ++S.OwnerReplicatedEffectRemovals;
				// bPostPredictObject is set by PostReplicatedAdd for the owning client's
				// predicted key. This identifies effect origin, not the removal execution route.
				UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: CooldownRemoved witness=%d source=%d endpoint=owner cycle=%d pawn=%s asc=%s instance=%s abilityHandle=%s key=%d handle=%s definition=%s start=%.6f serverStart=%.6f duration=%.6f remaining=%.6f predictionKey=%d postPredictObject=%d effectOrigin=%s pendingRemove=%d exact=%d removals=%d replicatedRemovals=%d eventCycles=%llu elapsed=%.6f worldTime=%.6f"),
					P->Frame.Witness, P->Frame.Source, S.Activations, *GetPathNameSafe(Source ? Source->Pawn.Get() : nullptr), *GetPathNameSafe(C), *GetPathNameSafe(S.Instance.Get()), *S.Handle.ToString(), S.Key.Current,
					*E.Handle.ToString(), *GetPathNameSafe(E.Spec.Def.Get()), E.StartWorldTime, E.StartServerWorldTime, E.GetDuration(), E.GetTimeRemaining(P->GetWorld()->GetTimeSeconds()), E.PredictionKey.Current,
					E.bPostPredictObject, E.bPostPredictObject ? TEXT("replicated-owner") : TEXT("predicted-or-other"), E.IsPendingRemove, Exact, S.OwnerEffectRemovals, S.OwnerReplicatedEffectRemovals, EventCycles, FPlatformTime::Seconds() - S.Started, P->GetWorld()->GetTimeSeconds());
				if (!Exact) P->Fail(TEXT("owner-cooldown-removal-context-changed"));
			});
		}
		State->ActivationDelegate = ASC->AbilityActivatedCallbacks.AddLambda([Weak](UGameplayAbility* A)
		{
			const uint64 EventCycles = FPlatformTime::Cycles64();
			ABwayBarrageTransportProbe* P = Weak.Get(); if (!P || !P->State || !A || A->GetCurrentAbilitySpecHandle() != P->State->Handle) return;
			auto& S = *P->State; ++S.Activations; S.Instance = Cast<ULyraGameplayAbility>(A); S.Key = A->GetCurrentActivationInfo().GetActivationPredictionKey();
			if (S.Activations == 1 && P->HasAuthority() && !P->CheckStartupActivation(false)) return;
			if (S.Activations == 1 && !P->HandoffStartup(P->HasAuthority() ? TEXT("authority-activation") : TEXT("owner-activation"), false)) return;
			const int32 Expected = P->Frame.Stage == CastOne ? 1 : P->Frame.Stage == CastTwo ? 2 : 0;
			const bool Exact = P->Live() && S.Instance.IsValid() && S.Key.IsValidKey() && S.Activations == Expected && S.Ends == Expected - 1;
			UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Activation witness=%d source=%d endpoint=%s cycle=%d instance=%s handle=%s key=%d keyBase=%d exact=%d elapsed=%.6f worldTime=%.6f eventCycles=%llu"),
				P->Frame.Witness, P->Frame.Source, P->HasAuthority() ? TEXT("authority") : TEXT("owner"), S.Activations, *GetPathNameSafe(A), *S.Handle.ToString(), S.Key.Current, S.Key.Base, Exact, FPlatformTime::Seconds() - S.Started, P->GetWorld()->GetTimeSeconds(), EventCycles);
			if (!Exact) P->Fail(TEXT("unexpected-activation"));
			else if (P->HasAuthority())
			{
				if (S.bFlightBoundArmed) { P->Fail(TEXT("flight-bound-already-armed")); return; }
				S.bFlightBoundArmed = true; S.FlightBoundCycle = S.Activations;
				S.LastFrameObserved = MAX_uint64; // An idle sample never satisfies this armed phase.
				UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: FrameBoundArm witness=%d source=%d stage=%d cycle=%d instance=%s handle=%s key=%d frame=%llu delta=%.6f limit=%.6f ownedBullets=%d elapsed=%.6f worldTime=%.6f"),
					P->Frame.Witness, P->Frame.Source, P->Frame.Stage, S.FlightBoundCycle, *GetPathNameSafe(A), *S.Handle.ToString(), S.Key.Current, GFrameCounter, P->GetWorld()->GetDeltaSeconds(), P->Frame.FrameQuantum, OwnedBullets(P->GetWorld(), S.Source()->Pawn.Get()), FPlatformTime::Seconds() - S.Started, P->GetWorld()->GetTimeSeconds());
				P->CheckFrameQuantum();
			}
		});
		State->EndDelegate = ASC->OnAbilityEnded.AddLambda([Weak](const FAbilityEndedData& D)
		{
			ABwayBarrageTransportProbe* P = Weak.Get(); if (!P || !P->State || D.AbilitySpecHandle != P->State->Handle) return;
			auto& S = *P->State; auto* A = D.AbilityThatEnded.Get(); auto* C = S.Source()->ASC.Get();
			const auto* Spec = C ? C->FindAbilitySpecFromHandle(S.Handle) : nullptr;
			const bool Exact = P->Live() && A == S.Instance.Get() && A && A->GetCurrentActivationInfo().GetActivationPredictionKey() == S.Key;
			++S.Ends;
			UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: AbilityEnded witness=%d source=%d endpoint=%s cycle=%d pawn=%s asc=%s instance=%s handle=%s key=%d exact=%d cancelled=%d activeAfter=%d movementTag=%d cooldownTag=%d shots=%d cleanup=%d elapsed=%.6f worldTime=%.6f"),
				P->Frame.Witness, P->Frame.Source, P->HasAuthority() ? TEXT("authority") : TEXT("owner"), S.Ends, *GetPathNameSafe(S.Source()->Pawn.Get()), *GetPathNameSafe(C), *GetPathNameSafe(A), *S.Handle.ToString(), A ? A->GetCurrentActivationInfo().GetActivationPredictionKey().Current : 0,
				Exact, D.bWasCancelled, Spec ? int32(Spec->IsActive()) : -1, C ? C->GetTagCount(TAG_Gameplay_MovementStopped) : -1, C ? C->GetTagCount(CooldownTag()) : -1, S.Shots.Num(), S.bCleaned, FPlatformTime::Seconds() - S.Started, P->GetWorld()->GetTimeSeconds());
			if (!S.bCleaned && (!Exact || D.bWasCancelled || !Spec || Spec->IsActive() || C->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)
				|| S.Ends != S.Activations || (P->HasAuthority() && S.Shots.Num() != P->Frame.Count * S.Ends))) P->Fail(TEXT("unexpected-ability-end"));
		});
	}
	if (HasAuthority())
	{
		State->SpawnDelegate = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([Weak](AActor* A)
		{
			ABwayBarrageTransportProbe* P = Weak.Get(); if (!P || !P->State || !A || !P->State->Source()) return;
			UClass* Bullet = FindObject<UClass>(nullptr, BulletPath);
			if (!Bullet || !A->IsA(Bullet) || A->GetOwner() != P->State->Source()->Pawn.Get()) return;
			auto& S = *P->State;
			FBwayBarrageTransportState::FShot Shot; Shot.Actor = A; Shot.Path = A->GetPathName(); Shot.Cycle = S.Activations;
			Shot.Index = S.Shots.Num() + 1; Shot.At = FPlatformTime::Seconds(); Shot.WorldAt = P->GetWorld()->GetTimeSeconds(); S.Shots.Add(Shot);
			const bool Exact = S.bFlightBoundArmed && S.FlightBoundCycle == S.Activations && P->CheckFrameQuantum() && P->VerifyRuntimeTuning() && P->SafeLane() && P->Live() && A->GetInstigator() == S.Source()->Pawn.Get() && S.Instance.IsValid()
				&& S.Instance->GetCurrentActivationInfo().GetActivationPredictionKey() == S.Key && S.Activations == S.Ends + 1;
			UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Shot witness=%d source=%d cycle=%d index=%d projectile=%s class=%s owner=%s instigator=%s instance=%s handle=%s key=%d exact=%d afterEnd=%d elapsed=%.6f worldTime=%.6f frame=%llu frameDelta=%.6f deferred=1"),
				P->Frame.Witness, P->Frame.Source, Shot.Cycle, Shot.Index, *Shot.Path, *A->GetClass()->GetPathName(), *GetPathNameSafe(A->GetOwner()), *GetPathNameSafe(A->GetInstigator()), *GetPathNameSafe(S.Instance.Get()), *S.Handle.ToString(), S.Key.Current, Exact, S.Activations <= S.Ends, Shot.At - S.Started, P->GetWorld()->GetTimeSeconds(), GFrameCounter, P->GetWorld()->GetDeltaSeconds());
			if (!Exact || S.Shots.Num() > P->Frame.Count * S.Activations) P->Fail(TEXT("unexpected-authority-shot"));
		}));
	}
#endif
}

void ABwayBarrageTransportProbe::Publish(int32 Stage)
{
	if (!State || !State->bCoordinator) return;
	if ((Frame.Stage == CastOne && Stage == Cooling) || (Frame.Stage == CastTwo && Stage == Cleanup))
	{
		// Called by AuthorityTick after its current-world guard and all four
		// settled acknowledgements. Include the first zero-bullet observation;
		// only later cooldown-idle frames are outside the projectile range bound.
		auto* Source = State->Source(); auto* ASC = Source ? Source->ASC.Get() : nullptr;
		const auto* Spec = ASC ? ASC->FindAbilitySpecFromHandle(State->Handle) : nullptr;
		const int32 Bullets = OwnedBullets(GetWorld(), Source ? Source->Pawn.Get() : nullptr);
		int32 Acknowledged = 0;
		for (const auto& W : State->Witnesses) if (W.IsValid() && State->Acks.FindRef(W->Frame.Witness) == Frame.Serial) ++Acknowledged;
		const bool Exact = Live() && State->bFlightBoundArmed && State->FlightBoundCycle == State->Activations
			&& Spec && !Spec->IsActive() && Spec->GetPrimaryInstance() == State->Instance.Get() && State->Instance.IsValid()
			&& State->Instance->GetCurrentActivationInfo().GetActivationPredictionKey() == State->Key
			&& State->Ends == State->Activations && State->Shots.Num() == Frame.Count * State->Activations
			&& Bullets == 0 && State->Witnesses.Num() == 4 && Acknowledged == 4;
		if (!Exact) { Fail(TEXT("flight-bound-retirement-precondition")); return; }
		if (!CheckFrameQuantum()) return;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: FrameBoundRetired witness=%d source=%d stage=%d nextStage=%d serial=%d cycle=%d instance=%s handle=%s key=%d exact=%d active=0 ownedBullets=%d shots=%d ends=%d witnesses=%d acknowledged=%d frame=%llu delta=%.6f limit=%.6f observedPeak=%.6f samples=%d elapsed=%.6f worldTime=%.6f reason=all-witness-cast-settled"),
			Frame.Witness, Frame.Source, Frame.Stage, Stage, Frame.Serial, State->FlightBoundCycle, *GetPathNameSafe(State->Instance.Get()), *State->Handle.ToString(), State->Key.Current, Exact, Bullets, State->Shots.Num(), State->Ends, State->Witnesses.Num(), Acknowledged, GFrameCounter, GetWorld()->GetDeltaSeconds(), Frame.FrameQuantum, State->ObservedFramePeak, State->FrameSamples, FPlatformTime::Seconds() - State->Started, GetWorld()->GetTimeSeconds());
		State->bFlightBoundArmed = false; State->LastFrameObserved = MAX_uint64;
	}
	++Frame.Serial; Frame.Stage = Stage; State->StageAt = FPlatformTime::Seconds();
	for (const auto& Weak : State->Witnesses) if (auto* W = Weak.Get())
	{
		const int32 Witness = W->Frame.Witness; W->Frame = Frame; W->Frame.Witness = Witness; W->ForceNetUpdate();
	}
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Stage source=%d serial=%d stage=%d witnesses=%d elapsed=%.6f worldTime=%.6f"), Frame.Source, Frame.Serial, Stage, State->Witnesses.Num(), FPlatformTime::Seconds() - State->Started, GetWorld()->GetTimeSeconds());
}
void ABwayBarrageTransportProbe::ServerReport_Implementation(int32 Serial, int32 Stage, bool bSuccess, const FString& Reason)
{
#if !UE_BUILD_SHIPPING
	auto* C = Coordinator.Get(); auto* PC = Cast<APlayerController>(GetOwner());
	if (!C || !C->State || !C->State->bCoordinator || !State || !PC || PC != State->LocalController.Get() || PC->PlayerState != State->LocalState.Get()
		|| !PC->PlayerState || PC->PlayerState->GetPlayerId() != Frame.Witness
		|| Serial != Frame.Serial || Stage != Frame.Stage || C->Frame.Serial != Serial || C->Frame.Stage != Stage || !C->State->Witnesses.Contains(this)) return;
	const auto* Roster = C->State->Roster.FindByPredicate([this](const FPlayer& P) { return P.Id == Frame.Witness; });
	if (!Roster || Roster->State.Get() != PC->PlayerState || !Roster->Pawn.IsValid() || Roster->Pawn->GetController() != PC || PC->GetPawn() != Roster->Pawn.Get())
	{ C->Fail(TEXT("witness-owner-identity-changed")); return; }
	if (!bSuccess) { C->Fail(*FString::Printf(TEXT("witness-%d-%s"), Frame.Witness, *Reason.Left(96))); return; }
	const int32 Previous = C->State->Acks.FindRef(Frame.Witness);
	if (Previous == Serial) return;
	if (Previous != Serial - 1) { C->Fail(TEXT("witness-ack-order")); return; }
	C->State->Acks.Add(Frame.Witness, Serial);
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Ack source=%d witness=%d serial=%d stage=%d controller=%s state=%s owned=1 elapsed=%.6f"), Frame.Source, Frame.Witness, Serial, Stage, *PC->GetPathName(), *PC->PlayerState->GetPathName(), FPlatformTime::Seconds() - C->State->Started);
#endif
}
void ABwayBarrageTransportProbe::Report(bool bSuccess, const TCHAR* Reason)
{
	if (!State || (bSuccess && State->ReportedSerial == Frame.Serial) || (!bSuccess && State->bReportedFailure)) return;
	if (bSuccess) State->ReportedSerial = Frame.Serial; else State->bReportedFailure = true;
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Report witness=%d source=%d serial=%d stage=%d success=%d reason=%s elapsed=%.6f"), Frame.Witness, Frame.Source, Frame.Serial, Frame.Stage, bSuccess, Reason, FPlatformTime::Seconds() - State->Started);
	ServerReport(Frame.Serial, Frame.Stage, bSuccess, FString(Reason));
}

bool ABwayBarrageTransportProbe::StartupIdle() const
{
	if (!State || !Live() || State->Activations || State->Ends || State->Cues || !State->Shots.IsEmpty() || State->HealthChanges) return false;
	const auto* Source = State->Source(); auto* ASC = Source->ASC.Get();
	const auto* Spec = State->bEndpoint ? ASC->FindAbilitySpecFromHandle(State->Handle) : nullptr;
	const auto* Anim = Source->Pawn->GetMesh() ? Source->Pawn->GetMesh()->GetAnimInstance() : nullptr;
	const FGameplayTagContainer CooldownQueryTags(CooldownTag());
	return (!State->bEndpoint || (Spec && !Spec->IsActive() && Spec->Ability && Spec->Ability->CheckCooldown(State->Handle, ASC->AbilityActorInfo.Get())))
		&& !ASC->HasMatchingGameplayTag(CooldownTag()) && !ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)
		&& ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(CooldownQueryTags)).IsEmpty()
		&& Anim && !Anim->Montage_IsActive(State->Montage.Get()) && SlotsSettled(Anim) && OwnedBullets(GetWorld(), Source->Pawn.Get()) == 0;
}

void ABwayBarrageTransportProbe::ObserveStartup()
{
#if !UE_BUILD_SHIPPING
	if (!State || Frame.Stage != Ready || State->bStartupHandoff || !State->Failure.IsEmpty()) return;
	const double Now = FPlatformTime::Seconds(), WorldTime = GetWorld()->GetTimeSeconds();
	const float WorldDelta = GetWorld()->GetDeltaSeconds();
	const double Gap = State->StartupLastWall < 0 ? 0 : Now - State->StartupLastWall;
	const bool Exact = Live(), Idle = Exact && StartupIdle();
	const auto Previous = State->StartupSample;
	const TCHAR* Reset = TEXT("none");
	if (!FMath::IsFinite(WorldTime) || !FMath::IsFinite(WorldDelta) || WorldDelta <= 0 || WorldDelta > Frame.FrameQuantum) Reset = TEXT("world-delta");
	else if (!FMath::IsFinite(Gap) || Gap < 0 || Gap > .3) Reset = TEXT("wall-gap");
	else if (State->StartupObservations && (GFrameCounter != Previous.EngineFrame + 1 || WorldTime <= Previous.WorldTime)) Reset = TEXT("frame-continuity");
	const bool Valid = FCString::Strcmp(Reset, TEXT("none")) == 0;
	if (!Valid) { ++State->StartupGeneration; State->StartupSample.Samples = 0; }
	if (Valid && State->StartupSample.Samples == 0)
	{
		State->StartupWindowWall = Now; State->StartupWindowWorld = WorldTime;
		State->StartupSample.PeakDelta = 0; State->StartupSample.MaxGap = 0;
	}
	auto& M = State->StartupSample;
	M.Serial = Frame.Serial; M.Generation = State->StartupGeneration; M.Observation = ++State->StartupObservations;
	M.EngineFrame = GFrameCounter; M.SourceGuid = Guid(State->Source()->Pawn.Get()); M.WorldTime = WorldTime;
	M.Elapsed = Now - State->Started; M.WorldDelta = WorldDelta;
	if (Valid)
	{
		++M.Samples; M.WallSpan = Now - State->StartupWindowWall; M.WorldSpan = WorldTime - State->StartupWindowWorld;
		M.PeakDelta = FMath::Max(M.PeakDelta, WorldDelta); M.MaxGap = FMath::Max(M.MaxGap, M.Samples > 1 ? Gap : 0.);
	}
	else { M.WallSpan = 0; M.WorldSpan = 0; M.PeakDelta = WorldDelta; M.MaxGap = Gap; }
	const bool ReadyNow = Valid && Idle && M.Samples >= 5 && M.WallSpan >= .5 && M.WorldSpan >= .5;
	const bool WasAdmitted = State->bStartupAdmitted;
	if (ReadyNow) State->bStartupAdmitted = true;
	State->StartupLastWall = Now;
	if (State->bCoordinator) { State->StartupAuthorityAt = Now; State->StartupAuthorityWorld = WorldTime; State->StartupAuthorityFrame = GFrameCounter; }
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: StartupFrame witness=%d source=%d serial=%d generation=%d observation=%d frame=%llu worldTime=%.6f worldDelta=%.6f elapsed=%.6f wallGap=%.6f windowSamples=%d wallSpan=%.6f worldSpan=%.6f peakDelta=%.6f maxGap=%.6f exact=%d idle=%d ready=%d admitted=%d resetReason=%s tickInterval=%.6f sourceGuid=%llu"),
		Frame.Witness, Frame.Source, M.Serial, M.Generation, M.Observation, M.EngineFrame, M.WorldTime, M.WorldDelta, M.Elapsed, Gap, M.Samples, M.WallSpan, M.WorldSpan, M.PeakDelta, M.MaxGap, Exact, Idle, ReadyNow, State->bStartupAdmitted, Reset, PrimaryActorTick.TickInterval, M.SourceGuid);
	if (!Idle) { Fail(TEXT("startup-preparation-not-exactly-idle")); return; }
	if (!Valid && WasAdmitted) { Fail(*FString::Printf(TEXT("startup-stability-lost-%s"), Reset)); return; }
	if (Now - State->Started > 15.) { Fail(TEXT("startup-stability-timeout")); return; }
	if (ReadyNow && (State->StartupLastSend < 0 || Now - State->StartupLastSend >= .1))
	{
		M.Sequence = ++State->StartupHeartbeats; State->StartupLastSend = Now;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: StartupReadySend witness=%d source=%d serial=%d generation=%d sequence=%d observation=%d frame=%llu sourceGuid=%llu worldTime=%.6f worldDelta=%.6f elapsed=%.6f windowSamples=%d wallSpan=%.6f worldSpan=%.6f peakDelta=%.6f maxGap=%.6f"),
			Frame.Witness, Frame.Source, M.Serial, M.Generation, M.Sequence, M.Observation, M.EngineFrame, M.SourceGuid, M.WorldTime, M.WorldDelta, M.Elapsed, M.Samples, M.WallSpan, M.WorldSpan, M.PeakDelta, M.MaxGap);
		ServerStartupReady(M);
		if (State->Failure.IsEmpty() && Frame.Stage == Ready) Report(true, TEXT("local-ready"));
	}
#endif
}

void ABwayBarrageTransportProbe::ServerStartupReady_Implementation(const FBwayBarrageStartupReady& M)
{
#if !UE_BUILD_SHIPPING
	auto* C = Coordinator.Get(); auto* PC = Cast<APlayerController>(GetOwner());
	if (!C || !C->State || !State || !C->State->bCoordinator) return;
	const auto* Roster = C->State->Roster.FindByPredicate([this](const FPlayer& P) { return P.Id == Frame.Witness; });
	const bool Owned = PC && PC == State->LocalController.Get() && PC->PlayerState == State->LocalState.Get() && Roster
		&& Roster->State.Get() == PC->PlayerState && Roster->Pawn.IsValid() && Roster->Pawn->GetController() == PC && PC->GetPawn() == Roster->Pawn.Get()
		&& C->State->Witnesses.Contains(this) && PC->PlayerState && PC->PlayerState->GetPlayerId() == Frame.Witness;
	const bool InStage = Frame.Stage == Ready && C->Frame.Stage == Ready && M.Serial == Frame.Serial && M.Serial == C->Frame.Serial;
	const auto& Old = State->StartupReceived;
	const bool Finite = FMath::IsFinite(M.WorldTime) && FMath::IsFinite(M.WorldDelta) && FMath::IsFinite(M.Elapsed)
		&& FMath::IsFinite(M.WallSpan) && FMath::IsFinite(M.WorldSpan) && FMath::IsFinite(M.PeakDelta) && FMath::IsFinite(M.MaxGap);
	const bool Window = Finite && M.Serial == 1 && M.Generation >= 1 && M.Sequence == Old.Sequence + 1 && M.Samples >= 5
		&& M.Observation >= M.Samples && M.EngineFrame > 0 && M.WorldTime >= M.WorldSpan && M.Elapsed >= M.WallSpan
		&& M.WallSpan >= .5 && M.WorldSpan >= .5 && M.WorldDelta > 0 && M.WorldDelta <= Frame.FrameQuantum
		&& M.PeakDelta >= M.WorldDelta && M.PeakDelta <= Frame.FrameQuantum && M.MaxGap >= 0 && M.MaxGap <= .3
		&& M.SourceGuid != 0 && C->State->Source() && M.SourceGuid == Guid(C->State->Source()->Pawn.Get());
	const bool Monotone = !Old.Sequence || (M.Generation == Old.Generation && M.Observation > Old.Observation && M.EngineFrame > Old.EngineFrame
		&& M.WorldTime > Old.WorldTime && M.Elapsed - Old.Elapsed >= .1 - 1.e-6 && M.Samples > Old.Samples
		&& M.WallSpan >= Old.WallSpan && M.WorldSpan >= Old.WorldSpan && M.PeakDelta >= Old.PeakDelta && M.MaxGap >= Old.MaxGap
		&& FMath::Abs((M.Elapsed - M.WallSpan) - (Old.Elapsed - Old.WallSpan)) <= 1.e-6
		&& FMath::Abs((M.WorldTime - M.WorldSpan) - (Old.WorldTime - Old.WorldSpan)) <= 1.e-6);
	const bool Accepted = Owned && InStage && Window && Monotone;
	const TCHAR* Reason = !Owned ? TEXT("identity") : !InStage ? TEXT("stage-ended") : !Window ? TEXT("window") : !Monotone ? TEXT("sequence") : TEXT("accepted");
	const double Now = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: StartupReadyReceive witness=%d source=%d serial=%d generation=%d sequence=%d observation=%d frame=%llu sourceGuid=%llu worldTime=%.6f worldDelta=%.6f elapsed=%.6f windowSamples=%d wallSpan=%.6f worldSpan=%.6f peakDelta=%.6f maxGap=%.6f controller=%s state=%s authorityElapsed=%.6f accepted=%d reason=%s"),
		Frame.Witness, Frame.Source, M.Serial, M.Generation, M.Sequence, M.Observation, M.EngineFrame, M.SourceGuid, M.WorldTime, M.WorldDelta, M.Elapsed, M.Samples, M.WallSpan, M.WorldSpan, M.PeakDelta, M.MaxGap,
		*GetPathNameSafe(PC), *GetPathNameSafe(PC ? PC->PlayerState.Get() : nullptr), Now - C->State->Started, Accepted, Reason);
	if (Accepted) { State->StartupReceived = M; State->StartupReceivedAt = Now; }
	else if (!Owned || InStage) C->Fail(*FString::Printf(TEXT("startup-readiness-rpc-%s"), Reason));
#endif
}

void ABwayBarrageTransportProbe::RestoreStartupTick(const TCHAR* Reason)
{
	if (!State || !State->bStartupTickOverride) return;
	SetActorTickInterval(State->StartupOriginalTickInterval); State->bStartupTickOverride = false;
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: StartupTick witness=%d source=%d enabled=0 original=%.6f current=%.6f observations=%d elapsed=%.6f reason=%s"),
		Frame.Witness, Frame.Source, State->StartupOriginalTickInterval, PrimaryActorTick.TickInterval, State->StartupObservations, FPlatformTime::Seconds() - State->Started, Reason);
}

bool ABwayBarrageTransportProbe::HandoffStartup(const TCHAR* Reason, bool bRequireIdle)
{
	if (!State || State->bStartupHandoff) return State && State->Failure.IsEmpty();
	const double Now = FPlatformTime::Seconds(), WorldTime = GetWorld()->GetTimeSeconds(), Age = Now - State->StartupLastWall;
	const float Delta = GetWorld()->GetDeltaSeconds();
	const auto& M = State->StartupSample;
	const bool Exact = Live(), Idle = !bRequireIdle || StartupIdle();
	const bool Valid = State->bStartupAdmitted && Exact && Idle && State->StartupHeartbeats > 0 && State->ReportedSerial >= 1
		&& State->Failure.IsEmpty() && FMath::IsFinite(Age) && Age >= 0 && Age <= .3 && FMath::IsFinite(Delta) && Delta > 0 && Delta <= Frame.FrameQuantum
		&& FMath::IsFinite(WorldTime) && WorldTime >= M.WorldTime && (GFrameCounter == M.EngineFrame || GFrameCounter == M.EngineFrame + 1)
		&& M.Samples >= 5 && M.WallSpan >= .5 && M.WorldSpan >= .5;
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: StartupHandoff witness=%d source=%d serial=%d reason=%s admitted=%d exact=%d idleRequired=%d idle=%d frame=%llu worldTime=%.6f worldDelta=%.6f elapsed=%.6f lastObservation=%d observationAge=%.6f windowSamples=%d wallSpan=%.6f worldSpan=%.6f valid=%d"),
		Frame.Witness, Frame.Source, Frame.Serial, Reason, State->bStartupAdmitted, Exact, bRequireIdle, Idle, GFrameCounter, WorldTime, Delta, Now - State->Started, M.Observation, Age, M.Samples, M.WallSpan, M.WorldSpan, Valid);
	if (!Valid) { Fail(TEXT("startup-handoff-not-fresh")); return false; }
	State->bStartupHandoff = true; RestoreStartupTick(Reason); return true;
}

bool ABwayBarrageTransportProbe::CheckStartupActivation(bool bObservedTick)
{
	if (!State) return false;
	const double Now = FPlatformTime::Seconds(), WorldTime = GetWorld()->GetTimeSeconds(), Age = Now - State->StartupAuthorityAt;
	const float Delta = GetWorld()->GetDeltaSeconds();
	const bool Exact = Live();
	const bool Valid = Exact && State->bCoordinator && Frame.Stage == CastOne && State->bStartupAdmitted && State->bStartupHandoff
		&& State->Failure.IsEmpty() && State->StartupAuthorityAt >= 0 && FMath::IsFinite(Age) && Age >= 0 && Age <= .3
		&& FMath::IsFinite(Delta) && Delta > 0 && Delta <= Frame.FrameQuantum && FMath::IsFinite(WorldTime)
		&& (bObservedTick ? WorldTime > State->StartupAuthorityWorld : WorldTime >= State->StartupAuthorityWorld)
		&& (bObservedTick ? GFrameCounter > State->StartupAuthorityFrame : GFrameCounter >= State->StartupAuthorityFrame)
		&& IsActorTickEnabled() && GetActorTickInterval() <= .05f;
	const int32 Observation = State->StartupPendingObservations + (bObservedTick ? 1 : 0);
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: %s witness=%d source=%d serial=%d observation=%d lastReadyObservation=%d previousFrame=%llu previousWorldTime=%.6f previousElapsed=%.6f frame=%llu worldTime=%.6f worldDelta=%.6f elapsed=%.6f observationAge=%.6f tickInterval=%.6f exact=%d admitted=%d handoff=%d valid=%d"),
		bObservedTick ? TEXT("StartupPendingFrame") : TEXT("StartupActivationCheck"), Frame.Witness, Frame.Source, Frame.Serial, Observation, State->StartupObservations, State->StartupAuthorityFrame, State->StartupAuthorityWorld, State->StartupAuthorityAt - State->Started,
		GFrameCounter, WorldTime, Delta, Now - State->Started, Age, GetActorTickInterval(), Exact, State->bStartupAdmitted, State->bStartupHandoff, Valid);
	// Only an actual actor Tick advances this observation cache. The activation RPC
	// validates its current world phase against that cache without refreshing it.
	if (bObservedTick)
	{
		++State->StartupPendingObservations; State->StartupAuthorityAt = Now; State->StartupAuthorityWorld = WorldTime; State->StartupAuthorityFrame = GFrameCounter;
	}
	if (!Valid) { Fail(TEXT("startup-authority-activation-not-fresh")); return false; }
	return true;
}
void ABwayBarrageTransportProbe::Fail(const TCHAR* Reason)
{
	if (!State || !State->Failure.IsEmpty()) return;
	State->Failure = Reason; State->FailAt = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Error, TEXT("BwayBarrageTransport: FAIL witness=%d source=%d serial=%d stage=%d reason=%s elapsed=%.6f"), Frame.Witness, Frame.Source, Frame.Serial, Frame.Stage, Reason, State->FailAt - State->Started);
	if (State->bCoordinator) Publish(Failed); else Report(false, Reason);
}

void ABwayBarrageTransportProbe::Sample(const TCHAR* Phase)
{
	const uint64 ClockBegin = FPlatformTime::Cycles64();
	const auto* Source = State->Source(); if (!Source || !Source->ASC.IsValid() || !Source->Pawn.IsValid()) return;
	auto* ASC = Source->ASC.Get(); const auto* Spec = State->bEndpoint ? ASC->FindAbilitySpecFromHandle(State->Handle) : nullptr;
	float Remaining = 0, Duration = 0;
	if (Spec && Spec->Ability) Spec->Ability->GetCooldownTimeRemainingAndDuration(State->Handle, ASC->AbilityActorInfo.Get(), Remaining, Duration);
	auto* Anim = Source->Pawn->GetMesh() ? Source->Pawn->GetMesh()->GetAnimInstance() : nullptr;
	const bool MontageActive = Anim && State->Montage.IsValid() && Anim->Montage_IsActive(State->Montage.Get());
	const int32 Active = Spec ? int32(Spec->IsActive()) : -1;
	const int32 MovementCount = ASC->GetTagCount(TAG_Gameplay_MovementStopped), CooldownCount = ASC->GetTagCount(CooldownTag());
	const int32 CooldownReady = Spec && Spec->Ability ? int32(Spec->Ability->CheckCooldown(State->Handle, ASC->AbilityActorInfo.Get())) : -1;
	const uint64 ClockEnd = FPlatformTime::Cycles64(); // Bounds the GAS cooldown/spec reads, not log serialization.
	++State->StateObservations;
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: State witness=%d source=%d serial=%d stage=%d phase=%s endpoint=%s pawn=%s guid=%llu elapsed=%.6f worldTime=%.6f active=%d movementTag=%d cooldownTag=%d remaining=%.6f duration=%.6f cooldownReady=%d cues=%d activations=%d ends=%d shots=%d montageActive=%d upperGlobal=%.6f fullGlobal=%.6f ownedBullets=%d health=%.6f healthChanges=%d sameLifetime=%d observation=%d frame=%llu worldDelta=%.6f clockBegin=%llu clockEnd=%llu"),
		Frame.Witness, Frame.Source, Frame.Serial, Frame.Stage, Phase, HasAuthority() ? TEXT("authority") : State->bOwner ? TEXT("owner") : TEXT("proxy"), *Source->Pawn->GetPathName(), Guid(Source->Pawn.Get()), FPlatformTime::Seconds() - State->Started, GetWorld()->GetTimeSeconds(), Active,
		MovementCount, CooldownCount, Remaining, Duration, CooldownReady,
		State->Cues, State->Activations, State->Ends, State->Shots.Num(), MontageActive, Anim ? Anim->GetSlotMontageGlobalWeight(TEXT("UpperBody")) : -1.f, Anim ? Anim->GetSlotMontageGlobalWeight(TEXT("FullBody")) : -1.f, OwnedBullets(GetWorld(), Source->Pawn.Get()), Health(*Source), State->HealthChanges, Live(), State->StateObservations, GFrameCounter, GetWorld()->GetDeltaSeconds(), ClockBegin, ClockEnd);
	if (Spec)
	{
		FGameplayTagContainer CooldownQueryTags(CooldownTag());
		for (const auto H : ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(CooldownQueryTags)))
			if (const auto* E = ASC->GetActiveGameplayEffect(H))
			{
				const uint64 EffectClockBegin = FPlatformTime::Cycles64();
				const FString Definition = GetPathNameSafe(E->Spec.Def.Get());
				const float Start = E->StartWorldTime, ServerStart = E->StartServerWorldTime;
				const float EffectDuration = E->GetDuration(), EffectRemaining = E->GetTimeRemaining(GetWorld()->GetTimeSeconds());
				const int32 PredictionKey = E->PredictionKey.Current; const bool Replicated = E->bPostPredictObject;
				const uint64 EffectClockEnd = FPlatformTime::Cycles64();
				UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Effect witness=%d source=%d stage=%d endpoint=%s handle=%s definition=%s start=%.6f serverStart=%.6f duration=%.6f remaining=%.6f predictionKey=%d elapsed=%.6f worldTime=%.6f postPredictObject=%d clockBegin=%llu clockEnd=%llu"),
					Frame.Witness, Frame.Source, Frame.Stage, HasAuthority() ? TEXT("authority") : TEXT("owner"), *H.ToString(), *Definition, Start, ServerStart, EffectDuration, EffectRemaining, PredictionKey, FPlatformTime::Seconds() - State->Started, GetWorld()->GetTimeSeconds(), Replicated, EffectClockBegin, EffectClockEnd);
			}
	}
}

void ABwayBarrageTransportProbe::Observe()
{
#if !UE_BUILD_SHIPPING
	if (!State->bLocalReady)
	{
		if (Frame.Stage == Ready) SetupLocal();
		return;
	}
	if (Frame.Stage == Failed || !State->Failure.IsEmpty())
	{
		RemoveObservers(true);
		if (!State->bComplete)
		{
			State->bComplete = true;
			UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Complete witness=%d source=%d pass=0 cleanup=%d delegatesRemoved=%d reason=%s startupObservations=%d startupHeartbeats=%d startupAdmitted=%d startupHandoff=%d tickRestored=%d startupPendingObservations=%d"), Frame.Witness, Frame.Source, State->bCleaned, State->Removed, State->Failure.IsEmpty() ? TEXT("authority-failure") : *State->Failure,
				State->StartupObservations, State->StartupHeartbeats, State->bStartupAdmitted, State->bStartupHandoff, !State->bStartupTickOverride && FMath::IsNearlyEqual(PrimaryActorTick.TickInterval, State->StartupOriginalTickInterval), State->StartupPendingObservations);
		}
		return;
	}
	if (!Live()) { Fail(TEXT("captured-lifetime-health-or-round-changed")); return; }
	const double Now = FPlatformTime::Seconds();
	auto* Source = State->Source(); auto* ASC = Source->ASC.Get();
	auto* Spec = State->bEndpoint ? ASC->FindAbilitySpecFromHandle(State->Handle) : nullptr;
	auto* Anim = Source->Pawn->GetMesh()->GetAnimInstance();
	const bool MontageActive = Anim && Anim->Montage_IsActive(State->Montage.Get());
	const bool CoolingNow = ASC->HasMatchingGameplayTag(CooldownTag());
	State->bSawCooldown |= CoolingNow;
	bool bSampled = false;
	if (Now - State->LastSample >= .045 && Frame.Stage < Complete)
	{
		State->LastSample = Now; Sample(TEXT("observe"));
		bSampled = true;
	}
	if (Frame.Stage == Ready)
	{
		ObserveStartup();
		if (!State->Failure.IsEmpty()) return;
	}
	else if (Frame.Stage == CastOne && !State->bOwner && !HandoffStartup(TEXT("cast-stage"), false)) return;
	for (auto& Shot : State->Shots) if (Shot.bFinalized && !Shot.bMotionObserved && GetWorld()->GetTimeSeconds() > Shot.WorldAt)
	{
		Shot.bMotionObserved = true;
		const AActor* A = Shot.Actor.Get(); const auto* Move = A ? A->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
		const FVector V = Move ? Move->Velocity : FVector::ZeroVector;
		const FVector Position = A ? A->GetActorLocation() : Shot.Start;
		const FVector Travel = Position - Shot.Start;
		const double Along = FVector::DotProduct(Position - Shot.Origin, Shot.Direction);
		const double Lateral = (Position - Shot.Origin - Shot.Direction * Along).Size();
		const double WorldAge = GetWorld()->GetTimeSeconds() - Shot.WorldAt;
		const double Range = Frame.Offset + Frame.Speed * (Frame.Life + 2 * Frame.FrameQuantum);
		const bool Matches = !A || (Move && !Position.ContainsNaN() && !V.ContainsNaN()
			&& FVector::Distance(V, Shot.Direction * Frame.Speed) <= .01 && Lateral <= .1 && Along >= Frame.Offset - .1 && Along <= Range + .1
			&& Travel.Size() <= Frame.Speed * (WorldAge + 2 * Frame.FrameQuantum) + .1);
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: ShotMotion witness=%d source=%d cycle=%d index=%d projectile=%s present=%d movement=%d matched=%d vx=%.6f vy=%.6f vz=%.6f speed=%.6f x=%.6f y=%.6f z=%.6f travel=%.6f along=%.6f lateral=%.6f range=%.6f elapsed=%.6f age=%.6f worldTime=%.6f worldAge=%.6f frame=%llu"),
			Frame.Witness, Frame.Source, Shot.Cycle, Shot.Index, *Shot.Path, A != nullptr, Move != nullptr, Matches, V.X, V.Y, V.Z, V.Size(), Position.X, Position.Y, Position.Z, Travel.Size(), Along, Lateral, Range, Now - State->Started, Now - Shot.At, GetWorld()->GetTimeSeconds(), WorldAge, GFrameCounter);
		// A wall collision may legitimately destroy a bullet before the next observation.
		if (!Matches) { Fail(TEXT("observed-projectile-motion-outside-lane-contract")); return; }
	}
	if (State->SeenSerial != Frame.Serial)
	{
		State->SeenSerial = Frame.Serial;
		State->CastSettledSamples = 0;
		State->CastSettlementStarted = State->CastSettlementWorld = -1;
		State->CastSettlementFrame = MAX_uint64;
		if (State->bOwner && (Frame.Stage == CastOne || Frame.Stage == CastTwo))
		{
			const int32 Cycle = Frame.Stage == CastOne ? 1 : 2;
			const bool Before = Spec && !Spec->IsActive() && !CoolingNow && Spec->Ability->CheckCooldown(State->Handle, ASC->AbilityActorInfo.Get()) && SafeLane();
			if (Cycle == 1 && !HandoffStartup(TEXT("owner-dispatch"), true)) return;
			const bool Accepted = Before && !GAllowActorScriptExecutionInEditor && ASC->TryActivateAbility(State->Handle, true);
			const bool TuningVerified = Accepted && VerifyRuntimeTuning();
			UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Dispatch witness=%d source=%d cycle=%d serial=%d pawn=%s asc=%s handle=%s readyBefore=%d accepted=%d activations=%d tuningVerified=%d elapsed=%.6f readbackElapsed=%.6f worldTime=%.6f guard=%d"),
				Frame.Witness, Frame.Source, Cycle, Frame.Serial, *Source->Pawn->GetPathName(), *ASC->GetPathName(), *State->Handle.ToString(), Before, Accepted, State->Activations, TuningVerified, Now - State->Started, FPlatformTime::Seconds() - State->Started, GetWorld()->GetTimeSeconds(), GAllowActorScriptExecutionInEditor);
			if (!Accepted || State->Activations != Cycle) { Fail(TEXT("owner-cast-rejected")); return; }
			if (!TuningVerified) return;
			return; // GAS callbacks can synchronously change the state; re-resolve next tick.
		}
	}
	if (State->bChangingAim && (Frame.Stage == CastOne || Frame.Stage == CastTwo) && bSampled && !ObserveChangingAim(true)) return;
	if (Frame.Stage == CastOne || Frame.Stage == CastTwo)
	{
		const int32 Cycle = Frame.Stage == CastOne ? 1 : 2;
		const bool EndpointEnded = !State->bEndpoint || (Spec && !Spec->IsActive() && State->Ends == Cycle && State->Activations == Cycle && State->TunedActivation == Cycle);
		const bool Settled = State->Cues == Frame.Count * Cycle && EndpointEnded && !MontageActive && SlotsSettled(Anim) && OwnedBullets(GetWorld(), Source->Pawn.Get()) == 0 && !ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped)
			&& CoolingNow && Now - State->LastCueAt > FMath::Max(State->Montage->GetPlayLength(), Frame.Life) + .2 + 2. * Frame.Lag / 1000.;
		if (State->ReportedSerial != Frame.Serial)
		{
			if (State->CastSettlementStarted >= 0 && Now - State->CastSettlementStarted > 1.)
			{ Fail(TEXT("cast-settled-observation-timeout")); return; }
			if (!Settled)
			{
				State->CastSettledSamples = 0;
				State->CastSettlementWorld = -1;
				State->CastSettlementFrame = MAX_uint64;
			}
			else
			{
				if (State->CastSettlementStarted < 0) State->CastSettlementStarted = Now;
				// Acknowledgement must follow two ordinary State samples that actually
				// observed settlement. Later cleanup samples cannot prove this boundary.
				if (bSampled)
				{
					const double WorldTime = GetWorld()->GetTimeSeconds();
					if (!FMath::IsFinite(WorldTime) || (State->CastSettledSamples > 0
						&& (GFrameCounter <= State->CastSettlementFrame || WorldTime <= State->CastSettlementWorld)))
					{ Fail(TEXT("cast-settled-observation-did-not-advance")); return; }
					State->CastSettlementFrame = GFrameCounter;
					State->CastSettlementWorld = WorldTime;
					++State->CastSettledSamples;
					UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: CastSettledSample witness=%d source=%d serial=%d stage=%d cycle=%d sample=%d observation=%d frame=%llu worldTime=%.6f elapsed=%.6f"),
						Frame.Witness, Frame.Source, Frame.Serial, Frame.Stage, Cycle, State->CastSettledSamples, State->StateObservations, GFrameCounter, WorldTime, Now - State->Started);
				}
				if (State->CastSettledSamples >= 2)
				{
					if (State->bChangingAim && State->bOwner && Cycle == 1 && (!State->bAimInputQueued || !State->bAimInputConsumed))
					{ Fail(TEXT("changing-aim-local-input-not-consumed-before-cast-settlement")); return; }
					Report(true, TEXT("cast-presentation-settled"));
				}
			}
		}
	}
	else if (Frame.Stage == Cooling)
	{
		if (State->bOwner && !State->bEarlyRejected)
		{
			float BeforeRemaining = 0, BeforeDuration = 0;
			Spec->Ability->GetCooldownTimeRemainingAndDuration(State->Handle, ASC->AbilityActorInfo.Get(), BeforeRemaining, BeforeDuration);
			const int32 BeforeActivations = State->Activations;
			const bool Rejected = CoolingNow && !Spec->IsActive() && !Spec->Ability->CheckCooldown(State->Handle, ASC->AbilityActorInfo.Get())
				&& !GAllowActorScriptExecutionInEditor && !ASC->TryActivateAbility(State->Handle, true);
			UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: EarlyReject witness=%d source=%d handle=%s rejected=%d remaining=%.6f duration=%.6f activationsBefore=%d activationsAfter=%d elapsed=%.6f worldTime=%.6f guard=%d"),
				Frame.Witness, Frame.Source, *State->Handle.ToString(), Rejected, BeforeRemaining, BeforeDuration, BeforeActivations, State->Activations, Now - State->Started, GetWorld()->GetTimeSeconds(), GAllowActorScriptExecutionInEditor);
			if (!Rejected || State->Activations != BeforeActivations) { Fail(TEXT("early-cooldown-rejection-failed")); return; }
			State->bEarlyRejected = true; return;
		}
		const bool EndpointReady = !State->bEndpoint || (Spec && !Spec->IsActive() && Spec->Ability->CheckCooldown(State->Handle, ASC->AbilityActorInfo.Get()));
		if (State->bSawCooldown && !CoolingNow && EndpointReady && (!State->bOwner || State->bEarlyRejected)
			&& (!HasAuthority() || State->bNaturalExpiry)) Report(true, TEXT("natural-cooldown-ready"));
	}
	else if (Frame.Stage == Cleanup)
	{
		if (State->CleanupSettlementStarted < 0) State->CleanupSettlementStarted = Now;
		if (State->CleanupSettledSamples < 2 && Now - State->CleanupSettlementStarted > 1.)
		{ Fail(TEXT("cleanup-settled-observation-timeout")); return; }
		if (MontageActive || !SlotsSettled(Anim) || OwnedBullets(GetWorld(), Source->Pawn.Get()) != 0 || ASC->HasMatchingGameplayTag(TAG_Gameplay_MovementStopped) || (Spec && Spec->IsActive()) || !CoolingNow || State->Cues != Frame.Count * 2)
		{ Fail(TEXT("cleanup-endpoint-not-settled")); return; }
		// Each witness must preserve real post-cast evidence before removing its observers.
		// A fast cleanup-stage delivery can precede the next .045-second State sample.
		if (bSampled && State->CleanupSettledSamples < 2)
		{
			const double WorldTime = GetWorld()->GetTimeSeconds();
			if (!FMath::IsFinite(WorldTime) || (State->CleanupSettledSamples > 0
				&& (GFrameCounter <= State->CleanupSettlementFrame || WorldTime <= State->CleanupSettlementWorld)))
			{ Fail(TEXT("cleanup-settled-observation-did-not-advance")); return; }
			State->CleanupSettlementFrame = GFrameCounter;
			State->CleanupSettlementWorld = WorldTime;
			++State->CleanupSettledSamples;
			UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: CleanupSettledSample witness=%d source=%d serial=%d stage=%d sample=%d observation=%d frame=%llu worldTime=%.6f elapsed=%.6f"),
				Frame.Witness, Frame.Source, Frame.Serial, Frame.Stage, State->CleanupSettledSamples, State->StateObservations, GFrameCounter, WorldTime, Now - State->Started);
		}
		if (State->CleanupSettledSamples < 2) return;
		RemoveObservers(false); Report(true, TEXT("local-observers-removed"));
	}
	else if (Frame.Stage == Complete && !State->bComplete)
	{
		State->bComplete = true;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Complete witness=%d source=%d pass=1 cleanup=%d delegatesRemoved=%d cues=%d activations=%d ends=%d shots=%d healthChanges=%d cooldownTag=%d montageActive=%d sameLifetime=%d elapsed=%.6f observations=%d startupObservations=%d startupHeartbeats=%d startupAdmitted=%d startupHandoff=%d tickRestored=%d startupPendingObservations=%d"),
			Frame.Witness, Frame.Source, State->bCleaned, State->Removed, State->Cues, State->Activations, State->Ends, State->Shots.Num(), State->HealthChanges, ASC->GetTagCount(CooldownTag()), MontageActive, Live(), Now - State->Started, State->StateObservations, State->StartupObservations, State->StartupHeartbeats, State->bStartupAdmitted, State->bStartupHandoff, !State->bStartupTickOverride && FMath::IsNearlyEqual(PrimaryActorTick.TickInterval, State->StartupOriginalTickInterval), State->StartupPendingObservations);
		Report(true, TEXT("complete-observed"));
	}
#endif
}

void ABwayBarrageTransportProbe::AuthorityTick()
{
#if !UE_BUILD_SHIPPING
	if (!State->bCoordinator) return;
	const double Now = FPlatformTime::Seconds();
	if (Frame.Stage == Failed)
	{
		if (Now - State->StageAt > 2. + 2. * Frame.Lag / 1000.)
		{
			const auto Witnesses = State->Witnesses;
			for (const auto& W : Witnesses) if (W.IsValid() && W.Get() != this) W->Destroy();
			Destroy();
		}
		return;
	}
	if (Now - State->Started > 90.) { Fail(TEXT("gameplay-timeout")); return; }
	if (Frame.Stage == Ready && Now - State->StageAt > 15.)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: StartupTimeout source=%d serial=%d stageAge=%.6f observations=%d windowSamples=%d wallSpan=%.6f worldSpan=%.6f peakDelta=%.6f maxGap=%.6f admitted=%d reason=all-peer-readiness"),
			Frame.Source, Frame.Serial, Now - State->StageAt, State->StartupObservations, State->StartupSample.Samples, State->StartupSample.WallSpan, State->StartupSample.WorldSpan, State->StartupSample.PeakDelta, State->StartupSample.MaxGap, State->bStartupAdmitted);
		Fail(TEXT("startup-all-peer-readiness-timeout")); return;
	}
	if (!State->bLocalReady) return;
	if (State->Activations >= 1 && !State->FirstEffect.IsValid())
	{
		auto* ASC = State->Source()->ASC.Get(); FGameplayTagContainer CooldownQueryTags(CooldownTag());
		const auto Effects = ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(CooldownQueryTags));
		if (Effects.Num() != 1) { Fail(TEXT("authority-first-cooldown-effect-count")); return; }
		const auto* E = ASC->GetActiveGameplayEffect(Effects[0]);
		if (!E || !FMath::IsNearlyEqual(E->GetDuration(), Frame.Cooldown, .01f)) { Fail(TEXT("authority-cooldown-duration")); return; }
		State->FirstEffect = Effects[0]; State->FirstEffectStart = E->StartWorldTime; State->FirstEffectDuration = E->GetDuration();
		if (auto* Delegate = ASC->OnGameplayEffectRemoved_InfoDelegate(State->FirstEffect))
		{
			const TWeakObjectPtr<ABwayBarrageTransportProbe> Weak(this);
			State->RemovalDelegate = Delegate->AddLambda([Weak](const FGameplayEffectRemovalInfo& Info)
			{
				const uint64 EventCycles = FPlatformTime::Cycles64();
				auto* P = Weak.Get(); if (!P || !P->State || P->State->bCleaned) return;
				const auto& S = *P->State; const auto* Source = S.Source(); const auto* E = Info.ActiveEffect;
				const float Age = P->GetWorld()->GetTimeSeconds() - S.FirstEffectStart;
				const bool Exact = P->Live() && Source && E && Info.OwningASC.Get() == Source->ASC.Get()
					&& E->Handle == S.FirstEffect && E->Spec.Def && E->PredictionKey == S.Key
					&& E->StartWorldTime == S.FirstEffectStart && E->GetDuration() == S.FirstEffectDuration;
				P->State->bNaturalExpiry = Exact && !Info.bPrematureRemoval && !Info.bPredictionRejected
					&& Age >= S.FirstEffectDuration && Age - S.FirstEffectDuration <= .25f;
				UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Expired witness=%d source=%d handle=%s premature=%d predictionRejected=%d age=%.6f duration=%.6f natural=%d elapsed=%.6f worldTime=%.6f pawn=%s asc=%s definition=%s serverStart=%.6f predictionKey=%d key=%d exact=%d eventCycles=%llu"),
					P->Frame.Witness, P->Frame.Source, *S.FirstEffect.ToString(), Info.bPrematureRemoval, Info.bPredictionRejected, Age, S.FirstEffectDuration, P->State->bNaturalExpiry, FPlatformTime::Seconds() - S.Started, P->GetWorld()->GetTimeSeconds(),
					*GetPathNameSafe(Source ? Source->Pawn.Get() : nullptr), *GetPathNameSafe(Info.OwningASC.Get()), *GetPathNameSafe(E ? E->Spec.Def.Get() : nullptr), E ? E->StartServerWorldTime : -1.f, E ? E->PredictionKey.Current : 0, S.Key.Current, Exact, EventCycles);
				if (!P->State->bNaturalExpiry) P->Fail(TEXT("cooldown-removed-before-natural-expiry"));
			});
		}
		else { Fail(TEXT("missing-cooldown-removal-delegate")); return; }
	}
	if (State->FirstEffect.IsValid() && (!State->bNaturalExpiry || State->Activations == 2))
	{
		auto* ASC = State->Source()->ASC.Get(); FGameplayTagContainer CooldownQueryTags(CooldownTag());
		const auto Effects = ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(CooldownQueryTags));
		if (Effects.Num() != 1) { Fail(TEXT("authority-continuous-cooldown-effect-count")); return; }
		if (State->Activations == 2 && !State->SecondEffect.IsValid()) State->SecondEffect = Effects[0];
		const auto Expected = State->Activations == 2 ? State->SecondEffect : State->FirstEffect;
		const auto* E = ASC->GetActiveGameplayEffect(Effects[0]);
		if (!E || Effects[0] != Expected || !FMath::IsNearlyEqual(E->GetDuration(), Frame.Cooldown, .01f)
			|| (State->Activations == 1 && E->StartWorldTime != State->FirstEffectStart)
			|| (State->Activations == 2 && State->SecondEffect == State->FirstEffect))
		{ Fail(TEXT("authority-cooldown-identity-or-clock-changed")); return; }
	}
	bool AllAck = State->Witnesses.Num() == 4;
	for (const auto& Weak : State->Witnesses)
	{
		const auto* W = Weak.Get(); if (!W || State->Acks.FindRef(W->Frame.Witness) != Frame.Serial) AllAck = false;
	}
	if (!AllAck) return;
	if (Frame.Stage == Ready)
	{
		const double MaxAge = .3 + Frame.Lag / 1000.;
		for (const auto& Weak : State->Witnesses)
		{
			const auto* W = Weak.Get();
			if (!W || !W->State || !W->State->StartupReceived.Sequence || W->State->StartupReceived.Serial != Frame.Serial
				|| W->State->StartupReceivedAt < 0 || Now - W->State->StartupReceivedAt < 0 || Now - W->State->StartupReceivedAt > MaxAge)
			{
				if (Now - State->LastWait > 1.)
				{
					State->LastWait = Now;
					UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: StartupWaiting source=%d witness=%d serial=%d receiveAge=%.6f maxAge=%.6f elapsed=%.6f reason=missing-or-stale-heartbeat"),
						Frame.Source, W ? W->Frame.Witness : INDEX_NONE, Frame.Serial, W && W->State && W->State->StartupReceivedAt >= 0 ? Now - W->State->StartupReceivedAt : -1., MaxAge, Now - State->Started);
				}
				return;
			}
		}
		if (!State->bStartupAdmitted || State->StartupSample.EngineFrame != GFrameCounter || State->StartupSample.WorldTime != GetWorld()->GetTimeSeconds())
		{ Fail(TEXT("startup-authority-current-frame-missing")); return; }
		for (const auto& Weak : State->Witnesses)
		{
			const auto* W = Weak.Get(); const auto& M = W->State->StartupReceived;
			UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: StartupAdmission source=%d serial=%d witness=%d sequence=%d generation=%d observation=%d receiveElapsed=%.6f receiveAge=%.6f maxAge=%.6f frame=%llu worldTime=%.6f elapsed=%.6f"),
				Frame.Source, Frame.Serial, W->Frame.Witness, M.Sequence, M.Generation, M.Observation, W->State->StartupReceivedAt - State->Started, Now - W->State->StartupReceivedAt, MaxAge, GFrameCounter, GetWorld()->GetTimeSeconds(), Now - State->Started);
		}
		if (HandoffStartup(TEXT("authority-publish"), true)) Publish(CastOne);
	}
	else if (Frame.Stage == CastOne && State->Shots.Num() == Frame.Count && State->Ends == 1) Publish(Cooling);
	else if (Frame.Stage == Cooling && State->bNaturalExpiry) Publish(CastTwo);
	else if (Frame.Stage == CastTwo && State->Shots.Num() == Frame.Count * 2 && State->Ends == 2) Publish(Cleanup);
	else if (Frame.Stage == Cleanup) Publish(Complete);
	else if (Frame.Stage == Complete)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: AllComplete source=%d witnesses=4 shots=%d ends=%d cleanup=1"), Frame.Source, State->Shots.Num(), State->Ends);
		const auto Witnesses = State->Witnesses;
		for (const auto& W : Witnesses) if (W.IsValid() && W.Get() != this) W->Destroy();
		Destroy();
	}
#endif
}

void ABwayBarrageTransportProbe::RemoveObservers(bool bCancelCapturedActivation)
{
#if !UE_BUILD_SHIPPING
	RestoreStartupTick(TEXT("cleanup"));
	if (!State || State->bCleaned) return;
	State->bCleaned = true; // Synchronous cancellation callbacks are cleanup evidence, never a new accepted end.
	if (!State->bBound) return; // Other players' witness replicas have no local observers.
	auto* Source = State->Source(); auto* ASC = Source ? Source->ASC.Get() : nullptr;
	bool CancelDispatched = false;
	if (bCancelCapturedActivation && !GAllowActorScriptExecutionInEditor && State->bEndpoint && ASC && Source->State.IsValid() && Source->Pawn.IsValid()
		&& Source->State->GetPawn() == Source->Pawn.Get() && Source->State->GetLyraAbilitySystemComponent() == ASC
		&& ASC->GetOwnerActor() == Source->State.Get() && ASC->GetAvatarActor() == Source->Pawn.Get())
	{
		const auto* Spec = ASC->FindAbilitySpecFromHandle(State->Handle);
		if (Spec && Spec->IsActive() && Spec->GetPrimaryInstance() == State->Instance.Get() && State->Instance.IsValid()
			&& State->Instance->GetCurrentActivationInfo().GetActivationPredictionKey() == State->Key)
		{
			CancelDispatched = true;
			const auto Instance = State->Instance; const auto Handle = State->Handle; const auto Key = State->Key;
			ASC->CancelAbilitiesByFunc([Instance, Handle, Key](const ULyraGameplayAbility* A, FGameplayAbilitySpecHandle H)
			{ return A == Instance.Get() && H == Handle && A->GetCurrentActivationInfo().GetActivationPredictionKey() == Key; }, true);
		}
	}
	// Re-resolve after any synchronous GAS callbacks.
	ASC = State->Source() ? State->Source()->ASC.Get() : nullptr;
	if (State->ActivationDelegate.IsValid())
	{
		if (ASC) ASC->AbilityActivatedCallbacks.Remove(State->ActivationDelegate);
		++State->Removed; State->ActivationDelegate.Reset();
	}
	if (State->EndDelegate.IsValid())
	{
		if (ASC) ASC->OnAbilityEnded.Remove(State->EndDelegate);
		++State->Removed; State->EndDelegate.Reset();
	}
	if (State->OwnerEffectRemovalDelegate.IsValid())
	{
		if (ASC) ASC->OnAnyGameplayEffectRemovedDelegate().Remove(State->OwnerEffectRemovalDelegate);
		++State->Removed; State->OwnerEffectRemovalDelegate.Reset();
	}
	if (State->RemovalDelegate.IsValid())
	{
		if (ASC) if (auto* D = ASC->OnGameplayEffectRemoved_InfoDelegate(State->FirstEffect)) D->Remove(State->RemovalDelegate);
		++State->Removed; State->RemovalDelegate.Reset();
	}
	for (auto& P : State->Roster) if (P.HealthDelegate.IsValid())
	{
		if (P.ASC.IsValid()) P.ASC->GetGameplayAttributeValueChangeDelegate(ULyraHealthSet::GetHealthAttribute()).Remove(P.HealthDelegate);
		P.HealthDelegate.Reset(); ++State->Removed;
	}
	if (State->CueDelegate.IsValid()) { if (State->CueManager.IsValid()) State->CueManager->OnGameplayCueRouted().Remove(State->CueDelegate); State->CueDelegate.Reset(); ++State->Removed; }
	if (State->SpawnDelegate.IsValid()) { if (GetWorld()) GetWorld()->RemoveOnActorSpawnedHandler(State->SpawnDelegate); State->SpawnDelegate.Reset(); ++State->Removed; }
	if (State->bChangingAim)
	{
		const auto* PC = State->bOwner && Source ? Source->Controller.Get() : nullptr;
		const FRotator Pending = PC ? PC->RotationInput : FRotator::ZeroRotator;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: AimCleanup witness=%d source=%d endpoint=%s inputQueued=%d inputConsumed=%d inputCycles=%llu controller=%s pendingPitch=%.6f pendingYaw=%.6f pendingRoll=%.6f pendingZero=%d overrides=0 extraCallbacks=0 eventCycles=%llu"),
			Frame.Witness, Frame.Source, HasAuthority() ? TEXT("authority") : State->bOwner ? TEXT("owner") : TEXT("proxy"), State->bAimInputQueued, State->bAimInputConsumed, State->AimInputCycles, *GetPathNameSafe(PC), Pending.Pitch, Pending.Yaw, Pending.Roll, PC ? int32(Pending.IsNearlyZero(.001f)) : -1, FPlatformTime::Cycles64());
	}
	const auto* RemainingSpec = ASC && State->bEndpoint ? ASC->FindAbilitySpecFromHandle(State->Handle) : nullptr;
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Cleanup witness=%d source=%d cancelRequested=%d cancelDispatched=%d remainingActive=%d delegatesRemoved=%d remainingCallbacks=0 healthChanges=%d sameLifetime=%d ownedBullets=%d observations=%d startupObservations=%d startupHeartbeats=%d startupAdmitted=%d startupHandoff=%d tickRestored=%d startupPendingObservations=%d ownerEffectRemovals=%d ownerReplicatedRemovals=%d"),
		Frame.Witness, Frame.Source, bCancelCapturedActivation, CancelDispatched, RemainingSpec ? int32(RemainingSpec->IsActive()) : -1, State->Removed, State->HealthChanges, Live(), OwnedBullets(GetWorld(), Source ? Source->Pawn.Get() : nullptr), State->StateObservations, State->StartupObservations, State->StartupHeartbeats, State->bStartupAdmitted, State->bStartupHandoff, !State->bStartupTickOverride && FMath::IsNearlyEqual(PrimaryActorTick.TickInterval, State->StartupOriginalTickInterval), State->StartupPendingObservations, State->OwnerEffectRemovals, State->OwnerReplicatedEffectRemovals);
#endif
}
void ABwayBarrageTransportProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	if (!State) return;
	if (HasAuthority() && !Coordinator.IsValid() && Frame.Stage == Waiting)
	{
		if (!SetupAuthority() && FPlatformTime::Seconds() - State->Started > 30.) { Fail(*FString::Printf(TEXT("authority-setup-timeout-%s"), *State->WaitingFor)); Destroy(); }
		return;
	}
	CheckFrameQuantum();
	if (State->bCoordinator && State->bStartupHandoff && Frame.Stage == CastOne && State->Activations == 0 && !CheckStartupActivation(true)) return;
	auto* PC = Cast<APlayerController>(GetOwner());
	if (PC && PC->IsLocalController()) Observe();
	if (PC && PC->IsLocalController() && !State->bLocalReady && FPlatformTime::Seconds() - State->LastWait > 1.)
	{
		State->LastWait = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: WaitingReady witness=%d source=%d stage=%d reason=%s elapsed=%.6f"), Frame.Witness, Frame.Source, Frame.Stage, *State->WaitingFor, State->LastWait - State->Started);
	}
	if (State->bCoordinator) AuthorityTick();
#endif
}
void ABwayBarrageTransportProbe::EndPlay(const EEndPlayReason::Type Reason)
{
#if !UE_BUILD_SHIPPING
	const auto* PC = Cast<APlayerController>(GetOwner());
	const bool Local = State && (State->bLocalReady || (PC && PC->IsLocalController()));
	RemoveObservers(true);
	if (Local) UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Destroyed witness=%d source=%d complete=%d cleanup=%d reason=%d"), Frame.Witness, Frame.Source, State->bComplete, State->bCleaned, int32(Reason));
#endif
	Super::EndPlay(Reason);
}

#if !UE_BUILD_SHIPPING
namespace BwayBarrageTransport
{
void Start(bool bClient)
{
	const double Started = FPlatformTime::Seconds(); const TSharedRef<bool> Selected = MakeShared<bool>(false);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([bClient, Started, Selected](float)
	{
		if (FPlatformTime::Seconds() - Started > 100.) { UE_LOG(LogTemp, Error, TEXT("BwayBarrageTransport: FAIL reason=startup-timeout")); return false; }
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* W = Context.World();
			if (!W || !W->IsGameWorld() || (bClient ? W->GetNetMode() != NM_Client : W->GetNetMode() != NM_ListenServer)) continue;
			auto* GS = W->GetGameState<ABwayGameState>(); if (!GS) continue;
			if (bClient)
			{
				auto* PC = W->GetFirstPlayerController(); if (!PC || !PC->PlayerState) continue;
				UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Connected role=client player=%d world=%s"), PC->PlayerState->GetPlayerId(), *W->GetPathName());
				return false;
			}
			if (W->GetMapName().Contains(TEXT("HeroSelect_Staging")))
			{
				if (*Selected || GS->PlayerArray.Num() != 4 || !GS->HeroSelectionManager || !GS->HeroSelectionManager->IsSelectionActive()) continue;
				TArray<ABwayPlayerState*> Roster;
				for (APlayerState* PS : GS->PlayerArray) if (auto* P = Cast<ABwayPlayerState>(PS); P && !P->IsABot()) Roster.Add(P);
				if (Roster.Num() != 4) continue;
				Roster.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
				*Selected = true;
				for (int32 I = 0; I < 4; ++I)
				{
					const auto Hero = UBwayHeroRegistry::ResolveHeroIdByName(Heroes[I]);
					if (!GS->HeroSelectionManager->AssignHeroToPlayer(Roster[I], Hero, true) || Roster[I]->GetSelectedHeroId() != Hero)
					{ UE_LOG(LogTemp, Error, TEXT("BwayBarrageTransport: FAIL reason=hero-selection")); return false; }
					UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: Selected hero=%s player=%d"), Heroes[I], Roster[I]->GetPlayerId());
				}
			}
			else if (W->GetMapName().Contains(TEXT("L_BW_Dorado")))
			{
				for (TActorIterator<ABwayBarrageTransportProbe> It(W); It; ++It) return false;
				W->SpawnActor<ABwayBarrageTransportProbe>(); return false;
			}
		}
		return true;
	}), .1f);
}
FAutoConsoleCommand Host(TEXT("bway.Test.BarrageTransportHost"), TEXT("Development four-process Barrage delivery and real cooldown gate."), FConsoleCommandDelegate::CreateLambda([] { Start(false); }));
FAutoConsoleCommand Client(TEXT("bway.Test.BarrageTransportClient"), TEXT("Development Barrage transport client startup readiness."), FConsoleCommandDelegate::CreateLambda([] { Start(true); }));
}
#endif
