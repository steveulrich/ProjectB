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
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameState/BwayRoundManagementComponent.h"

ABwayGameState::ABwayGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	HeroSelectionManager = CreateDefaultSubobject<UBwayHeroSelectionManager>(TEXT("HeroSelectionManager"));
	HeroSelectionPhaseComponent = CreateDefaultSubobject<UBwayHeroSelectionPhaseComponent>(TEXT("HeroSelectionPhaseComponent"));
	CreateDefaultSubobject<UBwayBotCreationComponent>(TEXT("BotCreationComponent"));
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

	// Listen for match end to transition to PostGame
	if (UBwayRoundManagementComponent* RoundMgmt = FindComponentByClass<UBwayRoundManagementComponent>())
	{
		RoundMgmt->OnMatchEnded.AddDynamic(this, &ABwayGameState::HandleMatchEnded);
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
	return FindComponentByClass<UBwayRoundManagementComponent>();
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

	UE_LOG(LogTemp, Warning, TEXT("BwayGameState: MATCH ENDED — Team %d wins! Transitioning to PostGame."), WinningTeam + 1);

	// Broadcast match state change
	OnMatchStateChanged.Broadcast(FName("MatchComplete"));

	TransitionToPostGame(WinningTeam);
}

void ABwayGameState::TransitionToPostGame(int32 WinningTeam)
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayGameState: Transitioning to PostGame phase"));

	// Start PostGame phase via Lyra
	if (PostGamePhaseAbilityClass)
	{
		if (UWorld* World = GetWorld())
		{
			if (ULyraGamePhaseSubsystem* PhaseSubsystem = World->GetSubsystem<ULyraGamePhaseSubsystem>())
			{
				PhaseSubsystem->StartPhase(PostGamePhaseAbilityClass);
				UE_LOG(LogTemp, Log, TEXT("BwayGameState: Started PostGame phase via %s"),
					*PostGamePhaseAbilityClass->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayGameState: PostGamePhaseAbilityClass is not set!"));
	}

	// Show results screen to all players
	ShowResultsScreen(WinningTeam);
}

void ABwayGameState::ShowResultsScreen_Implementation(int32 WinningTeam)
{
	UE_LOG(LogTemp, Log, TEXT("BwayGameState: ShowResultsScreen base impl (override in Blueprint)"));

	if (ResultsScreenWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayGameState: No ResultsScreenWidgetClass configured"));
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(It->Get()))
		{
			BwayPC->Client_ShowResults(WinningTeam, ResultsScreenWidgetClass);
			UE_LOG(LogTemp, Log, TEXT("BwayGameState: Sent results screen RPC to %s"), *BwayPC->GetName());
		}
	}
}

void ABwayGameState::ReturnToFrontEnd()
{
	UE_LOG(LogTemp, Log, TEXT("BwayGameState: Returning to front-end"));

	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayGameState: ReturnToFrontEnd called on non-authority! Ignoring."));
		return;
	}

	// Prefer the FrontEndLevel set on the BP defaults; fall back to the Lyra frontend map
	// so the FE -> Match -> FE loop always works even if content hasn't wired an override yet.
	FString TravelURL;
	if (!FrontEndLevel.IsNull())
	{
		TravelURL = FrontEndLevel.GetLongPackageName();
	}
	else
	{
		TravelURL = TEXT("/Game/System/FrontEnd/Maps/L_LyraFrontEnd");
		UE_LOG(LogTemp, Warning, TEXT("BwayGameState: FrontEndLevel not set; falling back to Lyra frontend map %s"), *TravelURL);
	}

	if (UWorld* World = GetWorld())
	{
		World->ServerTravel(TravelURL + TEXT("?listen"), true);
		UE_LOG(LogTemp, Log, TEXT("BwayGameState: ServerTravel to %s"), *TravelURL);
	}
}