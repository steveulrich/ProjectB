// Copyright Epic Games, Inc. All Rights Reserved.

#include "BwayGameState.h"
#include "Relic/RelicActor.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayPlayerState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"

ABwayGameState::ABwayGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ABwayGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABwayGameState, Teams);
	DOREPLIFETIME(ABwayGameState, CurrentRoundState);
	DOREPLIFETIME(ABwayGameState, RoundStartTime);
	DOREPLIFETIME(ABwayGameState, CurrentRoundNumber);
	DOREPLIFETIME(ABwayGameState, RelicActor);
	DOREPLIFETIME(ABwayGameState, RelicPossessingTeam);
}

void ABwayGameState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (HasAuthority())
	{
		InitializeTeams();
	}
}

void ABwayGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Broadcast round time updates every second
	if (HasAuthority() && CurrentRoundState == ERoundState::RoundActive)
	{
		const float CurrentTime = GetWorld()->GetTimeSeconds();
		if (CurrentTime - LastRoundTimeUpdateBroadcast >= RoundTimeUpdateInterval)
		{
			LastRoundTimeUpdateBroadcast = CurrentTime;
			const int32 RemainingSeconds = GetRoundTimeRemaining();
			OnRoundTimeChanged.Broadcast(RemainingSeconds);
		}
	}
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
	Teams[TeamIndex].AlivePlayerCount++;

	UE_LOG(LogTemp, Log, TEXT("Added player %s to Team %d"), *PlayerState->GetPlayerName(), TeamIndex + 1);
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
			Team.AlivePlayerCount = FMath::Max(0, Team.AlivePlayerCount - 1);
			UE_LOG(LogTemp, Log, TEXT("Removed player %s from Team %d"), *PlayerState->GetPlayerName(), Team.TeamIndex + 1);
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
// Score Management
// ========================================

void ABwayGameState::AddScore(int32 TeamIndex, int32 Points)
{
	if (!HasAuthority() || !Teams.IsValidIndex(TeamIndex))
	{
		return;
	}

	Teams[TeamIndex].Score += Points;
	
	UE_LOG(LogTemp, Log, TEXT("Team %d scored! New score: %d"), TeamIndex + 1, Teams[TeamIndex].Score);
	
	// Broadcast score change
	OnScoreChanged.Broadcast(Teams[0].Score, Teams[1].Score);
}

int32 ABwayGameState::GetTeamScore(int32 TeamIndex) const
{
	if (Teams.IsValidIndex(TeamIndex))
	{
		return Teams[TeamIndex].Score;
	}
	return 0;
}

void ABwayGameState::ResetScores()
{
	if (!HasAuthority())
	{
		return;
	}

	for (FTeamInfo& Team : Teams)
	{
		Team.Score = 0;
	}

	OnScoreChanged.Broadcast(0, 0);
	UE_LOG(LogTemp, Log, TEXT("All scores reset"));
}

// ========================================
// Round State Management
// ========================================

void ABwayGameState::SetRoundState(ERoundState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurrentRoundState != NewState)
	{
		const ERoundState OldState = CurrentRoundState;
		CurrentRoundState = NewState;

		// Handle state transitions
		switch (NewState)
		{
		case ERoundState::RoundActive:
			RoundStartTime = GetWorld()->GetTimeSeconds();
			CurrentRoundNumber++;
			UE_LOG(LogTemp, Log, TEXT("Round %d started"), CurrentRoundNumber);
			break;

		case ERoundState::RoundEnding:
			UE_LOG(LogTemp, Log, TEXT("Round ending..."));
			break;

		case ERoundState::RoundComplete:
			UE_LOG(LogTemp, Log, TEXT("Round complete"));
			break;

		default:
			break;
		}

		// Broadcast state change
		OnRep_RoundState();
	}
}

void ABwayGameState::OnRep_RoundState()
{
	// Broadcast to blueprints/UI
	OnRoundStateChanged.Broadcast(FName( *StaticEnum<ERoundState>()->GetNameStringByValue((int64)CurrentRoundState)));
	
	UE_LOG(LogTemp, Log, TEXT("Round state changed to: %s"), 
		*StaticEnum<ERoundState>()->GetNameStringByValue((int64)CurrentRoundState));
}

int32 ABwayGameState::GetRoundTimeRemaining() const
{
	if (CurrentRoundState != ERoundState::RoundActive)
	{
		return 0;
	}

	const float ElapsedTime = GetWorld()->GetTimeSeconds() - RoundStartTime;
	const float RemainingTime = FMath::Max(0.0f, RoundDuration - ElapsedTime);
	return FMath::CeilToInt(RemainingTime);
}

// ========================================
// Relic Tracking
// ========================================

void ABwayGameState::SetRelicActor(ARelicActor* NewRelic)
{
	if (!HasAuthority())
	{
		return;
	}

	RelicActor = NewRelic;
	UE_LOG(LogTemp, Log, TEXT("Relic actor set: %s"), *GetNameSafe(RelicActor));
}

void ABwayGameState::SetRelicPossessingTeam(int32 TeamIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	if (RelicPossessingTeam != TeamIndex)
	{
		RelicPossessingTeam = TeamIndex;
		OnRep_RelicPossessingTeam();
	}
}

void ABwayGameState::OnRep_RelicPossessingTeam()
{
	if (RelicPossessingTeam >= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Team %d now possesses the relic"), RelicPossessingTeam + 1);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Relic is now neutral"));
	}
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

	Teams[TeamIndex].AlivePlayerCount = AliveCount;
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
		Teams[TeamIndex].AlivePlayerCount++;
		UE_LOG(LogTemp, Log, TEXT("Player %s respawned. Team %d alive count: %d"), 
			*PlayerState->GetPlayerName(), TeamIndex + 1, Teams[TeamIndex].AlivePlayerCount);
	}
}

void ABwayGameState::OnRep_TeamInfo()
{
	// Teams changed, notify UI or other systems if needed
	UE_LOG(LogTemp, Verbose, TEXT("Team info replicated"));
}