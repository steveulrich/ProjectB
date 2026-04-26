// Copyright Epic Games, Inc. All Rights Reserved.

#include "BreakawayGameMode.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "BwayPlayerController.h"
#include "BwayCharacterWithAbilities.h"
#include "SpawnSystem/BwaySpawnPointManagerComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayTagsManager.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogBreakawayGame);

ABreakawayGameMode::ABreakawayGameMode()
{
	// Set default game state class
	GameStateClass = ABwayGameState::StaticClass();
	PlayerStateClass = ABwayPlayerState::StaticClass();
	PlayerControllerClass = ABwayPlayerController::StaticClass();
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

	// Let level actors/components finish BeginPlay and register with the spawn
	// manager before spawning match-owned objects like the relic.
	GetWorldTimerManager().SetTimerForNextTick(this, &ABreakawayGameMode::SpawnInitialGameObjects);

	bGameInitialized = true;
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

void ABreakawayGameMode::RestartPlayer(AController* NewPlayer)
{
	UE_LOG(LogBreakawayGame, Log, TEXT("RestartPlayer: BEGIN for %s"), *GetNameSafe(NewPlayer));

	// Check for selected hero before spawning
	if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		if (ABwayPlayerState* BwayPS = PC->GetPlayerState<ABwayPlayerState>())
		{
			FPrimaryAssetId HeroId = BwayPS->GetSelectedHeroId();
			UE_LOG(LogBreakawayGame, Log, TEXT("RestartPlayer: PlayerState has SelectedHeroId = %s"), 
				HeroId.IsValid() ? *HeroId.ToString() : TEXT("NONE"));
		}
	}

	// Call base implementation to spawn the pawn
	Super::RestartPlayer(NewPlayer);

	// Now apply hero data to the newly spawned pawn
	ApplyHeroDataToNewPawn(NewPlayer);

	UE_LOG(LogBreakawayGame, Log, TEXT("RestartPlayer: END for %s"), *GetNameSafe(NewPlayer));
}

void ABreakawayGameMode::ApplyHeroDataToNewPawn(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		return;
	}

	// Get the player state to check for selected hero
	ABwayPlayerState* BwayPS = PC->GetPlayerState<ABwayPlayerState>();
	if (!BwayPS)
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("ApplyHeroDataToNewPawn: No BwayPlayerState for %s"), *PC->GetName());
		return;
	}

	// Get the selected hero ID
	FPrimaryAssetId HeroId = BwayPS->GetSelectedHeroId();
	if (!HeroId.IsValid())
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("ApplyHeroDataToNewPawn: Player %s has no hero selected"), *PC->GetName());
		return;
	}

	// Load the hero data asset
	UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId);
	if (!HeroData)
	{
		UE_LOG(LogBreakawayGame, Error, TEXT("ApplyHeroDataToNewPawn: Could not load hero data for %s"), *HeroId.ToString());
		return;
	}

	// Get the spawned pawn
	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("ApplyHeroDataToNewPawn: No pawn for player %s after RestartPlayer"), *PC->GetName());
		return;
	}

	// Cast to our character class
	ABwayCharacterWithAbilities* Character = Cast<ABwayCharacterWithAbilities>(Pawn);
	if (!Character)
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("ApplyHeroDataToNewPawn: Pawn is not ABwayCharacterWithAbilities (is %s)"), 
			*Pawn->GetClass()->GetName());
		return;
	}

	// Apply hero data to the character
	UE_LOG(LogBreakawayGame, Log, TEXT("ApplyHeroDataToNewPawn: Applying hero %s to character %s"), 
		*HeroData->DisplayName.ToString(), *Character->GetName());
	
	Character->InitializeHeroData(HeroData);
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
// Spawn System Integration
// ========================================

void ABreakawayGameMode::InitializeSpawnPointTags()
{
	// Initialize gameplay tags for spawn points
	Goal1SpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Goal.Team1"));
	Goal2SpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Goal.Team2"));
	
	UE_LOG(LogBreakawayGame, Log, TEXT("Initialized spawn point tags"));
}

void ABreakawayGameMode::SpawnInitialGameObjects()
{
	if (bInitialGameObjectsSpawned)
	{
		return;
	}

	++InitialGameObjectSpawnAttempts;

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

	// Relic ownership belongs to UBwayRelicManagerComponent. GameMode only kicks off
	// initial world-object spawning.
	if (UBwayRelicManagerComponent* RelicMgr = BwayGS->FindComponentByClass<UBwayRelicManagerComponent>())
	{
		if (!RelicMgr->SpawnRelic())
		{
			if (InitialGameObjectSpawnAttempts < 5)
			{
				UE_LOG(LogBreakawayGame, Warning, TEXT("Relic manager could not spawn relic; initial objects will retry next tick"));
				GetWorldTimerManager().SetTimerForNextTick(this, &ABreakawayGameMode::SpawnInitialGameObjects);
			}
			else
			{
				UE_LOG(LogBreakawayGame, Error, TEXT("Relic manager could not spawn relic after %d attempts"), InitialGameObjectSpawnAttempts);
			}
			return;
		}
	}
	else
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("No relic manager found - relic was not spawned"));
	}

	// Spawn goals
	SpawnPointManager->SpawnObjectsAtPoints(Goal1SpawnTag);
	SpawnPointManager->SpawnObjectsAtPoints(Goal2SpawnTag);
	
	bInitialGameObjectsSpawned = true;
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
