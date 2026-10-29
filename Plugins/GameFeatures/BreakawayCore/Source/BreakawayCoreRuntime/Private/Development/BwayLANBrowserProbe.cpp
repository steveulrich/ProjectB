// Development-only orchestration of the real session browser and connected return.
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "CommonActivatableWidget.h"
#include "CommonSessionSubsystem.h"
#include "CommonUIExtensions.h"
#include "Components/ListView.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/BwayUserFacingExperienceDefinition.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/PlatformMisc.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "PrimaryGameLayout.h"
#include "UI/BwayCommonButton.h"
#include "UI/BwayFrontEndFlowWidgets.h"
#include "UI/BwayMatchBreakdownWidget.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace BwayLANBrowser
{
bool Active = false;

UWorld* GameWorld()
{
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->WorldType == EWorldType::Game && World->GetGameInstance()) return World;
		}
	}
	return nullptr;
}

template<typename T>
T* ObjectProperty(UObject* Object, FName Name)
{
	const FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
	return Property ? Cast<T>(Property->GetObjectPropertyValue_InContainer(Object)) : nullptr;
}

UBwayMainMenuWidget* Frontend(APlayerController* PC, bool* OutLegacyShooterActive = nullptr)
{
	if (OutLegacyShooterActive) *OutLegacyShooterActive = false;
	UBwayMainMenuWidget* FoundMainMenu = nullptr;
	for (TObjectIterator<UCommonActivatableWidget> It; It; ++It)
	{
		if (It->IsTemplate() || It->GetOwningPlayer() != PC || !It->IsActivated()) continue;
		if (It->GetClass()->GetName() == TEXT("W_LyraFrontEnd_C"))
		{
			if (OutLegacyShooterActive) *OutLegacyShooterActive = true;
		}
		else if (auto* MainMenu = Cast<UBwayMainMenuWidget>(*It))
		{
			FoundMainMenu = MainMenu;
		}
	}
	return FoundMainMenu;
}

UBwayMatchBreakdownWidget* Results(APlayerController* PC)
{
	for (TObjectIterator<UBwayMatchBreakdownWidget> It; It; ++It)
	{
		if (It->GetOwningPlayer() == PC && It->IsActivated()) return *It;
	}
	return nullptr;
}

struct FProbe
{
	bool Host = false;
	bool Reconnect = false;
	int32 Step = 0;
	int32 Lap = 1;
	double Started = FPlatformTime::Seconds();
	double Since = Started;
	double LastBrowserRefresh = 0.0;
	TWeakObjectPtr<UBwayLocalMatchBrowserWidget> Browser;
	TWeakObjectPtr<UCommonSession_SearchSessionRequest> FirstSearch;
	FString FirstSessionId;
	bool LoggedWaitingRoster = false;

	void Diagnose()
	{
		UWorld* World = GameWorld();
		AGameStateBase* GS = World ? World->GetGameState() : nullptr;
		auto* BGS = Cast<ABwayGameState>(GS);
		auto* Round = BGS ? BGS->GetRoundManagement() : nullptr;
		UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: Diagnostic lap=%d step=%d map=%s phase=%d players=%d"),
			Lap, Step, World ? *World->GetMapName() : TEXT("None"), Round ? int32(Round->GetCurrentMatchPhase()) : -1, GS ? GS->PlayerArray.Num() : -1);
		if (!GS) return;
		for (APlayerState* PS : GS->PlayerArray)
		{
			const auto* BPS = Cast<ABwayPlayerState>(PS);
			UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: DiagnosticPeer player=%d bot=%d inactive=%d team=%d locked=%d pawn=%s hero=%s"),
				PS ? PS->GetPlayerId() : -1, PS && PS->IsABot(), PS && PS->IsInactive(), BPS ? BPS->GetTeamId() : -1,
				BPS && BPS->IsHeroLocked(), *GetNameSafe(PS ? PS->GetPawn() : nullptr), BPS ? *BPS->GetSelectedHeroId().ToString() : TEXT("None"));
		}
	}

	void Advance(int32 Next)
	{
		Step = Next;
		Since = FPlatformTime::Seconds();
	}
	bool Fail(const TCHAR* Reason)
	{
		Diagnose();
		UE_LOG(LogTemp, Error, TEXT("BwayLANBrowser: FAIL role=%s lap=%d step=%d reason=%s"), Host ? TEXT("host") : TEXT("client"), Lap, Step, Reason);
		Active = false;
		return false;
	}
	bool CheckLayoutBeforePlayerState(UWorld* World, APlayerController* PC, const TCHAR* Phase)
	{
		UPrimaryGameLayout* Expected = UPrimaryGameLayout::GetPrimaryGameLayout(PC->GetLocalPlayer());
		APlayerState* OriginalState = PC->PlayerState;
		bool PrimaryUnavailable = false;
		bool LayoutMatches = false;
		{
			// Reproduce the travel ordering synchronously, without ticking or sending
			// network updates. Restore the controller's reference before any other work.
			TGuardValue<decltype(PC->PlayerState)> Guard(PC->PlayerState, nullptr);
			// On a listen server the legacy lookup can return a remote controller;
			// it still cannot resolve the primary local player's layout.
			APlayerController* LegacyController = World->GetGameInstance()->GetPrimaryPlayerController(false);
			PrimaryUnavailable = UPrimaryGameLayout::GetPrimaryGameLayout(LegacyController) == nullptr;
			LayoutMatches = Expected && UPrimaryGameLayout::GetPrimaryGameLayoutForPrimaryPlayer(World) == Expected;
		}
		const bool Restored = PC->PlayerState == OriginalState;
		if (!PrimaryUnavailable || !LayoutMatches || !Restored) return Fail(TEXT("layout-before-player-state"));
		UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: LayoutWithoutPlayerState lap=%d phase=%s primaryUnavailable=%d layoutMatches=%d restored=%d"),
			Lap, Phase, PrimaryUnavailable, LayoutMatches, Restored);
		return true;
	}
	bool HostSession(UWorld* World, APlayerController* PC)
	{
		bool bLegacyShooter = false;
		UBwayMainMenuWidget* MainMenu = Frontend(PC, &bLegacyShooter);
		if (bLegacyShooter) return Fail(TEXT("legacy-shooter-frontend-active"));
		if (!MainMenu) return Fail(TEXT("breakaway-main-menu-missing"));
		auto* Lobby = Cast<UBwayMatchLobbyWidget>(MainMenu->ClickHostLanMatch());
		if (!Lobby || !Lobby->IsActivated()) return Fail(TEXT("host-lan-lobby-open-failed"));
		// Shorten this session-flow fixture only. Results are driven by production EndRound;
		// this is explicitly not evidence of natural combat or a goal scored by player input.
		TMap<FString, FString> FixtureArgs;
		FixtureArgs.Add(TEXT("NumBots"), TEXT("4"));
		FixtureArgs.Add(TEXT("PointsToWin"), TEXT("1"));
		FixtureArgs.Add(TEXT("RoundDuration"), TEXT("180"));
		FixtureArgs.Add(TEXT("WarmupDuration"), TEXT("3"));
		FixtureArgs.Add(TEXT("PostRoundDuration"), TEXT("2"));
		FString AdvertisedPlaylist;
		if (!Lobby->HostConfiguredMatch(FixtureArgs, AdvertisedPlaylist))
		{
			return Fail(TEXT("invalid-lan-playlist"));
		}
		UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: Hosted lap=%d mode=LAN playlist=%s via=UBwayMatchLobbyWidget"), Lap, *AdvertisedPlaylist);
		return true;
	}
	bool OpenBrowser(APlayerController* PC)
	{
		bool bLegacyShooter = false;
		UBwayMainMenuWidget* MainMenu = Frontend(PC, &bLegacyShooter);
		if (bLegacyShooter) return Fail(TEXT("legacy-shooter-frontend-active"));
		if (!MainMenu) return Fail(TEXT("breakaway-main-menu-missing"));
		Browser = Cast<UBwayLocalMatchBrowserWidget>(MainMenu->ClickFindLanMatch());
		if (!Browser.IsValid() || !Browser->IsActivated()) return Fail(TEXT("browser-push-failed"));
		LastBrowserRefresh = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: BrowserOpened lap=%d owner=%d widget=UBwayLocalMatchBrowserWidget"), Lap, PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1);
		Advance(1);
		return true;
	}
	bool Roster(UWorld* World, bool Gameplay)
	{
		AGameStateBase* GS = World->GetGameState();
		if (!GS) return false;
		int32 Humans = 0, Bots = 0, Teams[2] = {0, 0};
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (!PS || PS->IsInactive()) continue;
			PS->IsABot() ? ++Bots : ++Humans;
			if (Gameplay)
			{
				const auto* BPS = Cast<ABwayPlayerState>(PS);
				const auto* BGS = Cast<ABwayGameState>(GS);
				const int32 TeamIndex = BGS ? BGS->GetPlayerTeam(PS) : -1;
				if (!BPS || !BPS->GetSelectedHeroId().IsValid() || !BPS->IsHeroLocked() || !PS->GetPawn()
					|| TeamIndex < 0 || TeamIndex > 1 || BPS->GetTeamId() != TeamIndex + 1) return false;
				// BwayTeamBridge maps canonical indices 0/1 to Lyra IDs 1/2.
				++Teams[TeamIndex];
			}
		}
		return Humans == 4 && (Gameplay ? Bots == 4 && Teams[0] == 4 && Teams[1] == 4 : Bots == 0);
	}
	void LogRoster(UWorld* World, const TCHAR* Phase)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: Roster lap=%d phase=%s mode=%d players=%d"), Lap, Phase, int32(World->GetNetMode()), World->GetGameState()->PlayerArray.Num());
		for (APlayerState* PS : World->GetGameState()->PlayerArray)
		{
			const auto* BPS = Cast<ABwayPlayerState>(PS);
			const auto* BGS = World->GetGameState<ABwayGameState>();
			UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: Peer lap=%d phase=%s player=%d bot=%d team=%d lyraTeam=%d hero=%s"),
				Lap, Phase, PS->GetPlayerId(), PS->IsABot(), BGS ? BGS->GetPlayerTeam(PS) : -1, BPS ? BPS->GetTeamId() : -1,
				BPS ? *BPS->GetSelectedHeroId().PrimaryAssetName.ToString() : TEXT("None"));
		}
	}
	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 600.0 || Now - Since > 150.0) return Fail(TEXT("phase-timeout"));
		UWorld* World = GameWorld();
		APlayerController* PC = World ? World->GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
		if (!PC || !PC->IsLocalController()) return true;
		const bool IsFrontend = World->GetMapName().Contains(TEXT("L_LyraFrontEnd"));
		const bool IsStaging = World->GetMapName().Contains(TEXT("L_BW_HeroSelect_Staging"));
		const bool IsGameplay = World->GetMapName().Contains(TEXT("L_BW_Dorado"));
		auto* GS = World->GetGameState<ABwayGameState>();
		auto* Round = GS ? GS->GetRoundManagement() : nullptr;
		if (Step == 3 && IsGameplay && Round && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && !LoggedWaitingRoster)
		{
			LoggedWaitingRoster = true;
			Diagnose();
		}

		if (Step == 0 && IsFrontend && Frontend(PC))
		{
			if (!CheckLayoutBeforePlayerState(World, PC, TEXT("startup"))) return false;
			if (Host)
			{
				if (!HostSession(World, PC)) return false;
				Advance(2);
			}
			else
			{
				if (!OpenBrowser(PC)) return false;
			}
		}
		else if (Step == 1)
		{
			UBwayLocalMatchBrowserWidget* Screen = Browser.Get();
			if (!Screen || !Screen->IsActivated()) return Fail(TEXT("browser-lost-before-join"));
			UCommonSession_SearchSessionRequest* Request = Screen->GetActiveSearchRequest();
			if (Request && (Request->OnlineMode != ECommonSessionOnlineMode::LAN || Request->bUseLobbies)) return Fail(TEXT("search-not-lan"));
			const EBwayServerBrowserViewState ViewState = Screen->GetBrowserViewState();
			if (ViewState == EBwayServerBrowserViewState::Searching) return true;
			if (ViewState == EBwayServerBrowserViewState::Empty || ViewState == EBwayServerBrowserViewState::Error)
			{
				if (Now - LastBrowserRefresh >= 2.5)
				{
					LastBrowserRefresh = Now;
					Screen->RefreshServerList();
				}
				return true;
			}
			const TArray<FBwayServerBrowserRowViewModel>& Rows = Screen->GetRowViewModels();
			const TArray<TObjectPtr<UCommonSession_SearchResult>>& Discovered = Screen->GetDiscoveredSessions();
			const TArray<TObjectPtr<UBwayCommonButton>>& JoinButtons = Screen->GetRowJoinButtons();
			if (!Request || Rows.IsEmpty() || Discovered.Num() != Rows.Num() || JoinButtons.Num() != Rows.Num()) return true;
			for (int32 RowIdx = 0; RowIdx < Rows.Num(); ++RowIdx)
			{
				const FBwayServerBrowserRowViewModel& Row = Rows[RowIdx];
				UCommonSession_SearchResult* Result = Discovered[RowIdx];
				UBwayCommonButton* JoinBtn = JoinButtons[RowIdx];
				if (!Result || !JoinBtn || Row.PlaylistName != TEXT("DA_Playlist_CTR_LAN")) continue;
				if (Row.SessionName.IsEmpty() || !Row.MapName.Contains(TEXT("Dorado")) || Row.MaxPlayers != 8
					|| !Row.FormattedPlayerCount.ToString().EndsWith(TEXT("/8")))
				{
					return Fail(TEXT("browser-row-metadata-invalid"));
				}
				if (Row.bIsFull || Row.bIsUnreachable || !JoinBtn->GetIsEnabled()) continue;
				const FString SessionId = Result->GetDescription();
				if (Reconnect)
				{
					if (SessionId.IsEmpty()) return Fail(TEXT("missing-session-identity"));
					if (Lap == 1) { FirstSearch = Request; FirstSessionId = SessionId; }
					else if (FirstSearch.Get() == Request || FirstSessionId == SessionId) return Fail(TEXT("stale-rediscovery"));
					UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: SearchIdentity lap=%d session=%s fresh=1"), Lap, *SessionId);
				}
				UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: Discovered lap=%d results=%d entries=%d playlist=%s map=%s players=%s ping=%s open=%d capacity=%d"),
					Lap, Request->Results.Num(), Rows.Num(), *Row.PlaylistName, *Row.MapName,
					*Row.FormattedPlayerCount.ToString(), *Row.FormattedPing.ToString(),
					Result->GetNumOpenPublicConnections(), Result->GetMaxPublicConnections());
				Advance(2);
				if (!JoinBtn->SimulateClick()) return Fail(TEXT("browser-join-button-click-failed"));
				UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: EntryJoinRequested lap=%d"), Lap);
				return true;
			}
			if (Now - LastBrowserRefresh >= 2.5)
			{
				LastBrowserRefresh = Now;
				Screen->RefreshServerList();
			}
		}
		else if (Step == 2 && IsStaging && World->GetNetMode() == (Host ? NM_ListenServer : NM_Client))
		{
			UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: Staging lap=%d mode=%d local=%d"), Lap, int32(World->GetNetMode()), PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1);
			Advance(3);
		}
		else if (Step == 3 && IsGameplay && Round && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && Roster(World, true))
		{
			LogRoster(World, TEXT("playing"));
			Advance(Lap == 1 ? 4 : 8);
		}
		else if (Step == 4)
		{
			if (UBwayMatchBreakdownWidget* Breakdown = Results(PC))
			{
				if (Breakdown->GetPlayerColumns().Num() != 8) return true;
				UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: Results lap=1 columns=8 mode=%d"), int32(World->GetNetMode()));
				Advance(5);
			}
			else if (Host && Now - Since >= 20.0 && Round && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing)
			{
				UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: FixtureEndRound lap=1 source=production-EndRound"));
				Round->EndRound(0, EBwayWinCondition::GoalScored);
			}
		}
		else if (Step == 5)
		{
			if (Host && Now - Since >= 20.0)
			{
				UBwayMatchBreakdownWidget* Breakdown = Results(PC);
				if (!Breakdown) return Fail(TEXT("results-lost-before-return"));
				Advance(6);
				Breakdown->RequestReturnToLobby();
				UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: ReturnRequested lap=1 source=results-widget"));
			}
			else if (!Host && IsFrontend) Advance(6);
		}
		else if (Step == 6 && IsFrontend && Frontend(PC) && Roster(World, false))
		{
			if (World->GetNetMode() != (Host ? NM_ListenServer : NM_Client)) return Fail(TEXT("return-disconnected"));
			if (!CheckLayoutBeforePlayerState(World, PC, TEXT("return"))) return false;
			LogRoster(World, TEXT("frontend"));
			Advance(7);
		}
		else if (Step == 7)
		{
			if (Host && Now - Since >= 20.0)
			{
				Lap = 2;
				if (!HostSession(World, PC)) return false;
				Advance(2);
			}
			else if (!Host && Reconnect && Now - Since >= 8.0)
			{
				Lap = 2;
				Advance(9);
				Browser.Reset();
				UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: DisconnectRequested lap=2 source=game-instance"));
				World->GetGameInstance()->ReturnToMainMenu();
				return true;
			}
			else if (!Host && !Reconnect && IsStaging)
			{
				Lap = 2;
				UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: Staging lap=2 mode=%d local=%d"), int32(World->GetNetMode()), PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1);
				Advance(3);
			}
		}
		else if (Step == 8 && Now - Since >= 15.0)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: PASS role=%s laps=2 discovery=%d connectedReturn=1 rehost=1 reconnect=%d"), Host ? TEXT("host") : TEXT("client"), !Host, Reconnect);
			Active = false;
			if (!Host) FPlatformMisc::RequestExit(false);
			return false;
		}
		else if (Step == 9 && IsFrontend && World->GetNetMode() == NM_Standalone && Frontend(PC))
		{
			if (!CheckLayoutBeforePlayerState(World, PC, TEXT("disconnected"))) return false;
			UE_LOG(LogTemp, Display, TEXT("BwayLANBrowser: DisconnectedFrontend lap=2 mode=0"));
			Advance(10);
		}
		else if (Step == 10 && Now - Since >= 20.0 && IsFrontend && World->GetNetMode() == NM_Standalone && Frontend(PC))
		{
			// Allow the host's connected-return checkpoint and rehost to finish before
			// activating a new browser search. No direct-IP fallback is permitted.
			if (!OpenBrowser(PC)) return false;
		}
		return true;
	}
};

struct FSelectionProbe : FProbe
{
	int32 Role = 0;
	int32 Case = 0;
	double CaseSince = 0.0;
	double SelectionStarted = 0.0;
	double GameplayStarted = 0.0;
	double GameplayObserved = 0.0;
	bool LoggedTransientRoster = false;
	int32 OriginalClientId = -1;
	int32 DepartingPlayerId = -1;
	bool RemovedSelectionVerified = false;
	bool Rejoined = false;
	bool Configured = false;
	bool TestedDuplicateRequest = false;
	float OriginalLimit = 0.0f;
	TWeakObjectPtr<UBwayHeroSelectionManager> ModifiedManager;
	TArray<FPrimaryAssetId> Heroes;
	TMap<int32, FString> AuthorityStates;

	void RestoreLimit()
	{
		if (UBwayHeroSelectionManager* Manager = ModifiedManager.Get()) Manager->SelectionTimeLimit = OriginalLimit;
		ModifiedManager.Reset();
	}
	bool Reject(const TCHAR* Reason)
	{
		RestoreLimit();
		return Fail(Reason);
	}
	void NextCase(int32 Value)
	{
		Case = Value;
		CaseSince = FPlatformTime::Seconds();
	}
	bool LoadHeroes(UWorld* World)
	{
		if (Heroes.Num() >= 4) return true;
		UBwayHeroRegistry* Registry = UBwayHeroRegistry::Get(World);
		if (!Registry) return false;
		for (const auto& Soft : Registry->GetAllHeroSoftObjects())
		{
			if (UBwayHeroDataAsset* Data = Registry->LoadHeroSync(Soft)) Heroes.AddUnique(Data->GetPrimaryAssetId());
		}
		Heroes.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
		return Heroes.Num() >= 4;
	}
	bool Tick(float Delta)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 420.0) return Reject(TEXT("selection-probe-timeout"));
		// Frontend setup does not depend on a replicated PlayerState.
		if (Step < 3) return FProbe::Tick(Delta);
		UWorld* World = GameWorld();
		APlayerController* LocalPC = World ? World->GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
		if (Step == 20)
		{
			if (Now - Since > 30.0) return Reject(TEXT("standalone-frontend-timeout"));
			if (LocalPC && World->GetNetMode() == NM_Standalone && Frontend(LocalPC) && Now - Since >= 4.0)
			{
				UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: Standalone role=%d oldPlayer=%d"), Role, OriginalClientId);
				Rejoined = true;
				NextCase(10);
				Advance(0);
			}
			return true;
		}
		auto* PC = Cast<ABwayPlayerController>(LocalPC);
		auto* PS = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
		auto* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
		if (!PC || !PS) return true;
		if (!GS || !LoadHeroes(World)) return true;
		const FPrimaryAssetId Target = Heroes[Role];
		const bool Staging = World->GetMapName().Contains(TEXT("L_BW_HeroSelect_Staging"));
		if (Staging)
		{
			UBwayHeroSelectionManager* Manager = GS->HeroSelectionManager;
			if (!Manager || !Manager->IsSelectionActive() || GS->GetPlayerTeam(PS) < 0) return true;
			if (Manager->bAllowDuplicateHeroes) return Reject(TEXT("duplicate-heroes-enabled"));
			if (Host)
			{
				if (!Configured)
				{
					OriginalLimit = Manager->SelectionTimeLimit;
					ModifiedManager = Manager;
					Manager->SelectionTimeLimit = 120.0f;
					Manager->StartHeroSelection();
					SelectionStarted = Now;
					PC->Server_SubmitPreSelectedHero(Target, false);
					Configured = true;
					UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: Config unique=1 fixtureSeconds=120 originalSeconds=%.0f"), OriginalLimit);
				}
				int32 Humans = 0, LockedRemotes = 0;
				bool OldInRoster = false, OldInSelection = false;
				for (APlayerState* Peer : GS->PlayerArray)
				{
					auto* BPS = Cast<ABwayPlayerState>(Peer);
					if (!BPS || BPS->IsABot() || BPS->IsInactive()) continue;
					++Humans;
					if (BPS != PS && BPS->IsHeroLocked()) ++LockedRemotes;
					if (DepartingPlayerId < 0 && BPS->GetSelectedHeroId() == Heroes[3]) DepartingPlayerId = BPS->GetPlayerId();
					OldInRoster |= BPS->GetPlayerId() == DepartingPlayerId;
					const FString Signature = FString::Printf(TEXT("%s/%d/%d"), *BPS->GetSelectedHeroId().ToString(), BPS->IsHeroLocked(), GS->GetPlayerTeam(BPS));
					if (AuthorityStates.FindRef(BPS->GetPlayerId()) != Signature)
					{
						AuthorityStates.Add(BPS->GetPlayerId(), Signature);
						UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: Authority player=%d state=%s hero=%s locked=%d team=%d"), BPS->GetPlayerId(), *BPS->GetName(), *BPS->GetSelectedHeroId().ToString(), BPS->IsHeroLocked(), GS->GetPlayerTeam(BPS));
					}
				}
				for (const auto& Selection : Manager->GetAllPlayerSelections())
				{
					OldInSelection |= Selection.PlayerState && Selection.PlayerState->GetPlayerId() == DepartingPlayerId;
				}
				if (DepartingPlayerId >= 0 && !OldInRoster && !OldInSelection && !RemovedSelectionVerified)
				{
					RemovedSelectionVerified = true;
					UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: Removed player=%d roster=0 selection=0"), DepartingPlayerId);
				}
				if (Now - SelectionStarted >= 80.0 && Humans == 4 && LockedRemotes == 3 && RemovedSelectionVerified && !PS->IsHeroLocked())
				{
					RestoreLimit();
					UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: Release humans=4 remoteLocks=3 restoredLimit=1"));
					PC->Server_SubmitPreSelectedHero(Target, true);
				}
				return true;
			}
			if (Case == 0)
			{
				if (PS->GetSelectedHeroId().IsValid() || PS->IsHeroLocked()) return Reject(TEXT("unexpected-initial-selection"));
				OriginalClientId = PS->GetPlayerId();
				PS->ServerLockHeroSelection();
				NextCase(1);
			}
			else if (Case == 1 && Now - CaseSince >= 2.0)
			{
				if (PS->IsHeroLocked() || PS->GetSelectedHeroId().IsValid()) return Reject(TEXT("empty-selection-locked"));
				UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: EmptyLockRejected role=%d player=%d"), Role, PS->GetPlayerId());
				PC->Server_SubmitPreSelectedHero(Target, false);
				NextCase(2);
			}
			else if (Case == 2 && Now - CaseSince >= 2.0)
			{
				if (PS->IsHeroLocked() || PS->GetSelectedHeroId() != Target) return Reject(TEXT("valid-preview-failed"));
				PS->ServerSetSelectedHeroId(FPrimaryAssetId());
				PC->Server_SubmitPreSelectedHero(FPrimaryAssetId(), true);
				PC->Server_SubmitPreSelectedHero(FPrimaryAssetId(TEXT("HeroDataAsset"), TEXT("MissingForSelectionTest")), true);
				PC->Server_SubmitPreSelectedHero(FPrimaryAssetId(TEXT("NotAHero"), Target.PrimaryAssetName), true);
				for (APlayerState* Peer : GS->PlayerArray)
				{
					auto* BPS = Cast<ABwayPlayerState>(Peer);
					if (BPS && BPS != PS && !BPS->IsInactive() && BPS->GetSelectedHeroId().IsValid()
						&& GS->GetPlayerTeam(BPS) == GS->GetPlayerTeam(PS))
					{
						PC->Server_SubmitPreSelectedHero(BPS->GetSelectedHeroId(), true);
						TestedDuplicateRequest = true;
						UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: DuplicateRequested role=%d player=%d peer=%d hero=%s"), Role, PS->GetPlayerId(), BPS->GetPlayerId(), *BPS->GetSelectedHeroId().ToString());
						break;
					}
				}
				NextCase(3);
			}
			else if (Case == 3 && Now - CaseSince >= 2.0)
			{
				if (PS->IsHeroLocked() || PS->GetSelectedHeroId() != Target) return Reject(TEXT("invalid-request-changed-or-locked-hero"));
				UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: InvalidRejected role=%d player=%d hero=%s locked=0 duplicateTested=%d"), Role, PS->GetPlayerId(), *Target.ToString(), TestedDuplicateRequest);
				// This reliable RPC follows the invalid controller requests on the same actor.
				PC->Server_SubmitPreSelectedHero(Target, true);
				NextCase(4);
			}
			else if (Case == 4 && Now - CaseSince >= 2.0)
			{
				if (!PS->IsHeroLocked() || PS->GetSelectedHeroId() != Target) return Reject(TEXT("valid-lock-failed"));
				PS->ServerLockHeroSelection();
				PS->ServerLockHeroSelection();
				PS->ServerSetSelectedHeroId(Heroes[(Role + 1) % Heroes.Num()]);
				PC->Server_SubmitPreSelectedHero(Heroes[(Role + 1) % Heroes.Num()], true);
				NextCase(5);
			}
			else if (Case == 5 && Now - CaseSince >= 2.0)
			{
				if (!PS->IsHeroLocked() || PS->GetSelectedHeroId() != Target) return Reject(TEXT("repeated-lock-or-change-mutated-hero"));
				UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: LockedPreserved role=%d player=%d hero=%s locked=1"), Role, PS->GetPlayerId(), *Target.ToString());
				NextCase(6);
				if (Role == 3)
				{
					UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: Disconnect role=3 player=%d phase=selection"), OriginalClientId);
					Advance(20);
					Browser.Reset();
					World->GetGameInstance()->ReturnToMainMenu();
					return true;
				}
			}
			else if (Case == 10)
			{
				if (PS->GetPlayerId() == OriginalClientId) return Reject(TEXT("rejoin-reused-old-player-state"));
				PC->Server_SubmitPreSelectedHero(Target, true);
				NextCase(11);
			}
			else if (Case == 11 && Now - CaseSince >= 2.0)
			{
				if (!PS->IsHeroLocked() || PS->GetSelectedHeroId() != Target) return Reject(TEXT("rejoin-lock-failed"));
				UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: Rejoined role=3 oldPlayer=%d player=%d hero=%s locked=1"), OriginalClientId, PS->GetPlayerId(), *Target.ToString());
				NextCase(12);
			}
			return true;
		}
		if (World->GetMapName().Contains(TEXT("L_BW_Dorado")) && GS->GetRoundManagement()
			&& GS->GetRoundManagement()->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && GameplayStarted == 0.0)
		{
			if (GameplayObserved == 0.0) GameplayObserved = Now;
			if (!Roster(World, true))
			{
				if (Now - GameplayObserved > 20.0) return Reject(TEXT("gameplay-roster-timeout"));
				return true;
			}
			TSet<FPrimaryAssetId> TeamHeroes[2];
			bool Unique = true;
			for (APlayerState* Peer : GS->PlayerArray)
			{
				auto* BPS = Cast<ABwayPlayerState>(Peer);
				if (!BPS || BPS->IsInactive()) continue;
				const int32 Team = GS->GetPlayerTeam(BPS);
				if (Team < 0 || Team > 1) return Reject(TEXT("invalid-gameplay-team"));
				Unique &= !TeamHeroes[Team].Contains(BPS->GetSelectedHeroId());
				TeamHeroes[Team].Add(BPS->GetSelectedHeroId());
			}
			if (!Unique)
			{
				if (!LoggedTransientRoster) { LogRoster(World, TEXT("unsettled")); LoggedTransientRoster = true; }
				if (Now - GameplayObserved > 10.0) return Reject(TEXT("duplicate-gameplay-team-hero"));
				return true;
			}
			if (Now - GameplayObserved < 2.0) return true;
			if (PS->GetSelectedHeroId() != Target || !PS->IsHeroLocked() || (!Host && Case != (Role == 3 ? 12 : 6))) return Reject(TEXT("selection-did-not-survive-travel"));
			LogRoster(World, TEXT("playing"));
			UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: Gameplay role=%d player=%d hero=%s unique=1 players=8"), Role, PS->GetPlayerId(), *Target.ToString());
			GameplayStarted = Now;
		}
		if (GameplayStarted > 0.0 && Now - GameplayStarted >= 15.0)
		{
			RestoreLimit();
			UE_LOG(LogTemp, Display, TEXT("BwaySelectionEdges: PASS role=%d unique=1 requests=1 disconnect=%d"), Role, Host ? RemovedSelectionVerified : Role == 3 ? Rejoined : false);
			Active = false;
			if (!Host) FPlatformMisc::RequestExit(false);
			return false;
		}
		return true;
	}
};

void StartSelection(int32 Role)
{
	if (Active || Role < 0 || Role > 3) return;
	Active = true;
	TSharedRef<FSelectionProbe> Probe = MakeShared<FSelectionProbe>();
	Probe->Role = Role;
	Probe->Host = Role == 0;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Delta) { return Probe->Tick(Delta); }), 0.5f);
}
void StartSelectionClient(const TArray<FString>& Args)
{
	StartSelection(Args.Num() == 1 ? FCString::Atoi(*Args[0]) : -1);
}
FAutoConsoleCommand SelectionHostCommand(TEXT("bway.Test.SelectionEdgesHost"), TEXT("Development: hold selection for remote validation and disconnect checks."), FConsoleCommandDelegate::CreateStatic(&StartSelection, 0));
FAutoConsoleCommand SelectionClientCommand(TEXT("bway.Test.SelectionEdgesClient"), TEXT("Development: remote selection checks; argument is client role 1, 2 or 3."), FConsoleCommandWithArgsDelegate::CreateStatic(&StartSelectionClient));

void Start(bool Host, bool Reconnect)
{
	if (Active) return;
	Active = true;
	TSharedRef<FProbe> Probe = MakeShared<FProbe>();
	Probe->Host = Host;
	Probe->Reconnect = Reconnect;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Delta) { return Probe->Tick(Delta); }), 0.5f);
}
FAutoConsoleCommand HostCommand(TEXT("bway.Test.LANBrowserHost"), TEXT("Development: host LAN, observe roster/results, return and rehost."), FConsoleCommandDelegate::CreateStatic(&Start, true, false));
FAutoConsoleCommand ClientCommand(TEXT("bway.Test.LANBrowserClient"), TEXT("Development: actual browser search/entry join and connected return."), FConsoleCommandDelegate::CreateStatic(&Start, false, false));
FAutoConsoleCommand ReconnectHostCommand(TEXT("bway.Test.LANBrowserReconnectHost"), TEXT("Development: host LAN through client disconnect and rediscovery."), FConsoleCommandDelegate::CreateStatic(&Start, true, true));
FAutoConsoleCommand ReconnectClientCommand(TEXT("bway.Test.LANBrowserReconnectClient"), TEXT("Development: disconnect fully, recover frontend and rejoin through a fresh browser search."), FConsoleCommandDelegate::CreateStatic(&Start, false, true));
}
#endif
