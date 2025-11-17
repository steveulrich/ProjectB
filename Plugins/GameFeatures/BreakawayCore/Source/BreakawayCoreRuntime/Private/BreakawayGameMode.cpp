// Copyright Epic Games, Inc. All Rights Reserved.

#include "BreakawayGameMode.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "BwayCharacterWithAbilities.h"
#include "Relic/RelicActor.h"
#include "SpawnSystem/BwaySpawnPoint.h"
#include "SpawnSystem/BwaySpawnPointManagerComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayTagsManager.h"

DEFINE_LOG_CATEGORY(LogBreakawayGame);

ABreakawayGameMode::ABreakawayGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Set default game state class
	GameStateClass = ABwayGameState::StaticClass();
	PlayerStateClass = ABwayPlayerState::StaticClass();
}

void ABreakawayGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	
	UE_LOG(LogBreakawayGame, Log, TEXT("Initializing Breakaway Game Mode"));
}

void ABreakawayGameMode::InitGameState()
{
	Super::InitGameState();

	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (BwayGS)
	{
		// Add spawn point manager component to game state
		SpawnPointManager = Cast<UBwaySpawnPointManagerComponent>(
			BwayGS->AddComponentByClass(UBwaySpawnPointManagerComponent::StaticClass(), false, FTransform::Identity, false));

		if (SpawnPointManager)
		{
			SpawnPointManager->RegisterComponent();
			UE_LOG(LogBreakawayGame, Log, TEXT("Spawn Point Manager added to Game State"));
		}

		UE_LOG(LogBreakawayGame, Log, TEXT("Breakaway Game State initialized"));
	}
}

void ABreakawayGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogBreakawayGame, Warning, TEXT("Breakaway GameMode Loaded"));

	// Initialize spawn point tags
	InitializeSpawnPointTags();

	// Spawn initial game objects
	SpawnInitialGameObjects();

	// Start first round after delay if configured
	if (bAutoStartFirstRound)
	{
		FTimerHandle UnusedHandle;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &ABreakawayGameMode::StartRound, PreRoundDelay, false);
		UE_LOG(LogBreakawayGame, Log, TEXT("First round will start in %.1f seconds"), PreRoundDelay);
	}

	bGameInitialized = true;
}

void ABreakawayGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS || BwayGS->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return;
	}

	// Check if round time has expired
	if (BwayGS->GetRoundTimeRemaining() <= 0)
	{
		OnRoundTimerExpired();
	}
}

void ABreakawayGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer)
	{
		return;
	}

	// Assign player to a team
	AssignPlayerToTeam(NewPlayer);

	ABwayPlayerState* PS = NewPlayer->GetPlayerState<ABwayPlayerState>();
	if (PS && PS->GetSelectedHeroId().IsValid())
	{
		// Hero spawning is handled by HandleStartingNewPlayer in base implementation
		UE_LOG(LogBreakawayGame, Log, TEXT("Player %s logged in with hero %s"), 
			*NewPlayer->GetName(), *PS->GetSelectedHeroId().ToString());
	}
	else
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("Player %s logged in without a selected hero"), 
			*NewPlayer->GetName());
	}
}

void ABreakawayGameMode::Logout(AController* Exiting)
{
	if (APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		ABwayGameState* BwayGS = GetBreakawayGameState();
		if (BwayGS && PC->PlayerState)
		{
			BwayGS->RemovePlayerFromTeam(PC->PlayerState);
		}

		// Cancel any pending respawn timers
		if (RespawnTimers.Contains(Exiting))
		{
			GetWorldTimerManager().ClearTimer(RespawnTimers[Exiting]);
			RespawnTimers.Remove(Exiting);
		}
	}

	Super::Logout(Exiting);
}

AActor* ABreakawayGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS || !Player || !Player->PlayerState)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// Determine which team the player is on
	const int32 TeamIndex = BwayGS->GetPlayerTeam(Player->PlayerState);
	
	// Use spawn point manager if available
	if (SpawnPointManager)
	{
		// Get spawn points for the player's team
		TArray<ABwaySpawnPoint*> TeamSpawnPoints = SpawnPointManager->GetSpawnPointsByTeam(TeamIndex);
		
		if (TeamSpawnPoints.Num() > 0)
		{
			// Choose random spawn point
			const int32 RandomIndex = FMath::RandRange(0, TeamSpawnPoints.Num() - 1);
			ABwaySpawnPoint* ChosenSpawnPoint = TeamSpawnPoints[RandomIndex];
			
			// Return a dummy player start actor at the spawn point location
			// Note: In a full implementation, you might want to create actual PlayerStart actors
			// or return the spawn point itself if it inherits from PlayerStart
			UE_LOG(LogBreakawayGame, Log, TEXT("Using spawn point manager for player spawn"));
			
			// For now, we'll still fall back to traditional PlayerStarts
			// but you could extend ABwaySpawnPoint to inherit from APlayerStart
		}
	}

	// Fallback to tag-based player start search
	FName SpawnTag = (TeamIndex == 0) ? Team1SpawnPointTag : Team2SpawnPointTag;
	TArray<AActor*> TeamSpawns = GetPlayerStartsWithTag(SpawnTag);

	if (TeamSpawns.Num() > 0)
	{
		// Return a random spawn from the team's spawns
		const int32 RandomIndex = FMath::RandRange(0, TeamSpawns.Num() - 1);
		return TeamSpawns[RandomIndex];
	}

	// Fall back to default behavior if no tagged spawns found
	UE_LOG(LogBreakawayGame, Warning, TEXT("No spawn points found with tag %s, using default spawn"), *SpawnTag.ToString());
	return Super::ChoosePlayerStart_Implementation(Player);
}

// ========================================
// Round Management
// ========================================

void ABreakawayGameMode::StartRound()
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS)
	{
		UE_LOG(LogBreakawayGame, Error, TEXT("Cannot start round - GameState is null"));
		return;
	}

	UE_LOG(LogBreakawayGame, Log, TEXT("Starting Round %d"), BwayGS->GetCurrentRoundNumber() + 1);

	// Reset round state
	ResetRoundState();

	// Set state to active
	BwayGS->SetRoundState(ERoundState::RoundActive);
}

void ABreakawayGameMode::EndRound(int32 WinningTeam, EWinCondition WinCondition)
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS)
	{
		return;
	}

	// Set state to ending
	BwayGS->SetRoundState(ERoundState::RoundEnding);

	// Award point to winning team
	BwayGS->AddScore(WinningTeam, 1);

	// Log the win condition
	FString WinConditionStr;
	switch (WinCondition)
	{
	case EWinCondition::GoalScored:
		WinConditionStr = TEXT("Goal Scored");
		break;
	case EWinCondition::TeamEliminated:
		WinConditionStr = TEXT("Team Eliminated");
		break;
	case EWinCondition::TimeExpired:
		WinConditionStr = TEXT("Time Expired - Possession");
		break;
	default:
		WinConditionStr = TEXT("Unknown");
		break;
	}

	UE_LOG(LogBreakawayGame, Log, TEXT("Round ended! Team %d wins via %s"), 
		WinningTeam + 1, *WinConditionStr);

	// Check if match is over
	if (CheckMatchEnd())
	{
		UE_LOG(LogBreakawayGame, Log, TEXT("Match Over! Team %d wins!"), WinningTeam + 1);
		BwayGS->SetRoundState(ERoundState::RoundComplete);
		return;
	}

	// Set state to complete
	BwayGS->SetRoundState(ERoundState::RoundComplete);

	// Schedule next round
	FTimerHandle UnusedHandle;
	GetWorldTimerManager().SetTimer(UnusedHandle, this, &ABreakawayGameMode::StartRound, 
		BwayGS->RoundEndDelay, false);
}

void ABreakawayGameMode::ResetRoundState()
{
	// Reset relic using spawn point manager
	if (SpawnPointManager)
	{
		SpawnPointManager->ResetAllSpawnPoints(RelicSpawnTag);
	}
	else
	{
		// Fallback to old method
		ResetRelic();
	}

	// Respawn all players
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			RespawnPlayer(PC);
		}
	}

	// Reset buildables if needed (TODO: Implement buildable cleanup)
}

// ========================================
// Scoring & Win Conditions
// ========================================

void ABreakawayGameMode::OnRelicScored(int32 ScoringTeam)
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS || BwayGS->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return;
	}

	UE_LOG(LogBreakawayGame, Log, TEXT("Goal! Team %d scored with the relic"), ScoringTeam + 1);

	// End round with goal scored win condition
	EndRound(ScoringTeam, EWinCondition::GoalScored);
}

void ABreakawayGameMode::CheckTeamElimination()
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS || BwayGS->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return;
	}

	// Check if either team has no alive players
	for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
	{
		const FTeamInfo& TeamInfo = BwayGS->GetTeamInfo(TeamIndex);
		if (TeamInfo.TeamMembers.Num() > 0 && TeamInfo.AlivePlayerCount == 0)
		{
			// This team is eliminated, other team wins
			const int32 WinningTeam = (TeamIndex == 0) ? 1 : 0;
			UE_LOG(LogBreakawayGame, Log, TEXT("Team %d eliminated! Team %d wins the round"), 
				TeamIndex + 1, WinningTeam + 1);
			
			EndRound(WinningTeam, EWinCondition::TeamEliminated);
			return;
		}
	}
}

void ABreakawayGameMode::OnRoundTimerExpired()
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS || BwayGS->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return;
	}

	UE_LOG(LogBreakawayGame, Log, TEXT("Round timer expired! Checking relic possession..."));

	// Determine which team has possession based on relic location
	const int32 PossessingTeam = DetermineRelicPossessionTeam();

	if (PossessingTeam >= 0)
	{
		UE_LOG(LogBreakawayGame, Log, TEXT("Team %d had possession when time expired"), PossessingTeam + 1);
		EndRound(PossessingTeam, EWinCondition::TimeExpired);
	}
	else
	{
		// Relic is neutral - nobody wins, start new round
		UE_LOG(LogBreakawayGame, Log, TEXT("Relic was neutral when time expired - no winner, starting new round"));
		BwayGS->SetRoundState(ERoundState::RoundComplete);
		
		FTimerHandle UnusedHandle;
		GetWorldTimerManager().SetTimer(UnusedHandle, this, &ABreakawayGameMode::StartRound, 
			BwayGS->RoundEndDelay, false);
	}
}

bool ABreakawayGameMode::CheckMatchEnd()
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS)
	{
		return false;
	}

	// Check if either team reached winning score
	for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
	{
		if (BwayGS->GetTeamScore(TeamIndex) >= BwayGS->PointsToWin)
		{
			return true;
		}
	}

	return false;
}

// ========================================
// Team Management
// ========================================

void ABreakawayGameMode::AssignPlayerToTeam(APlayerController* Player)
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS || !Player || !Player->PlayerState)
	{
		return;
	}

	// Check if already on a team
	if (BwayGS->GetPlayerTeam(Player->PlayerState) >= 0)
	{
		return;
	}

	// Assign to team with fewer players
	const int32 TeamIndex = GetTeamWithFewerPlayers();
	BwayGS->AddPlayerToTeam(Player->PlayerState, TeamIndex);

	UE_LOG(LogBreakawayGame, Log, TEXT("Assigned player %s to Team %d"), 
		*Player->GetName(), TeamIndex + 1);
}

int32 ABreakawayGameMode::GetTeamWithFewerPlayers() const
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS)
	{
		return 0;
	}

	const FTeamInfo& Team1 = BwayGS->GetTeamInfo(0);
	const FTeamInfo& Team2 = BwayGS->GetTeamInfo(1);

	// Return team with fewer players, or Team 1 if equal
	return (Team1.TeamMembers.Num() <= Team2.TeamMembers.Num()) ? 0 : 1;
}

// ========================================
// Relic Management
// ========================================

void ABreakawayGameMode::ResetRelic()
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS)
	{
		return;
	}

	ARelicActor* Relic = BwayGS->GetRelicActor();
	if (!Relic)
	{
		UE_LOG(LogBreakawayGame, Error, TEXT("Cannot reset relic - no relic actor found"));
		return;
	}

	// Use spawn point manager if available
	if (SpawnPointManager)
	{
		ABwaySpawnPoint* RelicSpawn = SpawnPointManager->GetRandomSpawnPoint(RelicSpawnTag);
		if (RelicSpawn)
		{
			Relic->SetActorLocation(RelicSpawn->GetActorLocation());
			Relic->SetActorRotation(RelicSpawn->GetActorRotation());
			Relic->OnDropped();
			
			UE_LOG(LogBreakawayGame, Log, TEXT("Relic reset to spawn point: %s"), *RelicSpawn->GetName());
			return;
		}
	}

	// Fallback to tag-based spawn search
	TArray<AActor*> RelicSpawns = GetPlayerStartsWithTag(RelicSpawnPointTag);
	if (RelicSpawns.Num() > 0)
	{
		AActor* SpawnPoint = RelicSpawns[0];
		Relic->SetActorLocation(SpawnPoint->GetActorLocation());
		Relic->SetActorRotation(SpawnPoint->GetActorRotation());
		Relic->OnDropped();
		
		UE_LOG(LogBreakawayGame, Log, TEXT("Relic reset to spawn location"));
	}
	else
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("No relic spawn points found"));
	}
}

void ABreakawayGameMode::OnRelicCarrierChanged(ABwayCharacterWithAbilities* NewCarrier)
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS)
	{
		return;
	}

	if (NewCarrier && NewCarrier->GetPlayerState())
	{
		const int32 TeamIndex = BwayGS->GetPlayerTeam(NewCarrier->GetPlayerState());
		BwayGS->SetRelicPossessingTeam(TeamIndex);
	}
	else
	{
		BwayGS->SetRelicPossessingTeam(-1); // Neutral
	}
}

int32 ABreakawayGameMode::DetermineRelicPossessionTeam() const
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS)
	{
		return -1;
	}

	ARelicActor* Relic = BwayGS->GetRelicActor();
	if (!Relic)
	{
		return -1;
	}

	// If relic has a carrier, use their team
	if (Relic->CurrentCarrier && Relic->CurrentCarrier->GetPlayerState())
	{
		return BwayGS->GetPlayerTeam(Relic->CurrentCarrier->GetPlayerState());
	}

	// Otherwise use last possessing team from game state
	return BwayGS->GetRelicPossessingTeam();
}

// ========================================
// Player Management
// ========================================

void ABreakawayGameMode::OnPlayerDied(AController* VictimController, AController* KillerController)
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS || !VictimController || !VictimController->PlayerState)
	{
		return;
	}

	// Update game state
	BwayGS->OnPlayerDied(VictimController->PlayerState);

	UE_LOG(LogBreakawayGame, Log, TEXT("Player %s died"), 
		*VictimController->PlayerState->GetPlayerName());

	// Check for team elimination
	CheckTeamElimination();

	// Schedule respawn
	FTimerHandle& RespawnTimer = RespawnTimers.FindOrAdd(VictimController);
	GetWorldTimerManager().SetTimer(RespawnTimer, 
		[this, VictimController]() { RespawnPlayer(VictimController); }, 
		RespawnDelay, false);
}

void ABreakawayGameMode::RespawnPlayer(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	ABwayGameState* BwayGS = GetBreakawayGameState();
	APlayerController* PC = Cast<APlayerController>(Controller);
	
	if (!PC || !PC->PlayerState)
	{
		return;
	}

	// Destroy old pawn if exists
	if (APawn* OldPawn = Controller->GetPawn())
	{
		OldPawn->Destroy();
	}

	// Spawn new pawn using RestartPlayer
	RestartPlayer(Controller);

	// Update game state
	if (BwayGS)
	{
		BwayGS->OnPlayerRespawned(PC->PlayerState);
	}

	// Clear respawn timer
	if (RespawnTimers.Contains(Controller))
	{
		RespawnTimers.Remove(Controller);
	}

	UE_LOG(LogBreakawayGame, Log, TEXT("Player %s respawned"), 
		*PC->PlayerState->GetPlayerName());
}

// ========================================
// Spawn System Integration
// ========================================

void ABreakawayGameMode::InitializeSpawnPointTags()
{
	// Initialize gameplay tags for spawn points
	RelicSpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Relic"));
	Goal1SpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Goal.Team1"));
	Goal2SpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Goal.Team2"));
	
	UE_LOG(LogBreakawayGame, Log, TEXT("Initialized spawn point tags"));
}

void ABreakawayGameMode::SpawnInitialGameObjects()
{
	if (!SpawnPointManager)
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("No spawn point manager - cannot spawn initial objects"));
		return;
	}

	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS)
	{
		return;
	}

	// Spawn relic
	TArray<AActor*> SpawnedRelics = SpawnPointManager->SpawnObjectsAtPoints(RelicSpawnTag);
	if (SpawnedRelics.Num() > 0)
	{
		if (ARelicActor* Relic = Cast<ARelicActor>(SpawnedRelics[0]))
		{
			BwayGS->SetRelicActor(Relic);
			UE_LOG(LogBreakawayGame, Log, TEXT("Spawned relic at spawn point"));
		}
	}
	else
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("No relic spawned - check spawn points and spawn data"));
	}

	// Spawn goals
	SpawnPointManager->SpawnObjectsAtPoints(Goal1SpawnTag);
	SpawnPointManager->SpawnObjectsAtPoints(Goal2SpawnTag);
	
	UE_LOG(LogBreakawayGame, Log, TEXT("Spawned initial game objects"));
}

// ========================================
// Helper Functions
// ========================================

ABwayGameState* ABreakawayGameMode::GetBreakawayGameState() const
{
	return GetGameState<ABwayGameState>();
}

TArray<AActor*> ABreakawayGameMode::GetPlayerStartsWithTag(const FName& Tag) const
{
	TArray<AActor*> TaggedStarts;

	if (!GetWorld())
	{
		return TaggedStarts;
	}

	// Find all PlayerStart actors
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		if (Start && Start->PlayerStartTag == Tag)
		{
			TaggedStarts.Add(Start);
		}
	}

	return TaggedStarts;
}
