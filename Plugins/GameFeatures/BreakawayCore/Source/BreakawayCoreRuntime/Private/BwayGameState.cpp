// Copyright Epic Games, Inc. All Rights Reserved.

#include "BwayGameState.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "GameState/BwayBotCreationComponent.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayMidfieldDividerComponent.h"
#include "GameState/BwayTeamBridgeComponent.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "Combat/BwayCombatFeedbackRouterComponent.h"
#include "CommonSessionSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "Stats/BwayMatchStatsLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "IPAddress.h"

ABwayGameState::ABwayGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	HeroSelectionManager = CreateDefaultSubobject<UBwayHeroSelectionManager>(TEXT("HeroSelectionManager"));
	HeroSelectionPhaseComponent = CreateDefaultSubobject<UBwayHeroSelectionPhaseComponent>(TEXT("HeroSelectionPhaseComponent"));
	RoundManagementComponent = CreateDefaultSubobject<UBwayRoundManagementComponent>(TEXT("RoundManagementComponent"));
	ScoringComponent = CreateDefaultSubobject<UBwayScoringComponent>(TEXT("ScoringComponent"));
	RelicManagerComponent = CreateDefaultSubobject<UBwayRelicManagerComponent>(TEXT("RelicManagerComponent"));
	MidfieldDividerComponent = CreateDefaultSubobject<UBwayMidfieldDividerComponent>(TEXT("MidfieldDividerComponent"));
	TeamBridgeComponent = CreateDefaultSubobject<UBwayTeamBridgeComponent>(TEXT("TeamBridgeComponent"));
	BuildableRegistryComponent = CreateDefaultSubobject<UBwayBuildableRegistryComponent>(TEXT("BuildableRegistryComponent"));
	CombatFeedbackRouterComponent = CreateDefaultSubobject<UBwayCombatFeedbackRouterComponent>(TEXT("CombatFeedbackRouterComponent"));
}

void ABwayGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);
	if (HasAuthority() && HeroSelectionManager && HeroSelectionManager->IsSelectionActive())
	{
		HeroSelectionManager->RegisterPlayer(PlayerState);
	}
}

void ABwayGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);
	if (HasAuthority() && HeroSelectionManager)
	{
		HeroSelectionManager->UnregisterPlayer(PlayerState);
	}
}

void ABwayGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABwayGameState, Teams);
}

void ABwayGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (HasAuthority())
	{
		InitializeTeams();
	}

	// Listen for match end — presentation only (PostMatch GAS phase owned by RoundManagement).
	if (RoundManagementComponent)
	{
		RoundManagementComponent->OnMatchEnded.AddDynamic(this, &ABwayGameState::HandleMatchEnded);
	}
}

void ABwayGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Round timing is now handled by UBwayRoundManagementComponent::TickComponent()
}

void ABwayGameState::InitializeTeams()
{
	if (!HasAuthority())
	{
		return;
	}

	// Initialize two teams
	Teams.Empty();
	Teams.Add(FTeamInfo(0)); // Team 1
	Teams.Add(FTeamInfo(1)); // Team 2
}

// ========================================
// Team Management
// ========================================

const FTeamInfo& ABwayGameState::GetTeamInfo(int32 TeamIndex) const
{
	static FTeamInfo EmptyTeam;
	
	if (Teams.IsValidIndex(TeamIndex))
	{
		return Teams[TeamIndex];
	}
	
	return EmptyTeam;
}

int32 ABwayGameState::GetPlayerTeam(const APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return -1;
	}

	for (int32 TeamIndex = 0; TeamIndex < Teams.Num(); ++TeamIndex)
	{
		if (Teams[TeamIndex].TeamMembers.Contains(PlayerState))
		{
			return TeamIndex;
		}
	}

	return -1;
}

void ABwayGameState::AddPlayerToTeam(APlayerState* PlayerState, int32 TeamIndex)
{
	if (!HasAuthority() || !PlayerState || !Teams.IsValidIndex(TeamIndex))
	{
		return;
	}

	// Remove from any existing team first
	RemovePlayerFromTeam(PlayerState);

	// Add to new team
	Teams[TeamIndex].TeamMembers.AddUnique(PlayerState);
	Teams[TeamIndex].AlivePlayerCount = Teams[TeamIndex].TeamMembers.Num();

	if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState))
	{
		// Offset by 1 since Breakaway uses indices 0/1 but Lyra expects TeamIDs 1/2
		BwayPS->SetGenericTeamId(FGenericTeamId(TeamIndex + 1));
	}

	UE_LOG(LogTemp, Log, TEXT("Added player %s to Team %d"), *PlayerState->GetPlayerName(), TeamIndex + 1);
	if (HeroSelectionManager)
	{
		HeroSelectionManager->SynchronizePlayerSelectionState(PlayerState);
	}

	OnTeamsUpdated.Broadcast();
}

void ABwayGameState::RemovePlayerFromTeam(APlayerState* PlayerState)
{
	if (!HasAuthority() || !PlayerState)
	{
		return;
	}

	for (FTeamInfo& Team : Teams)
	{
		if (Team.TeamMembers.Remove(PlayerState) > 0)
		{
			Team.AlivePlayerCount = FMath::Min(Team.AlivePlayerCount, Team.TeamMembers.Num());
			UE_LOG(LogTemp, Log, TEXT("Removed player %s from Team %d"), *PlayerState->GetPlayerName(), Team.TeamIndex + 1);
			
			if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState))
			{
				BwayPS->SetGenericTeamId(FGenericTeamId::NoTeam);
			}

			OnTeamsUpdated.Broadcast();
		}
	}
}

bool ABwayGameState::AreOnSameTeam(const AActor* ActorA, const AActor* ActorB) const
{
	if (!ActorA || !ActorB)
	{
		return false;
	}

	const int32 TeamA = GetTeamIndexForActor(ActorA);
	const int32 TeamB = GetTeamIndexForActor(ActorB);

	return (TeamA != -1 && TeamA == TeamB);
}

int32 ABwayGameState::GetTeamIndexForActor(const AActor* Actor) const
{
	if (!Actor)
	{
		return -1;
	}

	// Try to get team from character
	if (const ABwayCharacterWithAbilities* Character = Cast<ABwayCharacterWithAbilities>(Actor))
	{
		if (const APlayerState* PS = Character->GetPlayerState())
		{
			return GetPlayerTeam(PS);
		}
	}

	// Try to get team from player controller
	if (const APlayerController* PC = Cast<APlayerController>(Actor))
	{
		if (const APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			return GetPlayerTeam(PS);
		}
	}

	// Try to get team from player state directly
	if (const APlayerState* PS = Cast<APlayerState>(Actor))
	{
		return GetPlayerTeam(PS);
	}

	return -1;
}

// ========================================
// Round State Management
// ========================================

UBwayRoundManagementComponent* ABwayGameState::GetRoundManagement() const
{
	return RoundManagementComponent;
}

bool ABwayGameState::IsCanonicalRoundManagement(const UBwayRoundManagementComponent* Component) const
{
	return Component != nullptr && RoundManagementComponent == Component;
}

ERoundState ABwayGameState::GetCurrentRoundState() const
{
	if (const UBwayRoundManagementComponent* RoundMgmt = GetRoundManagement())
	{
		return RoundMgmt->GetCurrentRoundState();
	}
	return ERoundState::WaitingToStart;
}

int32 ABwayGameState::GetRoundTimeRemaining() const
{
	if (const UBwayRoundManagementComponent* RoundMgmt = GetRoundManagement())
	{
		return RoundMgmt->GetRoundTimeRemaining();
	}
	return 0;
}

int32 ABwayGameState::GetCurrentRoundNumber() const
{
	if (const UBwayRoundManagementComponent* RoundMgmt = GetRoundManagement())
	{
		return RoundMgmt->GetCurrentRoundNumber();
	}
	return 0;
}

// ========================================
// Player Tracking
// ========================================

void ABwayGameState::UpdateTeamAliveCount(int32 TeamIndex, int32 AliveCount)
{
	if (!HasAuthority() || !Teams.IsValidIndex(TeamIndex))
	{
		return;
	}

	Teams[TeamIndex].AlivePlayerCount = FMath::Clamp(AliveCount, 0, Teams[TeamIndex].TeamMembers.Num());
}

void ABwayGameState::OnPlayerDied(APlayerState* PlayerState)
{
	if (!HasAuthority() || !PlayerState)
	{
		return;
	}

	const int32 TeamIndex = GetPlayerTeam(PlayerState);
	if (Teams.IsValidIndex(TeamIndex))
	{
		Teams[TeamIndex].AlivePlayerCount = FMath::Max(0, Teams[TeamIndex].AlivePlayerCount - 1);
		UE_LOG(LogTemp, Log, TEXT("Player %s died. Team %d alive count: %d"), 
			*PlayerState->GetPlayerName(), TeamIndex + 1, Teams[TeamIndex].AlivePlayerCount);
	}
}

void ABwayGameState::OnPlayerRespawned(APlayerState* PlayerState)
{
	if (!HasAuthority() || !PlayerState)
	{
		return;
	}

	const int32 TeamIndex = GetPlayerTeam(PlayerState);
	if (Teams.IsValidIndex(TeamIndex))
	{
		Teams[TeamIndex].AlivePlayerCount = FMath::Min(Teams[TeamIndex].AlivePlayerCount + 1, Teams[TeamIndex].TeamMembers.Num());
		UE_LOG(LogTemp, Log, TEXT("Player %s respawned. Team %d alive count: %d"), 
			*PlayerState->GetPlayerName(), TeamIndex + 1, Teams[TeamIndex].AlivePlayerCount);
	}
}

void ABwayGameState::OnRep_TeamInfo()
{
	UE_LOG(LogTemp, Verbose, TEXT("Team info replicated"));
	OnTeamsUpdated.Broadcast();
}

// ========================================
// Match Flow Control
// ========================================

void ABwayGameState::HandleMatchEnded(int32 WinningTeam, int32 TotalRounds)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("BwayGameState: MATCH ENDED — Team %d wins after %d round(s). Showing results."),
		WinningTeam + 1, TotalRounds);

	OnMatchStateChanged.Broadcast(FName("MatchComplete"));

	ShowResultsScreen(WinningTeam);
}

void ABwayGameState::TransitionToPostGame(int32 WinningTeam)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayGameState: TransitionToPostGame — presentation only (PostMatch phase owned by RoundManagement)"));
	ShowResultsScreen(WinningTeam);
}

void ABwayGameState::ShowResultsScreen_Implementation(int32 WinningTeam)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayGameState: ShowResultsScreen base impl (override in Blueprint)"));

	if (ResultsScreenWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayGameState: No ResultsScreenWidgetClass configured"));
		return;
	}

	// Capture once on authority. PlayerState replication and controller RPCs can
	// arrive independently, so clients must not rebuild final totals from live state.
	if (!bHasFinalResultsSummary)
	{
		const FBwayResolvedMatchFlowSettings Settings = UBwayMatchFlowLibrary::ResolveMatchFlowSettings(this, nullptr, nullptr);
		FinalResultsSummary = UBwayMatchStatsLibrary::BuildPostMatchSummaryData(
			this, WinningTeam, GetCurrentRoundNumber(), FMath::Max(0.0f, Settings.PostMatchSummaryDuration));
		bHasFinalResultsSummary = true;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(It->Get()))
		{
			SendResultsToPlayer(BwayPC);
		}
	}
}

bool ABwayGameState::SendResultsToPlayer(ABwayPlayerController* Player)
{
	if (!HasAuthority() || !Player || !bHasFinalResultsSummary || ResultsScreenWidgetClass.IsNull()) return false;

	FBwayPostMatchSummaryData ViewerSummary = FinalResultsSummary;
	const APlayerState* Viewer = Player->PlayerState;
	ViewerSummary.LocalPlayerTeamIndex = INDEX_NONE;
	bool bParticipated = false;
	for (FBwayMatchBreakdownPlayerColumn& Column : ViewerSummary.PlayerColumns)
	{
		Column.bIsLocalPlayer = Viewer && Column.PlayerId == Viewer->GetPlayerId();
		if (Column.bIsLocalPlayer)
		{
			bParticipated = true;
			ViewerSummary.LocalPlayerTeamIndex = Column.GameTeamIndex;
		}
	}
	ViewerSummary.bLocalPlayerWon = bParticipated && ViewerSummary.WinningTeam >= 0
		&& ViewerSummary.LocalPlayerTeamIndex == ViewerSummary.WinningTeam;
	// A visitor did not win or lose this match. Show its unchanged breakdown directly.
	if (!bParticipated) ViewerSummary.InterstitialDurationSeconds = 0.0f;
	Player->Client_ShowResults(ViewerSummary, ResultsScreenWidgetClass);
	UE_LOG(LogTemp, Log, TEXT("BwayGameState: Sent retained results to %s (%d-%d, participant=%d)"),
		*Player->GetName(), ViewerSummary.Team0Score, ViewerSummary.Team1Score, bParticipated);
	return true;
}

void ABwayGameState::RestartMatchFromResults()
{
	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (!HasAuthority() || !GameMode || bResultsTravelPending ||
		!RoundManagementComponent || RoundManagementComponent->GetCurrentMatchPhase() != EBwayMatchPhase::PostMatch)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayGameState: Ignoring rematch request outside idle PostMatch"));
		return;
	}

	// ProcessServerTravel consumes removal tokens, then Browse reparses its result
	// against LastURL. Clear that base so the second parse cannot restore old flags.
	FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(World);
	const FURL PreviousBaseURL = WorldContext.LastURL;
	UBwayGameplayUrlLibrary::ResetMatchTravelOptions(WorldContext.LastURL);
	FString TravelURL = UWorld::RemovePIEPrefix(World->URL.Map) + TEXT("?NoSeamlessTravel");
	if (World->GetNetMode() == NM_ListenServer)
	{
		TravelURL += TEXT("?listen");
	}
	// PIE's LastURL may hold 7777 while its socket actually uses 17777.
	// ServerTravel forbids a port in its input, so normalize the relative-travel base.
	if (UNetDriver* NetDriver = World->GetNetDriver())
	{
		if (const TSharedPtr<const FInternetAddr> LocalAddress = NetDriver->GetLocalAddr())
		{
			WorldContext.LastURL.Port = LocalAddress->GetPort();
		}
	}

	bResultsTravelPending = true;
	if (!World->ServerTravel(TravelURL, false, false))
	{
		WorldContext.LastURL = PreviousBaseURL;
		bResultsTravelPending = false;
		UE_LOG(LogTemp, Error, TEXT("BwayGameState: Rematch travel rejected"));
		return;
	}
	if (UBwayBotCreationComponent* Bots = FindComponentByClass<UBwayBotCreationComponent>())
	{
		Bots->ShutdownAllBotsForTravel();
	}
	UE_LOG(LogTemp, Log, TEXT("BwayGameState: Rematch ServerTravel to %s"), *TravelURL);
}

void ABwayGameState::ReturnToFrontEnd()
{
	UE_LOG(LogTemp, Log, TEXT("BwayGameState: Returning to front-end"));

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayGameState: ReturnToFrontEnd called on non-authority! Ignoring."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (UBwayBotCreationComponent* BotCreation = FindComponentByClass<UBwayBotCreationComponent>())
	{
		BotCreation->ShutdownAllBotsForTravel();
	}

	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (UCommonSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UCommonSessionSubsystem>())
		{
			SessionSubsystem->CleanUpSessions();
		}
	}

	// 11-7 canonical target is Lyra front-end with its menu experience.
	static const TCHAR* LyraFrontEndMap = TEXT("/Game/System/FrontEnd/Maps/L_LyraFrontEnd");
	static const TCHAR* LyraFrontEndExperience = TEXT("B_LyraFrontEnd_Experience");

	FString MapPath = LyraFrontEndMap;
	if (!FrontEndLevel.IsNull())
	{
		const FString OverridePath = FrontEndLevel.GetLongPackageName();
		if (OverridePath.Contains(TEXT("L_LyraFrontEnd")))
		{
			MapPath = OverridePath;
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("BwayGameState: FrontEndLevel '%s' ignored for 11-7 — travelling to Lyra front-end %s"),
				*OverridePath, LyraFrontEndMap);
		}
	}

	const FString PieTravelOptions = FString::Printf(TEXT("Experience=%s"), LyraFrontEndExperience);
	const FString ServerTravelOptions = FString::Printf(TEXT("listen?Experience=%s"), LyraFrontEndExperience);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(It->Get()))
		{
			BwayPC->Client_DismissResultsScreen();
		}
	}

#if WITH_EDITOR
	// PIE: OpenLevel reloads the session in-process — avoids PendingNetGame reconnect to the old listen port.
	if (World->WorldType == EWorldType::PIE)
	{
		UGameplayStatics::OpenLevel(World, FName(*MapPath), /*bAbsolute=*/true, PieTravelOptions);
		UE_LOG(LogTemp, Log, TEXT("BwayGameState: OpenLevel (PIE WorldType, NetMode=%d) to %s?%s"),
			static_cast<int32>(World->GetNetMode()), *MapPath, *PieTravelOptions);
		return;
	}
#endif

	// Packaged / dedicated server: hard ServerTravel with listen (full context switch back to menu).
	const FString TravelURL = FString::Printf(TEXT("%s?%s"), *MapPath, *ServerTravelOptions);
	World->ServerTravel(TravelURL, /*bAbsolute=*/true, /*bShouldSkipGameNotify=*/false);
	UE_LOG(LogTemp, Log, TEXT("BwayGameState: ServerTravel to %s"), *TravelURL);
}
