// Copyright Epic Games, Inc. All Rights Reserved.

#include "BreakawayGameMode.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "BwayCharacterWithAbilities.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicDataAsset.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

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
		UE_LOG(LogBreakawayGame, Log, TEXT("Breakaway Game State initialized"));
	}
}

void ABreakawayGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogBreakawayGame, Warning, TEXT("Breakaway GameMode Loaded"));

	if (bSpawnDefaultRelicsAtStart)
	{
		SpawnDefaultRelics();
	}

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
	
	// Get appropriate spawn points based on team
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
	// Reset relic
	ResetRelic();

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


ARelicActor* ABreakawayGameMode::SpawnRelic(const URelicDataAsset* RelicData, const FTransform& SpawnTransform)
{
	if (!RelicData)
	{
		UE_LOG(LogBreakawayGame, Error, TEXT("SpawnRelic called with null RelicData!"));
		return nullptr;
	}

	if (!RelicData->RelicSettings)
	{
		UE_LOG(LogBreakawayGame, Error, TEXT("RelicData %s has null RelicSettings!"), *RelicData->GetName());
		return nullptr;
	}

	// Determine which actor class to spawn
	TSubclassOf<ARelicActor> ActorClass = RelicData->RelicActorClass;
	if (!ActorClass)
	{
		ActorClass = ARelicActor::StaticClass();
	}

	// Use deferred spawning to inject data before BeginPlay
	ARelicActor* NewRelic = GetWorld()->SpawnActorDeferred<ARelicActor>(
		ActorClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);

	if (NewRelic)
	{
		// Initialize with the data asset
		NewRelic->InitializeRelicData(RelicData);
        
		// Finish spawning
		NewRelic->FinishSpawning(SpawnTransform);

		UE_LOG(LogBreakawayGame, Log, TEXT("Successfully spawned relic: %s at %s"), 
			*RelicData->RelicDisplayName.ToString(), *SpawnTransform.GetLocation().ToString());
	}

	return NewRelic;
}

void ABreakawayGameMode::ResetRelic()
{
	if (!ActiveRelic)
	{
		SpawnDefaultRelics();
		return;
	}

	// Find relic spawn point
	TArray<AActor*> RelicSpawns = GetPlayerStartsWithTag(RelicSpawnPointTag);

	if (RelicSpawns.Num() > 0)
	{
		AActor* SpawnPoint = RelicSpawns[0];
		ActiveRelic->SetActorLocation(SpawnPoint->GetActorLocation());
		ActiveRelic->SetActorRotation(SpawnPoint->GetActorRotation());
		
		// Reset relic state
		ActiveRelic->OnDropped();
		
		UE_LOG(LogBreakawayGame, Log, TEXT("Relic reset to spawn location"));
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
	if (!BwayGS || !ActiveRelic)
	{
		return -1;
	}

	// If relic has a carrier, use their team
	if (ActiveRelic->CurrentCarrier && ActiveRelic->CurrentCarrier->GetPlayerState())
	{
		return BwayGS->GetPlayerTeam(ActiveRelic->CurrentCarrier->GetPlayerState());
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


void ABreakawayGameMode::SpawnDefaultRelics()
{
	if (DefaultRelicTypes.Num() == 0)
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("No default relic types configured in GameMode!"));
		return;
	}

	for (const URelicDataAsset* RelicData : DefaultRelicTypes)
	{
		if (!RelicData)
		{
			continue;
		}

		// Use spawn location from settings if available
		FVector SpawnLocation = FVector::ZeroVector;
		if (RelicData->RelicSettings && RelicData->RelicSettings->SpawnLocations.Num() > 0)
		{
			if (RelicData->RelicSettings->bUseRandomSpawnLocation)
			{
				int32 RandomIndex = FMath::RandRange(0, RelicData->RelicSettings->SpawnLocations.Num() - 1);
				SpawnLocation = RelicData->RelicSettings->SpawnLocations[RandomIndex];
			}
			else
			{
				SpawnLocation = RelicData->RelicSettings->SpawnLocations[0];
			}
		}
		else if (RelicData->RelicSettings)
		{
			SpawnLocation = RelicData->RelicSettings->DefaultSpawnLocation;
		}

		FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
		SpawnRelic(RelicData, SpawnTransform);
	}
}