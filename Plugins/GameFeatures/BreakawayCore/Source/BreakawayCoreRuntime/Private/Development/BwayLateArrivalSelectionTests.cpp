#include "Development/BwayLateArrivalSelectionTestTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "BwayPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

// Narrow access to the real production callbacks and state. No duplicate policy.
struct FBwayLateArrivalSelectionTestAccess
{
	static bool Pending(const UBwayHeroSelectionPhaseComponent* Phase)
	{
		return Phase->GetWorld()->GetTimerManager().TimerExists(Phase->PendingTravelTimerHandle);
	}
	static float Remaining(const UBwayHeroSelectionPhaseComponent* Phase)
	{
		return Phase->GetWorld()->GetTimerManager().GetTimerRemaining(Phase->PendingTravelTimerHandle);
	}
	static bool RecheckPending(const UBwayHeroSelectionPhaseComponent* Phase)
	{
		return Phase->GetWorld()->GetTimerManager().TimerExists(Phase->RosterRecheckTimerHandle);
	}
	static bool Ready(const UBwayHeroSelectionPhaseComponent* Phase) { return Phase->IsCurrentRosterReadyForTravel(); }
	static bool Committed(const UBwayHeroSelectionPhaseComponent* Phase) { return Phase->bTravelCommitted; }
	static void Commit(UBwayHeroSelectionPhaseComponent* Phase) { Phase->CommitPendingTravel(); }
	static void Schedule(UBwayHeroSelectionPhaseComponent* Phase) { Phase->SchedulePostHeroSelectionTravel(); }
	static bool Prepare(ABreakawayGameMode* Mode, APlayerController* Player) { return Mode->PreparePlayerHeroSelection(Player); }
	static bool Awaiting(const ABreakawayGameMode* Mode, APlayerController* Player) { return Mode->PlayersAwaitingLateJoinHeroSelection.Contains(Player); }
	static bool SpawnBlocked(const ABreakawayGameMode* Mode, APlayerController* Player) { return Mode->ShouldDeferPlayerRestartForHeroSelection(Player); }
};

namespace BwayLateArrivalSelectionTests
{
using FAccess = FBwayLateArrivalSelectionTestAccess;

struct FFixture
{
	explicit FFixture(FAutomationTestBase* InTest) : Test(InTest) {}
	~FFixture()
	{
		if (!World) return;
		if (Phase && Phase->HasBegunPlay()) Phase->EndPlay(EEndPlayReason::EndPlayInEditor);
		if (Manager && Manager->HasBegunPlay()) Manager->EndPlay(EEndPlayReason::EndPlayInEditor);
		for (UObject* Owner : {static_cast<UObject*>(Mode), static_cast<UObject*>(Phase), static_cast<UObject*>(Manager), static_cast<UObject*>(Rounds), static_cast<UObject*>(GS)})
			if (Owner) World->GetTimerManager().ClearAllTimersForObject(Owner);
		World->NextURL.Reset();
		World->DestroyWorld(false);
		if (Instance) Instance->Shutdown();
		GEngine->DestroyWorldContext(World);
	}

	bool Setup(bool bStaging, float SelectionSeconds = 60.f)
	{
		if (!Test->TestNotNull(TEXT("Engine owns the fixture world context"), GEngine)) return false;
		const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
			.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
		if (!Test->TestNotNull(TEXT("Late-arrival fixture world"), World)) return false;
		// ServerTravel queries the engine-owned seamless-travel handler even when
		// the GameMode observes dispatch without loading another map.
		Instance = NewObject<UGameInstance>(GEngine);
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.OwningGameInstance = Instance;
		World->SetGameInstance(Instance);
		Context.SetCurrentWorld(World);
		// Timeout selection discovers its real catalog through the GameInstance
		// registry; static hero-ID lookup alone does not initialize that subsystem.
		Instance->Init();
		auto* Registry = Instance->GetSubsystem<UBwayHeroRegistry>();
		if (!Test->TestNotNull(TEXT("Production hero registry subsystem"), Registry)
			|| !Test->TestTrue(TEXT("Production hero catalog is populated"), !Registry->GetAllHeroSoftObjects().IsEmpty())) return false;
		GS = World->SpawnActor<ABwayGameState>();
		Mode = World->SpawnActor<ABwayLateArrivalTestGameMode>();
		if (!Test->TestNotNull(TEXT("Production GameState"), GS) || !Test->TestNotNull(TEXT("Observed production GameMode"), Mode)) return false;
		World->SetGameState(GS);
		Mode->GameState = GS;
		const auto* ModeProperty = FindFProperty<FObjectPropertyBase>(UWorld::StaticClass(), TEXT("AuthorityGameMode"));
		const auto* TeamsProperty = FindFProperty<FArrayProperty>(ABwayGameState::StaticClass(), TEXT("Teams"));
		if (!Test->TestNotNull(TEXT("Authority GameMode fixture property"), ModeProperty) || !Test->TestNotNull(TEXT("Team fixture property"), TeamsProperty)) return false;
		ModeProperty->SetObjectPropertyValue_InContainer(World, Mode);
		auto* Teams = TeamsProperty->ContainerPtrToValuePtr<TArray<FTeamInfo>>(GS);
		if (Teams->IsEmpty()) { Teams->Add(FTeamInfo(0)); Teams->Add(FTeamInfo(1)); }
		if (!Test->TestEqual(TEXT("Exactly two production teams"), Teams->Num(), 2)) return false;
		SetOptions(bStaging ? TEXT("?HeroSelectStaging=1?HeroSelectTargetMap=/BreakawayCore/Maps/L_BW_Dorado?HeroSelectTargetExperience=B_BW_Experience_CaptureTheRelic?NumBots=0") : TEXT("?SkipHeroSelection=1?NumBots=0"));
		Manager = GS->HeroSelectionManager;
		Phase = GS->HeroSelectionPhaseComponent;
		Rounds = GS->GetRoundManagement();
		Manager->SelectionTimeLimit = SelectionSeconds;
		Phase->HeroSelectionWidgetClass = UCommonActivatableWidget::StaticClass();
		GS->ResultsScreenWidgetClass = UUserWidget::StaticClass();
		Heroes = {
			FPrimaryAssetId(TEXT("HeroDataAsset"), TEXT("DA_BW_HeroData_Argus")),
			FPrimaryAssetId(TEXT("HeroDataAsset"), TEXT("DA_BW_HeroData_Alona")),
			FPrimaryAssetId(TEXT("HeroDataAsset"), TEXT("DA_BW_HeroData_Gunslinger")),
			FPrimaryAssetId(TEXT("HeroDataAsset"), TEXT("DA_BW_HeroData_Hexweaver"))};
		for (const auto& Hero : Heroes)
			if (!Test->TestNotNull(TEXT("Real hero definition"), UBwayHeroRegistry::GetHeroDataById(Hero))) return false;
		// Exercise real component teardown only after a valid component BeginPlay.
		// The world itself remains isolated from engine travel and phase-asset ticks.
		UActorComponent* LifecycleComponents[] = { Manager, Phase };
		for (auto* Component : LifecycleComponents)
		{
			if (!Component->IsRegistered()) Component->RegisterComponent();
			Component->RegisterAllComponentTickFunctions(true);
			if (!Component->HasBegunPlay()) Component->BeginPlay();
		}
		World->GetTimerManager().Tick(0.f);
		return Test->TestEqual(TEXT("World resolves production authority"), World->GetAuthGameMode(), static_cast<AGameModeBase*>(Mode));
	}

	void SetOptions(const FString& Options)
	{
		Mode->OptionsString = Options;
		World->URL = FURL(nullptr, *(TEXT("/BreakawayCore/Maps/L_BW_HeroSelect_Staging") + Options), TRAVEL_Absolute);
	}

	bool AddPlayer(bool bNotifyRoster = true)
	{
		const auto* ActiveProperty = FindFProperty<FBoolProperty>(UBwayHeroSelectionManager::StaticClass(), TEXT("bSelectionActive"));
		if (!Test->TestNotNull(TEXT("Manager selection-state control property"), ActiveProperty)) return false;
		const bool bSelectionWasActive = ActiveProperty->GetPropertyValue_InContainer(Manager);
		// The adversarial case deliberately omits manager registration while still
		// inserting a real owned PlayerState into the live roster. Restore the
		// manager before the commit callback; do not alter its existing selections.
		if (!bNotifyRoster) ActiveProperty->SetPropertyValue_InContainer(Manager, false);
		ON_SCOPE_EXIT { if (!bNotifyRoster) ActiveProperty->SetPropertyValue_InContainer(Manager, bSelectionWasActive); };
		auto* PC = World->SpawnActor<ABwayLateArrivalTestController>();
		if (!Test->TestNotNull(TEXT("Owned controller"), PC)) return false;
		World->AddController(PC);
		if (!Test->TestNotNull(TEXT("Engine outer required by LocalPlayer ClassWithin"), GEngine)) return false;
		PC->Player = NewObject<ULocalPlayer>(GEngine);
		auto* PS = PC->GetPlayerState<ABwayPlayerState>();
		if (!PS)
		{
			FActorSpawnParameters Spawn;
			Spawn.Owner = PC;
			PS = World->SpawnActor<ABwayPlayerState>(Spawn);
		}
		if (!Test->TestNotNull(TEXT("Production PlayerState"), PS)) return false;
		PC->SetPlayerState(PS);
		PS->SetPlayerId(100 + Players.Num());
		Controllers.Add(PC); Players.Add(PS);
		if (bNotifyRoster) GS->AddPlayerState(PS);
		else GS->PlayerArray.AddUnique(PS); // Adversarial missed projection/notification at the callback boundary.
		GS->AddPlayerToTeam(PS, (Players.Num() - 1) % 2);
		return true;
	}

	bool StartThree(float SelectionSeconds = 60.f)
	{
		if (!Setup(true, SelectionSeconds)) return false;
		for (int32 Index = 0; Index < 3; ++Index) if (!AddPlayer()) return false;
		Phase->StartHeroSelectionPhase();
		for (int32 Index = 0; Index < 3; ++Index) if (!Lock(Index)) return false;
		return Test->TestTrue(TEXT("Three real locks queue travel without closing selection"), FAccess::Pending(Phase) && Phase->IsHeroSelectionActive() && Manager->IsSelectionActive());
	}

	bool Lock(int32 Index)
	{
		const bool bAssigned = Players[Index]->TrySetSelectedHeroId(Heroes[Index]);
		if (!Test->TestTrue(TEXT("Real authority hero selection accepted"), bAssigned)) return false;
		Players[Index]->ServerLockHeroSelection();
		return Test->TestTrue(TEXT("Real authority lock accepted"), Players[Index]->IsHeroLocked());
	}

	void ExpectOriginalLocks() const
	{
		for (int32 Index = 0; Index < FMath::Min(3, Players.Num()); ++Index)
		{
			Test->TestTrue(FString::Printf(TEXT("Prior player %d stays locked"), Index), Players[Index]->IsHeroLocked());
			Test->TestEqual(FString::Printf(TEXT("Prior player %d keeps selected hero"), Index), Players[Index]->GetSelectedHeroId(), Heroes[Index]);
		}
	}

	void RemoveLastPlayer()
	{
		auto* PC = Controllers.Last();
		auto* PS = Players.Last();
		Mode->Logout(PC);
		GS->RemovePlayerState(PS);
		PC->SetPlayerState(nullptr);
		World->RemoveController(PC);
		// Retain the removed actors inside this disposable world for duplicate and
		// deferred-callback controls. They are no longer in either live roster.
		LastRemovedPlayer = PS; LastRemovedController = PC;
		Controllers.Pop(); Players.Pop();
	}

	void Advance(float Seconds)
	{
		if (!World) return;
		World->GetTimerManager().Tick(Seconds);
		if (Manager) Manager->TickComponent(Seconds, LEVELTICK_All, nullptr);
	}
	void SetMatchPhase(EBwayMatchPhase Value)
	{
		const auto* Property = FindFProperty<FEnumProperty>(UBwayRoundManagementComponent::StaticClass(), TEXT("CurrentMatchPhase"));
		if (Test->TestNotNull(TEXT("Reflected match-phase fixture property"), Property))
			*Property->ContainerPtrToValuePtr<EBwayMatchPhase>(Rounds) = Value;
	}

	FAutomationTestBase* Test;
	UWorld* World = nullptr;
	UGameInstance* Instance = nullptr;
	ABwayGameState* GS = nullptr;
	ABwayLateArrivalTestGameMode* Mode = nullptr;
	UBwayHeroSelectionManager* Manager = nullptr;
	UBwayHeroSelectionPhaseComponent* Phase = nullptr;
	UBwayRoundManagementComponent* Rounds = nullptr;
	TArray<ABwayPlayerState*> Players;
	TArray<ABwayLateArrivalTestController*> Controllers;
	TArray<FPrimaryAssetId> Heroes;
	ABwayPlayerState* LastRemovedPlayer = nullptr;
	ABwayLateArrivalTestController* LastRemovedController = nullptr;
};

struct FStep
{
	float AdvanceSeconds;
	TFunction<bool(FFixture&)> Verify;
};

class FScenario : public IAutomationLatentCommand
{
public:
	FScenario(FAutomationTestBase* Test, TArray<FStep>&& InSteps) : Fixture(Test), Steps(MoveTemp(InSteps)) {}
	bool Update() override
	{
		if (!Steps.IsValidIndex(Index)) return true;
		// AActor::ProcessEvent otherwise suppresses native RPC/delegate dispatch in
		// an editor-owned world whose actors have not been globally initialized.
		FEditorScriptExecutionGuard ScriptExecution;
		Fixture.Advance(Steps[Index].AdvanceSeconds);
		const bool bContinue = Steps[Index].Verify(Fixture);
		return !bContinue || ++Index == Steps.Num();
	}
private:
	FFixture Fixture;
	TArray<FStep> Steps;
	int32 Index = 0;
};
}

using namespace BwayLateArrivalSelectionTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalRosterTest, "Breakaway.FrontEnd.LateArrivalSelection.RosterAndReadiness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalRosterTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FScenario(this, {
		{0.f, [](FFixture& F) { return F.StartThree(); }},
		{0.5f, [](FFixture& F)
		{
			const float Before = FAccess::Remaining(F.Phase);
			F.Manager->OnAllPlayersReady.Broadcast(); F.Manager->OnAllPlayersReady.Broadcast();
			F.Test->TestTrue(TEXT("Duplicate readiness does not rearm grace deadline"), Before > 0.f && FMath::IsNearlyEqual(FAccess::Remaining(F.Phase), Before));
			if (!F.AddPlayer()) return false;
			F.Mode->HandleStartingNewPlayer_Implementation(F.Controllers.Last());
			F.ExpectOriginalLocks();
			F.Test->TestFalse(TEXT("Fourth admission cancels pending travel"), FAccess::Pending(F.Phase));
			F.Test->TestFalse(TEXT("Fourth player was not silently assigned or locked"), F.Players.Last()->IsHeroLocked() || F.Players.Last()->GetSelectedHeroId().IsValid());
			F.Test->TestEqual(TEXT("Newcomer receives chooser request"), F.Controllers.Last()->SelectionShows, 1);
			F.Test->TestTrue(TEXT("Shared selection continues to block premature spawning"), FAccess::SpawnBlocked(F.Mode, F.Controllers.Last()));
			return true;
		}},
		{5.5f, [](FFixture& F)
		{
			F.Test->TestEqual(TEXT("Original grace cannot travel with unlocked fourth player"), F.Mode->TravelRequests, 0);
			F.ExpectOriginalLocks();
			return F.Lock(3);
		}},
		{5.1f, [](FFixture& F)
		{
			F.Test->TestEqual(TEXT("Fourth lock admits exactly one travel request"), F.Mode->TravelRequests, 1);
			F.Test->TestEqual(TEXT("Validated travel dispatched once"), F.Mode->TravelDispatches, 1);
			F.Test->TestTrue(TEXT("Accepted URL carries locked selection into match"), F.Mode->DispatchedURL.Contains(TEXT("L_BW_Dorado?")) && F.Mode->DispatchedURL.Contains(TEXT("?SkipHeroSelection=1")));
			F.Test->TestTrue(TEXT("Selection closes only after accepted dispatch"), FAccess::Committed(F.Phase) && F.Phase->HasHeroSelectionCompleted() && !F.Manager->IsSelectionActive());
			F.World->NextURL.Reset(); // Remove the engine's duplicate URL guard: production must guard its own commit.
			F.Manager->OnAllPlayersReady.Broadcast(); FAccess::Schedule(F.Phase); FAccess::Commit(F.Phase);
			F.Test->TestEqual(TEXT("Repeated readiness/callback cannot duplicate committed travel"), F.Mode->TravelRequests, 1);
			F.ExpectOriginalLocks(); return true;
		}}
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalBoundaryTest, "Breakaway.FrontEnd.LateArrivalSelection.LiveRosterAtCommit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalBoundaryTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FScenario(this, {
		{0.f, [](FFixture& F) { return F.StartThree(); }},
		{0.1f, [](FFixture& F)
		{
			if (!F.AddPlayer(false)) return false;
			if (!F.Test->TestEqual(TEXT("Adversarial manager projection still contains three ready players"), F.Manager->GetTotalPlayers(), 3)) return false;
			F.Test->TestTrue(TEXT("Projection alone would allow travel"), F.Manager->AreAllPlayersReady());
			F.Test->TestFalse(TEXT("Live roster independently rejects unlocked newcomer"), FAccess::Ready(F.Phase));
			FAccess::Commit(F.Phase);
			F.Test->TestEqual(TEXT("Commit callback vetoes stale all-ready projection"), F.Mode->TravelRequests, 0);
			F.Test->TestFalse(TEXT("Veto does not mark travel committed"), FAccess::Committed(F.Phase));
			F.Test->TestTrue(TEXT("Veto leaves selection available"), F.Phase->IsHeroSelectionActive() && F.Manager->IsSelectionActive());
			F.ExpectOriginalLocks(); return true;
		}}
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalDisconnectTest, "Breakaway.FrontEnd.LateArrivalSelection.DisconnectResumesOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalDisconnectTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FScenario(this, {
		{0.f, [](FFixture& F) { return F.StartThree() && F.AddPlayer(); }},
		{0.1f, [](FFixture& F)
		{
			F.Test->TestFalse(TEXT("Unlocked arrival holds travel"), FAccess::Pending(F.Phase));
			F.RemoveLastPlayer(); return true;
		}},
		{0.1f, [](FFixture& F)
		{
			F.Test->TestEqual(TEXT("Disconnect removes selection entry"), F.Manager->GetTotalPlayers(), 3);
			F.Test->TestTrue(TEXT("Remaining locked roster receives one grace timer"), FAccess::Pending(F.Phase));
			F.ExpectOriginalLocks(); return true;
		}},
		{0.5f, [](FFixture& F)
		{
			const float Before = FAccess::Remaining(F.Phase);
			F.GS->RemovePlayerState(F.LastRemovedPlayer);
			F.Test->TestTrue(TEXT("Duplicate removal cannot extend travel deadline"), Before > 0.f && FMath::IsNearlyEqual(FAccess::Remaining(F.Phase), Before));
			F.Test->TestFalse(TEXT("Duplicate removal does not schedule another roster callback"), FAccess::RecheckPending(F.Phase));
			return true;
		}},
		{4.6f, [](FFixture& F)
		{
			F.Test->TestEqual(TEXT("Disconnect resumes one validated travel"), F.Mode->TravelRequests, 1);
			F.Test->TestFalse(TEXT("Commit leaves no pending travel"), FAccess::Pending(F.Phase));
			F.Test->TestFalse(TEXT("Commit leaves no roster recheck"), FAccess::RecheckPending(F.Phase));
			return true;
		}},
		{6.f, [](FFixture& F) { return F.Test->TestEqual(TEXT("No delayed duplicate after disconnect"), F.Mode->TravelRequests, 1); }}
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalTimeoutTest, "Breakaway.FrontEnd.LateArrivalSelection.TimeoutPreservesLocks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalTimeoutTest::RunTest(const FString&)
{
	AddExpectedMessage(TEXT("BwayHeroSelectionManager: Selection time expired"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
	ADD_LATENT_AUTOMATION_COMMAND(FScenario(this, {
		{0.f, [](FFixture& F) { return F.StartThree(2.f); }},
		{1.f, [](FFixture& F) { return F.AddPlayer(); }},
		{1.25f, [](FFixture& F)
		{
			F.Test->TestFalse(TEXT("New arrival receives fresh deadline beyond original timeout"), F.Players.Last()->IsHeroLocked());
			F.ExpectOriginalLocks(); return true;
		}},
		{1.f, [](FFixture& F)
		{
			F.Test->TestTrue(TEXT("Actual selection timeout resolves newcomer"), F.Players.Last()->IsHeroLocked() && UBwayHeroRegistry::GetHeroDataById(F.Players.Last()->GetSelectedHeroId()));
			F.Test->TestTrue(TEXT("Timeout queues bounded grace after resolving selection"), FAccess::Pending(F.Phase));
			F.ExpectOriginalLocks(); return true;
		}},
		{5.1f, [](FFixture& F) { F.ExpectOriginalLocks(); return F.Test->TestEqual(TEXT("Timeout resolution travels once"), F.Mode->TravelRequests, 1); }}
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalTeardownTest, "Breakaway.FrontEnd.LateArrivalSelection.TeardownCancelsCallbacks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalTeardownTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FScenario(this, {
		{0.f, [](FFixture& F) { return F.StartThree(); }},
		{0.1f, [](FFixture& F)
		{
			F.Phase->HandleSelectionRosterChanged();
			F.Test->TestTrue(TEXT("Teardown control owns a roster callback"), FAccess::RecheckPending(F.Phase));
			if (!F.Test->TestTrue(TEXT("Teardown control began its component lifecycle"), F.Phase->HasBegunPlay())) return false;
			F.Phase->EndPlay(EEndPlayReason::EndPlayInEditor);
			F.Test->TestFalse(TEXT("Teardown removes pending travel timer"), FAccess::Pending(F.Phase));
			F.Test->TestFalse(TEXT("Teardown removes pending roster callback"), FAccess::RecheckPending(F.Phase));
			F.Test->TestFalse(TEXT("Teardown does not report selection success"), F.Phase->HasHeroSelectionCompleted());
			F.Manager->OnAllPlayersReady.Broadcast(); return true;
		}},
		{10.f, [](FFixture& F) { return F.Test->TestEqual(TEXT("Neither readiness nor cleared callbacks travel after teardown"), F.Mode->TravelRequests, 0); }}
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalPrematchTest, "Breakaway.FrontEnd.LateArrivalSelection.PrematchAdmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalPrematchTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FScenario(this, {
		{0.f, [](FFixture& F)
		{
			if (!F.Setup(false) || !F.AddPlayer() || !F.Lock(0) || !F.AddPlayer()) return false;
			if (!F.Test->TestTrue(TEXT("Real editor Prematch hero-lock predicate is enabled"), UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayMatchMap(F.Rounds))) return false;
			F.Rounds->EnterPrematch();
			// Phase-ability startup is a separate gate; retain the real orchestrator's
			// Prematch state but prevent unrelated assets from starting in this world.
			F.World->GetTimerManager().ClearAllTimersForObject(F.Rounds);
			F.Mode->PostLogin(F.Controllers[1]);
			F.Mode->HandleStartingNewPlayer_Implementation(F.Controllers[1]);
			F.Test->TestTrue(TEXT("Prematch arrival has individual spawn gate"), FAccess::Awaiting(F.Mode, F.Controllers[1]) && FAccess::SpawnBlocked(F.Mode, F.Controllers[1]));
			F.Test->TestEqual(TEXT("PostLogin and seamless-start notifications show chooser once"), F.Controllers[1]->SelectionShows, 1);
			F.Rounds->TryCompletePrematchAfterHeroLocks();
			F.Test->TestEqual(TEXT("Unlocked arrival prevents premature Prematch completion"), F.Rounds->GetCurrentMatchPhase(), EBwayMatchPhase::Prematch);
			if (!F.Lock(1)) return false;
			F.Manager->OnPlayerHeroLocked.Broadcast(F.Players[1], F.Heroes[1]); // Duplicate callback before deferred completion.
			return true;
		}},
		{0.1f, [](FFixture& F)
		{
			F.Test->TestEqual(TEXT("Real lock completion resumes Prematch into Warmup"), F.Rounds->GetCurrentMatchPhase(), EBwayMatchPhase::Warmup);
			F.World->GetTimerManager().ClearAllTimersForObject(F.Rounds);
			F.Test->TestFalse(TEXT("Accepted lock clears individual admission gate"), FAccess::Awaiting(F.Mode, F.Controllers[1]) || FAccess::SpawnBlocked(F.Mode, F.Controllers[1]));
			F.Test->TestEqual(TEXT("Duplicate lock notifications hide selection once"), F.Controllers[1]->SelectionHides, 1);
			F.Test->TestTrue(TEXT("Carried player's lock preserved"), F.Players[0]->IsHeroLocked());
			F.Test->TestEqual(TEXT("Carried player's hero preserved"), F.Players[0]->GetSelectedHeroId(), F.Heroes[0]);
			return true;
		}}
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalExclusionsTest, "Breakaway.FrontEnd.LateArrivalSelection.PreservedAdmissionPaths", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalExclusionsTest::RunTest(const FString&)
{
	FEditorScriptExecutionGuard ScriptExecution;
	FFixture F(this);
	if (!F.Setup(false) || !F.AddPlayer() || !F.Lock(0)) return false;
	TestFalse(TEXT("Valid carried hero does not reopen selection"), FAccess::Prepare(F.Mode, F.Controllers[0]));
	TestFalse(TEXT("Valid carried hero has no individual gate"), FAccess::Awaiting(F.Mode, F.Controllers[0]));
	F.SetOptions(TEXT("?SkipHeroSelection=1?ForceHumanoid=1?NumBots=0"));
	if (!F.AddPlayer()) return false;
	TestFalse(TEXT("ForceHumanoid preserves explicit no-hero path"), FAccess::Prepare(F.Mode, F.Controllers[1]));
	TestFalse(TEXT("ForceHumanoid does not acquire a selection spawn gate"), FAccess::SpawnBlocked(F.Mode, F.Controllers[1]));
	TestEqual(TEXT("ForceHumanoid does not show selection"), F.Controllers[1]->SelectionShows, 0);
	F.SetOptions(TEXT("?SkipHeroSelection=1?NumBots=0"));
	F.SetMatchPhase(EBwayMatchPhase::PostMatch);
	F.GS->ShowResultsScreen(0);
	if (!F.AddPlayer()) return false;
	F.Mode->PostLogin(F.Controllers[2]);
	TestFalse(TEXT("PostMatch visitor is not diverted into selection"), FAccess::Awaiting(F.Mode, F.Controllers[2]));
	TestEqual(TEXT("PostMatch visitor has no selection widget"), F.Controllers[2]->SelectionShows, 0);
	TestEqual(TEXT("PostMatch visitor receives retained results"), F.Controllers[2]->ResultsShows, 1);
	TestTrue(TEXT("Results arrival leaves original hero locked"), F.Players[0]->IsHeroLocked());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalPrematchDisconnectTest, "Breakaway.FrontEnd.LateArrivalSelection.PrematchDisconnect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalPrematchDisconnectTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FScenario(this, {
		{0.f, [](FFixture& F)
		{
			if (!F.Setup(false) || !F.AddPlayer() || !F.Lock(0) || !F.AddPlayer()) return false;
			if (!F.Test->TestTrue(TEXT("Real editor Prematch disconnect predicate is enabled"), UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayMatchMap(F.Rounds))) return false;
			F.Rounds->EnterPrematch();
			F.World->GetTimerManager().ClearAllTimersForObject(F.Rounds);
			F.Mode->HandleStartingNewPlayer_Implementation(F.Controllers[1]);
			F.Rounds->TryCompletePrematchAfterHeroLocks();
			F.Test->TestEqual(TEXT("Control: unselected human keeps Prematch waiting"), F.Rounds->GetCurrentMatchPhase(), EBwayMatchPhase::Prematch);
			F.RemoveLastPlayer(); return true;
		}},
		{0.1f, [](FFixture& F)
		{
			F.Test->TestEqual(TEXT("Production roster removal releases final Prematch hold"), F.Rounds->GetCurrentMatchPhase(), EBwayMatchPhase::Warmup);
			F.World->GetTimerManager().ClearAllTimersForObject(F.Rounds);
			F.Test->TestFalse(TEXT("Disconnect removes individual gate"), FAccess::Awaiting(F.Mode, F.LastRemovedController));
			F.Test->TestTrue(TEXT("Remaining player's valid lock survives"), F.Players[0]->IsHeroLocked());
			return true;
		}}
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalStaleLockTest, "Breakaway.FrontEnd.LateArrivalSelection.RemovedPlayerLockCallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalStaleLockTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FScenario(this, {
		{0.f, [](FFixture& F)
		{
			if (!F.Setup(false) || !F.AddPlayer()) return false;
			F.SetMatchPhase(EBwayMatchPhase::Playing);
			F.Mode->HandleStartingNewPlayer_Implementation(F.Controllers[0]);
			if (!F.Lock(0)) return false;
			F.RemoveLastPlayer(); return true;
		}},
		{0.1f, [](FFixture& F)
		{
			F.Test->TestFalse(TEXT("Removed lock owner cannot retain admission gate"), FAccess::Awaiting(F.Mode, F.LastRemovedController));
			F.Test->TestEqual(TEXT("Deferred lock callback does not act on removed controller"), F.LastRemovedController->SelectionHides, 0);
			F.Test->TestNull(TEXT("Removed owner receives no replacement pawn"), F.LastRemovedController->GetPawn());
			return true;
		}}
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalInvalidCarriedTest, "Breakaway.FrontEnd.LateArrivalSelection.InvalidCarriedLockRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalInvalidCarriedTest::RunTest(const FString&)
{
	FEditorScriptExecutionGuard ScriptExecution;
	FFixture F(this);
	if (!F.Setup(false) || !F.AddPlayer()) return false;
	const auto* HeroProperty = FindFProperty<FStructProperty>(ABwayPlayerState::StaticClass(), TEXT("SelectedHeroId"));
	const auto* LockedProperty = FindFProperty<FBoolProperty>(ABwayPlayerState::StaticClass(), TEXT("bHeroLocked"));
	if (!TestNotNull(TEXT("Carried hero control property"), HeroProperty) || !TestNotNull(TEXT("Carried lock control property"), LockedProperty)) return false;
	*HeroProperty->ContainerPtrToValuePtr<FPrimaryAssetId>(F.Players[0]) = FPrimaryAssetId(TEXT("HeroDataAsset"), TEXT("Missing_Test_Hero"));
	LockedProperty->SetPropertyValue_InContainer(F.Players[0], true);
	F.Mode->HandleStartingNewPlayer_Implementation(F.Controllers[0]);
	TestFalse(TEXT("Unresolvable carried lock is reopened"), F.Players[0]->IsHeroLocked());
	TestTrue(TEXT("Invalid carry remains gated until a valid choice"), FAccess::Awaiting(F.Mode, F.Controllers[0]) && FAccess::SpawnBlocked(F.Mode, F.Controllers[0]));
	TestEqual(TEXT("Invalid carry receives one chooser"), F.Controllers[0]->SelectionShows, 1);
	TestTrue(TEXT("Player can replace invalid carried choice through production request"), F.Players[0]->TrySetSelectedHeroId(F.Heroes[0]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalRejectedTravelTest, "Breakaway.FrontEnd.LateArrivalSelection.RejectedTravelNeedsHostRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalRejectedTravelTest::RunTest(const FString&)
{
	AddExpectedMessage(TEXT("BwayHeroSelectionPhaseComponent: staging travel request failed; selection remains open"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 2, false);
	AddExpectedMessage(TEXT("BwayHeroSelectionManager: Selection time expired"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
	ADD_LATENT_AUTOMATION_COMMAND(FScenario(this, {
		{0.f, [](FFixture& F) { if (!F.StartThree(8.f)) return false; F.Mode->bRejectTravel = true; return true; }},
		{5.1f, [](FFixture& F)
		{
			F.Test->TestEqual(TEXT("Real ServerTravel rejection observed once"), F.Mode->TravelRequests, 1);
			F.Test->TestEqual(TEXT("Rejected request never dispatches a map"), F.Mode->TravelDispatches, 0);
			F.Test->TestTrue(TEXT("Travel failure preserves active selection"), F.Phase->HasPostSelectionTravelFailed() && F.Phase->IsHeroSelectionActive() && F.Manager->IsSelectionActive());
			F.Test->TestFalse(TEXT("Failure never claims selection complete"), F.Phase->HasHeroSelectionCompleted());
			F.ExpectOriginalLocks();
			// A remote server-side controller has no LocalPlayer; authority alone
			// must not authorize it to retry the host's failed travel.
			auto* Remote = F.Controllers[1];
			UPlayer* OriginalPlayer = Remote->Player;
			Remote->Player = nullptr;
			F.Test->TestFalse(TEXT("Remote controller cannot offer host retry"), Remote->CanRetryHeroSelectionTravel());
			F.Test->TestFalse(TEXT("Remote controller cannot request host retry"), Remote->RetryHeroSelectionTravel());
			Remote->Player = OriginalPlayer;
			F.Test->TestTrue(TEXT("Locked host can explicitly retry"), F.Controllers[0]->CanRetryHeroSelectionTravel());
			F.Test->TestFalse(TEXT("Repeated engine rejection remains a failure"), F.Controllers[0]->RetryHeroSelectionTravel());
			F.Controllers[0]->RetryHeroSelectionTravel(); // Duplicate authored click path in the same frame.
			F.Test->TestEqual(TEXT("One explicit failed retry per input frame"), F.Mode->TravelRequests, 2);
			F.Manager->OnAllPlayersReady.Broadcast(); FAccess::Schedule(F.Phase); FAccess::Commit(F.Phase);
			return F.Test->TestFalse(TEXT("Readiness cannot queue another automatic attempt"), FAccess::Pending(F.Phase));
		}},
		{10.f, [](FFixture& F)
		{
			F.Test->TestEqual(TEXT("Global timeout cannot silently retry rejected travel"), F.Mode->TravelRequests, 2);
			F.Test->TestTrue(TEXT("Failure survives timeout"), F.Phase->HasPostSelectionTravelFailed());
			F.ExpectOriginalLocks();
			if (!F.AddPlayer()) return false;
			F.Test->TestFalse(TEXT("Host retry rechecks newly admitted live roster"), F.Controllers[0]->CanRetryHeroSelectionTravel());
			F.Test->TestFalse(TEXT("Unlocked newcomer vetoes explicit retry"), F.Controllers[0]->RetryHeroSelectionTravel());
			F.RemoveLastPlayer();
			F.Test->TestTrue(TEXT("Disconnect restores retry availability without clearing failure"), F.Controllers[0]->CanRetryHeroSelectionTravel() && F.Phase->HasPostSelectionTravelFailed());
			return true;
		}},
		{0.1f, [](FFixture& F)
		{
			F.Test->TestFalse(TEXT("Disconnect cannot rearm failed travel"), FAccess::Pending(F.Phase));
			if (!F.AddPlayer() || !F.Lock(3)) return false;
			F.Test->TestTrue(TEXT("Newcomer lock restores host retry availability"), F.Controllers[0]->CanRetryHeroSelectionTravel());
			return F.Test->TestFalse(TEXT("Newcomer lock alone cannot retry failed travel"), FAccess::Pending(F.Phase));
		}},
		{6.f, [](FFixture& F)
		{
			F.Test->TestEqual(TEXT("Roster notifications never clear the failure latch"), F.Mode->TravelRequests, 2);
			F.Mode->bRejectTravel = false;
			F.Test->TestTrue(TEXT("Explicit host retry accepts corrected travel once"), F.Controllers[0]->RetryHeroSelectionTravel());
			F.Test->TestEqual(TEXT("Only three deliberate total attempts"), F.Mode->TravelRequests, 3);
			F.Test->TestEqual(TEXT("Exactly one accepted map dispatch"), F.Mode->TravelDispatches, 1);
			F.Test->TestFalse(TEXT("Accepted recovery clears replicated failure"), F.Phase->HasPostSelectionTravelFailed());
			F.Test->TestTrue(TEXT("Recovered selection completes"), F.Phase->HasHeroSelectionCompleted() && !F.Manager->IsSelectionActive());
			F.World->NextURL.Reset();
			F.Test->TestFalse(TEXT("Duplicate retry after dispatch is rejected"), F.Controllers[0]->RetryHeroSelectionTravel());
			FAccess::Commit(F.Phase);
			F.Test->TestEqual(TEXT("No duplicate accepted travel"), F.Mode->TravelRequests, 3);
			F.ExpectOriginalLocks();
			return F.Test->TestTrue(TEXT("Fourth lock survives recovery"), F.Players[3]->IsHeroLocked() && F.Players[3]->GetSelectedHeroId() == F.Heroes[3]);
		}}
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayLateArrivalMissingTargetTest, "Breakaway.FrontEnd.LateArrivalSelection.MissingTargetNeedsHostRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBwayLateArrivalMissingTargetTest::RunTest(const FString&)
{
	AddExpectedMessage(TEXT("BwayHeroSelectionPhaseComponent: HeroSelectStaging is set but no HeroSelectTargetMap or PostHeroSelectionMapID was provided"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1, false);
	AddExpectedMessage(TEXT("BwayHeroSelectionManager: Selection time expired"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
	ADD_LATENT_AUTOMATION_COMMAND(FScenario(this, {
		{0.f, [](FFixture& F)
		{
			if (!F.StartThree()) return false;
			F.SetOptions(TEXT("?HeroSelectStaging=1?NumBots=0"));
			F.Phase->PostHeroSelectionMapID = FPrimaryAssetId();
			return true;
		}},
		{5.1f, [](FFixture& F)
		{
			F.Test->TestTrue(TEXT("Missing target latches recoverable failure"), F.Phase->HasPostSelectionTravelFailed());
			return F.Test->TestEqual(TEXT("Missing target never reaches engine travel"), F.Mode->TravelRequests, 0);
		}},
		{61.f, [](FFixture& F)
		{
			F.Test->TestTrue(TEXT("Missing-target failure survives global timeout"), F.Phase->HasPostSelectionTravelFailed());
			F.Test->TestFalse(TEXT("Timeout leaves no hidden travel retry"), FAccess::Pending(F.Phase));
			F.SetOptions(TEXT("?HeroSelectStaging=1?HeroSelectTargetMap=/BreakawayCore/Maps/L_BW_Dorado?HeroSelectTargetExperience=B_BW_Experience_CaptureTheRelic?NumBots=0"));
			F.Manager->OnAllPlayersReady.Broadcast();
			F.Test->TestFalse(TEXT("Correcting configuration alone does not silently retry"), FAccess::Pending(F.Phase));
			F.Test->TestTrue(TEXT("Host deliberately retries corrected configuration"), F.Controllers[0]->RetryHeroSelectionTravel());
			F.Test->TestEqual(TEXT("Corrected target dispatches exactly once"), F.Mode->TravelDispatches, 1);
			F.ExpectOriginalLocks();
			return F.Test->TestFalse(TEXT("Recovery clears missing-target failure"), F.Phase->HasPostSelectionTravelFailed());
		}}
	}));
	return true;
}
#endif
