#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "GameFramework/PlayerController.h"

UBwayHeroSelectionPhaseComponent::UBwayHeroSelectionPhaseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);

	// Set default phase tag
	PhaseTag = FGameplayTag::RequestGameplayTag(FName("GamePhase.HeroSelection"));
}

void UBwayHeroSelectionPhaseComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only server manages the phase
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Initialized"));
}

void UBwayHeroSelectionPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bPhaseActive)
	{
		EndHeroSelectionPhase();
	}

	Super::EndPlay(EndPlayReason);
}

void UBwayHeroSelectionPhaseComponent::StartHeroSelectionPhase()
{
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: StartHeroSelectionPhase called on client"));
		return;
	}

	if (bPhaseActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: Phase already active"));
		return;
	}

	bPhaseActive = true;

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Starting hero selection phase"));

	// Add phase tag to game state
	if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		if (PhaseTag.IsValid())
		{
			// GameState->AddGameplayTag(PhaseTag); // If you have tag support
		}
	}

	// Get selection manager
	UBwayHeroSelectionManager* Manager = GetSelectionManager();
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: No selection manager found!"));
		EndHeroSelectionPhase(); // Fail gracefully
		return;
	}

	// Register all current players
	if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			Manager->RegisterPlayer(PS);
		}
	}

	// Bind to all players ready event
	Manager->OnAllPlayersReady.AddDynamic(this, &UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady);

	// Start selection phase
	Manager->StartHeroSelection();

	// Show UI to all players
	ShowHeroSelectionUI();

	// Broadcast event
	OnHeroSelectionPhaseStarted.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Hero selection phase started"));
}

void UBwayHeroSelectionPhaseComponent::EndHeroSelectionPhase()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (!bPhaseActive)
	{
		return;
	}

	bPhaseActive = false;

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Ending hero selection phase"));

	// Get selection manager
	UBwayHeroSelectionManager* Manager = GetSelectionManager();
	if (Manager)
	{
		// Unbind from events
		Manager->OnAllPlayersReady.RemoveDynamic(this, &UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady);

		// Assign default heroes to anyone who didn't select
		AssignDefaultHeroes();

		// End selection phase
		Manager->EndHeroSelection();
	}

	// Hide UI from all players
	HideHeroSelectionUI();

	// Spawn heroes for all players
	SpawnHeroesForAllPlayers();

	// Remove phase tag
	if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		if (PhaseTag.IsValid())
		{
			// GameState->RemoveGameplayTag(PhaseTag); // If you have tag support
		}
	}

	// Broadcast event
	OnHeroSelectionPhaseEnded.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Hero selection phase ended"));
}

void UBwayHeroSelectionPhaseComponent::SkipHeroSelection()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Skipping hero selection"));

	// Assign default heroes to everyone
	AssignDefaultHeroes();

	// Spawn heroes immediately
	SpawnHeroesForAllPlayers();
}

void UBwayHeroSelectionPhaseComponent::ShowHeroSelectionUI()
{
	if (!HeroSelectionWidgetClass.IsNull())
	{
		// TODO: Push widget to all players
		// This requires integration with Lyra's UI system
		// For now, log and handle in Blueprint
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Should show UI (implement in Blueprint)"));
	}
}

void UBwayHeroSelectionPhaseComponent::HideHeroSelectionUI()
{
	// TODO: Remove widget from all players
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Should hide UI (implement in Blueprint)"));
}

void UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: All players ready - ending phase"));

	// End the phase
	EndHeroSelectionPhase();
}

void UBwayHeroSelectionPhaseComponent::SpawnHeroesForAllPlayers()
{
	ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner());
	if (!GameState)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: No game state"));
		return;
	}

	// Spawn hero for each player
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
		{
			SpawnHeroForPlayer(BwayPS);
		}
	}
}

void UBwayHeroSelectionPhaseComponent::SpawnHeroForPlayer(ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	// Get selected hero
	FPrimaryAssetId HeroId = PlayerState->GetSelectedHeroId();
	if (!HeroId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: Player %s has no hero selected"), 
			*PlayerState->GetPlayerName());
		return;
	}

	// Load hero data
	UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId);
	if (!HeroData)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: Could not load hero data for %s"), 
			*HeroId.ToString());
		return;
	}

	// Get player controller
	APlayerController* PC = Cast<APlayerController>(PlayerState->GetOwner());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: No controller for player %s"), 
			*PlayerState->GetPlayerName());
		return;
	}

	// TODO: Spawn hero character based on HeroData
	// This requires:
	// 1. Finding appropriate spawn point for player's team
	// 2. Spawning character with hero mesh/animations
	// 3. Applying hero ability set
	// 4. Possessing the character

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Spawning hero %s for player %s"), 
		*HeroData->DisplayName.ToString(), *PlayerState->GetPlayerName());

	// For now, just trigger a respawn through game mode
	// You'll need to modify your game mode to use hero data
	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		GameMode->RestartPlayer(PC);
	}
}

void UBwayHeroSelectionPhaseComponent::AssignDefaultHeroes()
{
	ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner());
	if (!GameState)
	{
		return;
	}

	// Check if we have a default hero
	if (DefaultHeroData.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: No default hero configured"));
		return;
	}

	// Assign default to anyone without a selection
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
		{
			if (!BwayPS->GetSelectedHeroId().IsValid())
			{
				// Load default hero
				//FPrimaryAssetId DefaultHeroId = DefaultHeroData.ToSoftObjectPath().GetAssetPathName();
				
				UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Assigning default hero to %s"), 
					*BwayPS->GetPlayerName());

				// This would need the actual primary asset ID from the asset
				// BwayPS->ServerSetSelectedHeroId(DefaultHeroId);
			}
		}
	}
}

UBwayHeroSelectionManager* UBwayHeroSelectionPhaseComponent::GetSelectionManager() const
{
	ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner());
	if (!GameState)
	{
		return nullptr;
	}

	return GameState->FindComponentByClass<UBwayHeroSelectionManager>();
}