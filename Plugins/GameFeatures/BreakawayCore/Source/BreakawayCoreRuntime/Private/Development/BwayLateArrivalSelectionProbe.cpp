// Development transport evidence only: synthetic selection, no physical frontend claim.
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "CommonActivatableWidget.h"
#include "CoreGlobals.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Settings/LevelEditorPlaySettings.h"
#endif

#if !UE_BUILD_SHIPPING
namespace BwayLateArrival
{
// Packaged processes have one observer each; PIE's single ticker owns four. They
// retain serialized IDs, never worlds/controllers/PlayerStates across travel.
bool bRunning = false;
struct FRun
{
	int32 Role = 0; // 0 = listen host; 3 = delayed fourth connection.
	int32 LocalId = INDEX_NONE;
	int32 LateId = INDEX_NONE;
	int32 ArenaArrivals = 0;
	uint32 ArenaWorldId = 0;
	double Started = FPlatformTime::Seconds();
	double InitialLockedAt = 0;
	double HoldStarted = 0;
	double SelectedEchoAt = 0;
	double StableSince = 0;
	TMap<int32, FPrimaryAssetId> Original;
	TMap<int32, FPrimaryAssetId> Final;
	FPrimaryAssetId RequestedHero;
	FPrimaryAssetId LateInitialHero;
	bool bAdmission = false;
	bool bGracePreserved = false;
	bool bSelectionObserved = false;
	bool bSelectionSent = false;
	bool bLockSent = false;
	bool bLockedObserved = false;
	bool bWidgetObserved = false;
	bool bPIE = false;
	bool bPassed = false;
	FString LastWait;
};

bool Fail(const FRun& Run, const TCHAR* Category, const TCHAR* Reason)
{
	if (!Run.bPIE) bRunning = false;
	UE_LOG(LogTemp, Error, TEXT("BwayLateArrival: FAIL role=%d player=%d category=%s reason=%s elapsed=%.2f observerCleanup=1"),
		Run.Role, Run.LocalId, Category, Reason, FPlatformTime::Seconds() - Run.Started);
	return false;
}

bool Wait(FRun& Run, const TCHAR* Reason)
{
	Run.StableSince = 0;
	if (Run.LastWait != Reason)
	{
		Run.LastWait = Reason;
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: Wait role=%d reason=%s elapsed=%.2f"), Run.Role, Reason, FPlatformTime::Seconds() - Run.Started);
	}
	return true;
}

TArray<ABwayPlayerState*> Players(ABwayGameState* GS)
{
	TArray<ABwayPlayerState*> Result;
	for (APlayerState* Player : GS->PlayerArray)
	{
		auto* PS = Cast<ABwayPlayerState>(Player);
		if (IsValid(PS) && !PS->IsABot() && !PS->IsInactive() && !PS->IsOnlyASpectator()) Result.Add(PS);
	}
	return Result;
}

bool ValidLocked(const ABwayPlayerState* PS)
{
	return PS->IsHeroLocked() && PS->GetSelectedHeroId().IsValid() && UBwayHeroRegistry::GetHeroDataById(PS->GetSelectedHeroId());
}

ABwayPlayerState* Find(const TArray<ABwayPlayerState*>& Roster, int32 Id)
{
	for (auto* PS : Roster) if (PS->GetPlayerId() == Id) return PS;
	return nullptr;
}

bool OriginalUnchanged(const FRun& Run, const TArray<ABwayPlayerState*>& Roster)
{
	for (const auto& Entry : Run.Original)
	{
		const auto* PS = Find(Roster, Entry.Key);
		if (!PS || !ValidLocked(PS) || PS->GetSelectedHeroId() != Entry.Value) return false;
	}
	return Run.Original.Num() == 3;
}

void ObserveWidget(FRun& Run, APlayerController* PC)
{
	if (Run.bWidgetObserved) return;
	const auto* Property = FindFProperty<FObjectProperty>(PC->GetClass(), TEXT("HeroSelectionWidget"));
	const auto* Widget = Property ? Cast<UCommonActivatableWidget>(Property->GetObjectPropertyValue_InContainer(PC)) : nullptr;
	if (Widget && Widget->IsActivated() && Widget->GetOwningPlayer() == PC)
	{
		Run.bWidgetObserved = true;
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: WidgetObserved role=%d player=%d activated=1 localOwner=1 physicalInput=0"), Run.Role, Run.LocalId);
	}
}

FPrimaryAssetId ChooseHero(ABwayGameState* GS, ABwayPlayerState* PS)
{
	const int32 Team = GS->GetPlayerTeam(PS);
	if (Team < 0) return {};
	if (auto* Registry = UBwayHeroRegistry::Get(GS).Get())
	{
		TArray<FPrimaryAssetId> Available;
		for (const auto& SoftHero : Registry->GetAllHeroSoftObjects())
			if (const auto* Hero = Registry->LoadHeroSync(SoftHero))
				if (Hero->GetPrimaryAssetId() != PS->GetSelectedHeroId() && GS->HeroSelectionManager->IsHeroAvailableForTeam(Hero->GetPrimaryAssetId(), Team)) Available.Add(Hero->GetPrimaryAssetId());
		Available.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
		if (!Available.IsEmpty()) return Available[0];
	}
	return {};
}

bool ObserveStaging(FRun& Run, UWorld* World, ABwayGameState* GS, APlayerController* LocalPC)
{
	const double Now = FPlatformTime::Seconds();
	auto* Manager = GS->HeroSelectionManager.Get();
	if (!Manager || !GS->HeroSelectionPhaseComponent) return Wait(Run, TEXT("selection-components"));
	const auto Roster = Players(GS);
	if (Roster.Num() > 4) return Fail(Run, TEXT("setup"), TEXT("unexpected-roster"));
	ObserveWidget(Run, LocalPC);
	if (Run.Role == 0 && Run.Original.IsEmpty())
	{
		if (Roster.Num() != 3 || GS->PlayerArray.Num() != 3 || Manager->GetTotalPlayers() != 3 || !Manager->IsSelectionActive())
			return Wait(Run, TEXT("three-registered-humans"));
		if (Manager->GetSelectionTimeRemaining() < 20.f) return Fail(Run, TEXT("setup"), TEXT("initial-selection-window-too-short"));
		for (auto* PS : Roster)
		{
			const auto* PC = PS->GetPlayerController();
			if (!PC || (!PC->IsLocalController() && !PC->GetNetConnection())) return Wait(Run, TEXT("initial-owner-connections"));
			if (PS->IsHeroLocked() || PS->GetPawn()) return Fail(Run, TEXT("setup"), TEXT("initial-roster-already-locked-or-spawned"));
		}
		Manager->ResolveMissingSelectionsAndLockAll(GS->HeroSelectionPhaseComponent->ResolveFallbackHeroId());
		for (auto* PS : Roster)
		{
			const auto Selection = Manager->GetPlayerSelection(PS);
			if (!ValidLocked(PS) || !Selection.bIsLocked || Selection.SelectedHeroId != PS->GetSelectedHeroId())
				return Fail(Run, TEXT("setup"), TEXT("production-initial-lock-failed"));
			Run.Original.Add(PS->GetPlayerId(), PS->GetSelectedHeroId());
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: Original role=0 player=%d hero=%s locked=1"), PS->GetPlayerId(), *PS->GetSelectedHeroId().ToString());
		}
		Run.InitialLockedAt = Now;
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: Host InitialLocksReady humans=3 registered=3 originalLocks=3 graceSeconds=5 syntheticSetup=1"));
		return true;
	}
	if (Run.Original.IsEmpty())
	{
		// Replication can deliver the fourth PS before an earlier PS's lock. Wait for
		// a coherent roster; never infer a missing lock from a single partial sample.
		TMap<int32, FPrimaryAssetId> Locked;
		for (auto* PS : Roster) if (ValidLocked(PS)) Locked.Add(PS->GetPlayerId(), PS->GetSelectedHeroId());
		if (Locked.Num() != 3 || (Run.Role == 3 ? Locked.Contains(Run.LocalId) : !Locked.Contains(Run.LocalId)))
			return Wait(Run, TEXT("original-three-replicated-locks"));
		Run.Original = MoveTemp(Locked);
		Run.InitialLockedAt = Now;
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: OriginalLocksObserved role=%d locks=3"), Run.Role);
	}
	if (!OriginalUnchanged(Run, Roster)) return Fail(Run, TEXT("gameplay"), TEXT("original-selection-or-lock-changed"));
	if (Roster.Num() != 4 || Manager->GetTotalPlayers() != 4) return Wait(Run, TEXT("fourth-registration"));
	ABwayPlayerState* Late = nullptr;
	for (auto* PS : Roster) if (!Run.Original.Contains(PS->GetPlayerId())) Late = PS;
	if (!Late || (Run.Role == 3 && Late->GetPlayerId() != Run.LocalId)) return Fail(Run, TEXT("setup"), TEXT("late-owner-identity"));
	Run.LateId = Late->GetPlayerId();
	const auto Selection = Manager->GetPlayerSelection(Late);
	if (Selection.PlayerState != Late) return Wait(Run, TEXT("fourth-manager-binding"));
	if (Late->GetPawn()) return Fail(Run, TEXT("gameplay"), TEXT("fourth-spawned-in-staging"));
	if (!Run.bAdmission)
	{
		if (Run.Role == 0 && Now - Run.InitialLockedAt >= 5.0) return Fail(Run, TEXT("setup"), TEXT("fourth-missed-pending-grace-window"));
		if (Late->IsHeroLocked() || Selection.bIsLocked) return Fail(Run, TEXT("gameplay"), TEXT("fourth-locked-before-admission-observation"));
		if (!Manager->IsSelectionActive()) return Fail(Run, TEXT("gameplay"), TEXT("selection-closed-during-admission"));
		Run.bAdmission = true;
		Run.HoldStarted = Now;
		Run.LateInitialHero = Late->GetSelectedHeroId();
		if (Run.Role == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: Host AdmissionVerified player=%d originalLocks=3 pawnless=1 elapsedFromLocks=%.2f"), Run.LateId, Now - Run.InitialLockedAt);
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: AdmissionObserved role=%d player=%d originalLocks=3 pawnless=1"), Run.Role, Run.LateId);
		}
	}
	if (Run.Role == 0 && !Run.bGracePreserved && Now - Run.InitialLockedAt >= 5.5)
	{
		if (Late->IsHeroLocked() || !Manager->IsSelectionActive()) return Fail(Run, TEXT("gameplay"), TEXT("fourth-did-not-hold-past-original-grace"));
		Run.bGracePreserved = true;
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: Host GracePreserved player=%d originalLocks=3 pawnless=1 unlocked=1 elapsedFromLocks=%.2f"), Run.LateId, Now - Run.InitialLockedAt);
	}
	if (Run.Role == 3)
	{
		if (Late->GetPlayerController() != LocalPC) return Fail(Run, TEXT("setup"), TEXT("late-rpc-owner-mismatch"));
		if (!Run.bSelectionSent)
		{
			if (Late->IsHeroLocked()) return Fail(Run, TEXT("gameplay"), TEXT("fourth-auto-locked-before-rpc"));
			if (Now - Run.HoldStarted < 6.0) return true;
			Run.RequestedHero = ChooseHero(GS, Late);
			if (!Run.RequestedHero.IsValid()) return Fail(Run, TEXT("setup"), TEXT("no-available-hero"));
			Run.bSelectionSent = true;
			Late->ServerSetSelectedHeroId(Run.RequestedHero);
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: Client SelectionRequested role=3 player=%d hero=%s heldSeconds=%.2f"), Run.LateId, *Run.RequestedHero.ToString(), Now - Run.HoldStarted);
			return true;
		}
		if (!Run.bLockSent)
		{
			if (Late->IsHeroLocked()) return Fail(Run, TEXT("gameplay"), TEXT("fourth-locked-before-own-lock-rpc"));
			if (Late->GetSelectedHeroId() != Run.RequestedHero) return Wait(Run, TEXT("selection-rpc-echo"));
			if (Run.SelectedEchoAt == 0) Run.SelectedEchoAt = Now;
			// Leave a separate authority-observable selected/unlocked sample. A single
			// coalesced selected+locked update cannot stand in for selection RPC proof.
			if (Now - Run.SelectedEchoAt < 0.75) return true;
			Run.bLockSent = true;
			Late->ServerLockHeroSelection();
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: Client LockRequested role=3 player=%d hero=%s selectionEcho=1"), Run.LateId, *Run.RequestedHero.ToString());
		}
	}
	if (!Late->IsHeroLocked() && Late->GetSelectedHeroId() != Run.LateInitialHero && Late->GetSelectedHeroId().IsValid() && UBwayHeroRegistry::GetHeroDataById(Late->GetSelectedHeroId()))
	{
		if (!Run.bSelectionObserved)
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: SelectionObserved role=%d player=%d hero=%s locked=0"), Run.Role, Run.LateId, *Late->GetSelectedHeroId().ToString());
		Run.bSelectionObserved = true;
	}
	if (ValidLocked(Late) && Selection.bIsLocked && Selection.SelectedHeroId == Late->GetSelectedHeroId())
	{
		if (Run.Role == 0 && (!Run.bGracePreserved || !Run.bSelectionObserved))
			return Fail(Run, TEXT("gameplay"), TEXT("fourth-lock-without-manual-selection-evidence"));
		if (!Run.bLockedObserved)
		{
			Run.bLockedObserved = true;
			for (auto* PS : Roster) Run.Final.Add(PS->GetPlayerId(), PS->GetSelectedHeroId());
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: FourthLocked role=%d player=%d hero=%s originalLocks=3 selectionRemaining=%.2f"), Run.Role, Run.LateId, *Late->GetSelectedHeroId().ToString(), Manager->GetSelectionTimeRemaining());
		}
	}
	// Admission has its own short bound, so a 60-second fallback cannot turn this
	// intended manual-RPC test into a success. No timeout/deadline is modified.
	if (!Run.bLockedObserved && Now - Run.HoldStarted > 20.0) return Fail(Run, TEXT("gameplay"), TEXT("manual-selection-lock-timeout"));
	return true;
}

bool ObserveArena(FRun& Run, UWorld* World, ABwayGameState* GS, APlayerController* LocalPC)
{
	if (Run.ArenaWorldId != World->GetUniqueID())
	{
		Run.ArenaWorldId = World->GetUniqueID();
		++Run.ArenaArrivals;
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: ArenaArrival role=%d count=%d world=%s"), Run.Role, Run.ArenaArrivals, *World->GetPathName());
	}
	if (Run.ArenaArrivals != 1) return Fail(Run, TEXT("gameplay"), TEXT("multiple-arena-arrivals"));
	if (!Run.bAdmission || !Run.bLockedObserved || Run.Final.Num() != 4 || (Run.Role == 0 && !Run.bGracePreserved))
		return Fail(Run, TEXT("setup"), TEXT("travel-before-late-admission-and-manual-lock-proof"));
	if (!GS->GetRoundManagement() || GS->GetRoundManagement()->GetCurrentMatchPhase() != EBwayMatchPhase::Playing)
		return Wait(Run, TEXT("playing-phase"));
	const auto Roster = Players(GS);
	if (Roster.Num() != 4) { Run.StableSince = 0; return Wait(Run, TEXT("four-gameplay-playerstates")); }
	for (const auto& Entry : Run.Final)
	{
		auto* PS = Find(Roster, Entry.Key);
		auto* Pawn = PS ? Cast<ABwayCharacterWithAbilities>(PS->GetPawn()) : nullptr;
		if (!PS || !ValidLocked(PS) || PS->GetSelectedHeroId() != Entry.Value || !Pawn || Pawn->GetPlayerState() != PS || !Pawn->HeroDataAsset
			|| Pawn->HeroDataAsset->GetPrimaryAssetId() != Entry.Value)
		{ Run.StableSince = 0; return Wait(Run, TEXT("four-matching-initialized-pawns")); }
		if (Run.Role == 0 && (!PS->GetPlayerController() || PS->GetPlayerController()->GetPawn() != Pawn))
		{ Run.StableSince = 0; return Wait(Run, TEXT("four-authority-possessions")); }
	}
	if (!LocalPC->GetPawn() || LocalPC->PlayerState->GetPlayerId() != Run.LocalId || LocalPC->GetPawn()->GetPlayerState() != LocalPC->PlayerState)
	{ Run.StableSince = 0; return Wait(Run, TEXT("local-possession")); }
	if (Run.Role == 3 && (!Run.bSelectionSent || !Run.bLockSent || Run.Final.FindRef(Run.LocalId) != Run.RequestedHero))
		return Fail(Run, TEXT("gameplay"), TEXT("late-owner-final-selection-mismatch"));
	const double Now = FPlatformTime::Seconds();
	if (Run.StableSince == 0) Run.StableSince = Now;
	if (Now - Run.StableSince < 1.0) return true;
	for (const auto& Entry : Run.Final)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: PeerFinal role=%d player=%d hero=%s locked=1 pawn=1 playing=1"), Run.Role, Entry.Key, *Entry.Value.ToString());
	}
	Run.bPassed = true;
	if (!Run.bPIE) bRunning = false;
	if (Run.Role == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: Host PASS player=%d peers=4 arenaArrivals=1 originalLocks=3 admission=1 gracePreserved=1 observerCleanup=1 physicalInput=0"), Run.LocalId);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: Client PASS role=%d player=%d peers=4 arenaArrivals=1 admission=1 observerCleanup=1 widgetObserved=%d physicalInput=0"), Run.Role, Run.LocalId, Run.bWidgetObserved);
		// The observer is removed on return; this separate scalar-only exit callback
		// lets every peer finish convergence while the runner keeps the host alive.
		if (!Run.bPIE) FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Now](float)
		{
			if (FPlatformTime::Seconds() - Now < 10.0) return true;
			FPlatformMisc::RequestExit(false);
			return false;
		}), 0.25f);
	}
	return false;
}

void Start(int32 Role)
{
	if (bRunning) { UE_LOG(LogTemp, Error, TEXT("BwayLateArrival: FAIL category=setup reason=already-running")); return; }
	bRunning = true;
	const auto Run = MakeShared<FRun>();
	Run->Role = Role;
	UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: Start role=%d timeout=180 syntheticSetup=1 physicalInput=0"), Role);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float)
	{
		if (FPlatformTime::Seconds() - Run->Started > 180.0) return Fail(*Run, TEXT("setup"), TEXT("observer-timeout"));
		if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
		{
			auto* World = Context.World();
			if (Context.WorldType != EWorldType::Game || !World || World->bIsTearingDown || World->GetNetMode() != (Run->Role == 0 ? NM_ListenServer : NM_Client)) continue;
			auto* GS = World->GetGameState<ABwayGameState>();
			auto* PC = World->GetFirstPlayerController();
			if (!GS || !PC || !PC->IsLocalController() || !PC->GetPlayerState<ABwayPlayerState>()) continue;
			if (Run->LocalId == INDEX_NONE)
			{
				Run->LocalId = PC->PlayerState->GetPlayerId();
				UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: StartupConnected role=%d player=%d world=%s"), Run->Role, Run->LocalId, *World->GetPathName());
			}
			if (World->GetMapName().Contains(TEXT("HeroSelect_Staging"))) return ObserveStaging(*Run, World, GS, PC);
			if (World->GetMapName().Contains(TEXT("L_BW_Dorado"))) return ObserveArena(*Run, World, GS, PC);
		}
		Run->StableSince = 0;
		return true;
	}), 0.1f);
}

#if WITH_EDITOR
struct FPIERun
{
	FRun Peers[4];
	int32 Instances[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
	bool bMapped = false;
	bool bJoinRequested = false;
};

// All four observers share one ticker. Instance IDs survive world replacement;
// each tick resolves the local controller again and checks its durable player ID.
bool TickPIE(FPIERun& State)
{
	if (!GEditor || !GEngine) return Fail(State.Peers[0], TEXT("setup"), TEXT("pie-session-ended"));
	if (FPlatformTime::Seconds() - State.Peers[0].Started > 180.0) return Fail(State.Peers[0], TEXT("setup"), TEXT("pie-observer-timeout"));
	TArray<int32> Instances;
	for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) Instances.Add(Context.PIEInstance);
	if (Instances.IsEmpty()) return Fail(State.Peers[0], TEXT("setup"), TEXT("pie-session-ended"));
	if (Instances.Num() > (State.bJoinRequested ? 4 : 3)) return Fail(State.Peers[0], TEXT("setup"), TEXT("unexpected-pie-world-count"));
	if (!State.bMapped)
	{
		if (Instances.Num() != 3) return Wait(State.Peers[0], TEXT("three-initial-pie-worlds"));
		Instances.Sort();
		TArray<int32> Clients;
		int32 Host = INDEX_NONE;
		TSet<int32> LocalIds;
		for (const int32 Instance : Instances)
		{
			const auto* Context = GEngine->GetWorldContextFromPIEInstance(Instance);
			auto* World = Context ? Context->World() : nullptr;
			auto* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
			auto* PC = World ? World->GetFirstPlayerController() : nullptr;
			auto* PS = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
			if (!GS || !PC || !PC->IsLocalController() || !PS || PS->GetPlayerId() < 0 || !GS->HeroSelectionManager
				|| Players(GS).Num() != 3 || GS->HeroSelectionManager->GetTotalPlayers() != 3)
				return Wait(State.Peers[0], TEXT("initial-three-world-ownership-and-rosters"));
			if (!World->GetMapName().Contains(TEXT("HeroSelect_Staging"))) return Fail(State.Peers[0], TEXT("setup"), TEXT("pie-must-start-in-staging"));
			LocalIds.Add(PS->GetPlayerId());
			if (World->GetNetMode() == NM_ListenServer)
			{
				if (Host != INDEX_NONE) return Fail(State.Peers[0], TEXT("setup"), TEXT("multiple-pie-authorities"));
				Host = Instance;
			}
			else if (World->GetNetMode() == NM_Client) Clients.Add(Instance);
			else return Fail(State.Peers[0], TEXT("setup"), TEXT("pie-must-use-listen-topology"));
		}
		if (Host == INDEX_NONE || Clients.Num() != 2) return Fail(State.Peers[0], TEXT("setup"), TEXT("pie-owner-topology"));
		if (LocalIds.Num() != 3) return Wait(State.Peers[0], TEXT("initial-unique-replicated-owner-ids"));
		State.Instances[0] = Host; State.Instances[1] = Clients[0]; State.Instances[2] = Clients[1];
		State.bMapped = true;
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: PIE Mapped worlds=3 listen=1 clients=2 realNetwork=1"));
	}
	if (State.bJoinRequested && State.Instances[3] == INDEX_NONE)
	{
		for (const int32 Instance : Instances)
			if (Instance != State.Instances[0] && Instance != State.Instances[1] && Instance != State.Instances[2])
				State.Instances[3] = Instance;
	}
	for (int32 Role = 0; Role < 4; ++Role)
	{
		auto& Run = State.Peers[Role];
		if (Run.bPassed || State.Instances[Role] == INDEX_NONE) continue;
		const auto* Context = GEngine->GetWorldContextFromPIEInstance(State.Instances[Role]);
		auto* World = Context ? Context->World() : nullptr;
		if (!World || World->bIsTearingDown) { Run.StableSince = 0; continue; }
		auto* GS = World->GetGameState<ABwayGameState>();
		auto* PC = World->GetFirstPlayerController();
		auto* PS = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
		if (!GS || !PC || !PC->IsLocalController() || !PS || PS->GetPlayerId() < 0) { Run.StableSince = 0; continue; }
		if (World->GetNetMode() != (Role == 0 ? NM_ListenServer : NM_Client)) return Fail(Run, TEXT("setup"), TEXT("pie-peer-netmode-changed"));
		if (Run.LocalId == INDEX_NONE)
		{
			Run.LocalId = PS->GetPlayerId();
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: StartupConnected role=%d player=%d pieInstance=%d world=%s"), Role, Run.LocalId, State.Instances[Role], *World->GetPathName());
		}
		else if (PS->GetPlayerId() != Run.LocalId) { Wait(Run, TEXT("pie-local-player-id-settling")); continue; }
		bool bContinue = true;
		if (World->GetMapName().Contains(TEXT("HeroSelect_Staging"))) bContinue = ObserveStaging(Run, World, GS, PC);
		else if (World->GetMapName().Contains(TEXT("L_BW_Dorado"))) bContinue = ObserveArena(Run, World, GS, PC);
		else Run.StableSince = 0;
		if (!bContinue && !Run.bPassed) return false;
		if (Role == 0 && Run.InitialLockedAt != 0 && !State.bJoinRequested)
		{
			State.bJoinRequested = true;
			GEditor->RequestLateJoin();
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: PIE LateJoinRequested initialWorlds=3 API=RequestLateJoin productionGraceSeconds=5"));
		}
	}
	for (const auto& Run : State.Peers) if (!Run.bPassed) return true;
	if (Instances.Num() != 4) return Fail(State.Peers[0], TEXT("setup"), TEXT("pie-final-world-count"));
	TSet<int32> LocalIds;
	for (const auto& Run : State.Peers)
	{
		LocalIds.Add(Run.LocalId);
		if (Run.Final.Num() != 4) return Fail(Run, TEXT("gameplay"), TEXT("pie-final-roster-size"));
		for (const auto& Entry : State.Peers[0].Final)
			if (Run.Final.FindRef(Entry.Key) != Entry.Value) return Fail(Run, TEXT("gameplay"), TEXT("pie-cross-peer-hero-divergence"));
	}
	if (LocalIds.Num() != 4) return Fail(State.Peers[0], TEXT("setup"), TEXT("pie-duplicate-local-owner"));
	UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: PIE COMPLETE passed=1 peers=4 playerHeroMapsEqual=1 realLateJoin=1 observerCleanup=1 processExitRequested=0 physicalInput=0"));
	return false;
}

FAutoConsoleCommand PIECommand(TEXT("bway.Test.LateArrivalPIE"), TEXT("Development real late join: start three same-process listen PIE worlds in staging; adds fourth with RequestLateJoin and reuses production selection assertions."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (bRunning) { UE_LOG(LogTemp, Error, TEXT("BwayLateArrival: FAIL category=setup reason=already-running")); return; }
		bool bSingleProcess = false;
		GetDefault<ULevelEditorPlaySettings>()->GetRunUnderOneProcess(bSingleProcess);
		if (!GEditor || !GEditor->PlayWorld || !bSingleProcess)
		{ UE_LOG(LogTemp, Error, TEXT("BwayLateArrival: FAIL category=setup reason=requires-active-single-process-pie")); return; }
		bRunning = true;
		const auto State = MakeShared<FPIERun>();
		for (int32 Role = 0; Role < 4; ++Role) { State->Peers[Role].Role = Role; State->Peers[Role].bPIE = true; }
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: PIE Start timeout=180 initialWorlds=3 targetWorlds=4 syntheticSetup=1 physicalInput=0"));
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State](float)
		{
			if (TickPIE(*State)) return true;
			bRunning = false;
			// Returning false destroys the sole ticker's four scalar-only observer states.
			return false;
		}), 0.1f);
	}));

FAutoConsoleCommand RetryHostCommand(TEXT("bway.Test.LateArrivalRetryHost"), TEXT("Development explicit host recovery. Supply one attempt token; queues exactly one native callback so nested travel RPCs run outside Python's editor-script guard."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		bool bValidToken = Args.Num() == 1 && !Args[0].IsEmpty() && Args[0].Len() <= 64;
		if (bValidToken) for (const TCHAR Ch : Args[0])
			if (!((Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z') || (Ch >= '0' && Ch <= '9') || Ch == '-' || Ch == '_')) bValidToken = false;
		if (!bValidToken) { UE_LOG(LogTemp, Error, TEXT("BwayLateArrival: FAIL category=setup reason=native-retry-invalid-token")); return; }
		if (bRunning) { UE_LOG(LogTemp, Error, TEXT("BwayLateArrival: FAIL category=setup reason=already-running token=%s"), *Args[0]); return; }
		bRunning = true;
		const FString Token = Args[0];
		UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: RetryHost Queued token=%s"), *Token);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Token](float)
		{
			// Python InvokeFunctionCall installs FEditorScriptExecutionGuard. Under
			// that guard Actor::GetFunctionCallspace returns Local even for Client RPCs.
			// Never invoke recovery until that synchronous Python call has returned.
			const bool bScriptGuard = GAllowActorScriptExecutionInEditor;
			ABwayPlayerController* Host = nullptr;
			int32 Authorities = 0;
			FString Failure;
			if (bScriptGuard) Failure = TEXT("native-retry-script-guard-active");
			else if (!GEngine) Failure = TEXT("native-retry-no-engine");
			else for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* World = Context.World();
				if ((Context.WorldType != EWorldType::PIE && Context.WorldType != EWorldType::Game) || !World || World->bIsTearingDown || World->GetNetMode() != NM_ListenServer) continue;
				++Authorities;
				auto* PC = Cast<ABwayPlayerController>(World->GetFirstPlayerController());
				if (PC && PC->IsLocalController() && PC->GetLocalPlayer()) Host = PC;
			}
			if (Failure.IsEmpty() && (Authorities != 1 || !Host)) Failure = TEXT("native-retry-requires-one-local-listen-host");
			const bool bCanRetry = Failure.IsEmpty() && Host->CanRetryHeroSelectionTravel();
			if (Failure.IsEmpty() && !bCanRetry) Failure = TEXT("native-retry-host-not-ready");
			// Serialize diagnostic identity before the production call can start travel.
			const int32 PlayerId = Host && Host->PlayerState ? Host->PlayerState->GetPlayerId() : INDEX_NONE;
			const FString WorldPath = Host ? Host->GetWorld()->GetPathName() : TEXT("none");
			const bool bInvoked = Failure.IsEmpty();
			const bool bAccepted = bInvoked && Host->RetryHeroSelectionTravel();
			if (bInvoked && !bAccepted) Failure = TEXT("native-retry-production-rejected");
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: RetryHost Result token=%s canRetry=%d invoked=%d accepted=%d scriptGuard=%d player=%d world=%s"),
				*Token, bCanRetry, bInvoked, bAccepted, bScriptGuard, PlayerId, *WorldPath);
			if (!Failure.IsEmpty()) UE_LOG(LogTemp, Warning, TEXT("BwayLateArrival: FAIL category=%s reason=%s token=%s"), bInvoked ? TEXT("gameplay") : TEXT("setup"), *Failure, *Token);
			bRunning = false;
			UE_LOG(LogTemp, Display, TEXT("BwayLateArrival: RetryHost Cleanup token=%s observerCleanup=1"), *Token);
			return false;
		}), 0.1f);
	}));
#endif

FAutoConsoleCommand HostCommand(TEXT("bway.Test.LateArrivalHost"), TEXT("Development four-process late-arrival selection probe. Start host plus two clients; launch client role3 on Host InitialLocksReady."),
	FConsoleCommandDelegate::CreateLambda([] { Start(0); }));
FAutoConsoleCommand ClientCommand(TEXT("bway.Test.LateArrivalClient"), TEXT("Development late-arrival observer. Role 1 or 2 observes; role 3 holds unlocked then submits actual selection/lock RPCs."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() != 1 || (Args[0] != TEXT("1") && Args[0] != TEXT("2") && Args[0] != TEXT("3")))
		{ UE_LOG(LogTemp, Error, TEXT("BwayLateArrival: FAIL category=setup reason=client-role-must-be-1-2-or-3")); return; }
		Start(FCString::Atoi(*Args[0]));
	}));
}
#endif
