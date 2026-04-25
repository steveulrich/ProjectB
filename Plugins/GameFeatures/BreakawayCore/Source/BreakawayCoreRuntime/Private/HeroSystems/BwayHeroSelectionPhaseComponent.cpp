#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

UBwayHeroSelectionPhaseComponent::UBwayHeroSelectionPhaseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);

	// Set default phase tag
	PhaseTag = FGameplayTag::RequestGameplayTag(FName("ShooterGame.GamePhase.HeroSelection"));
}

void UBwayHeroSelectionPhaseComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only server manages the phase
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Listen for when Lyra's phase system starts the HeroSelection phase
	if (UWorld* World = GetWorld())
	{
		if (ULyraGamePhaseSubsystem* PhaseSubsystem = World->GetSubsystem<ULyraGamePhaseSubsystem>())
		{
			PhaseSubsystem->WhenPhaseStartsOrIsActive(
				PhaseTag,
				EPhaseTagMatchType::ExactMatch,
				FLyraGamePhaseTagDelegate::CreateUObject(this, &UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated)
			);
			
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Listening for Lyra phase: %s"), *PhaseTag.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: Could not find LyraGamePhaseSubsystem - phase integration disabled"));
		}
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

// ============================================================
// Hero Selection Phase
// ============================================================

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

	// Get selection manager
	UBwayHeroSelectionManager* Manager = GetSelectionManager();
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: No selection manager found!"));
		EndHeroSelectionPhase(); // Fail gracefully
		return;
	}

	// IMPORTANT: Bind to the all-ready delegate FIRST, before any locking calls.
	// This ensures we receive the broadcast even if all players are pre-selected from Frontend.
	Manager->OnAllPlayersReady.AddDynamic(this, &UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady);

	// Start selection phase FIRST — this resets all player selections and starts the timer.
	// Must happen before registering players and applying pre-selections.
	Manager->StartHeroSelection();

	// Register all current players
	if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			Manager->RegisterPlayer(PS);
		}
	}

	// Now apply any pre-selections from the Frontend subsystem (after registration)
	if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
			{
				if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(BwayPS->GetPlayerController()))
				{
					PC->Client_RequestPreSelectedHero();
				}
			}
		}
	}

	// Show UI to players who haven't already locked
	ShowHeroSelectionUI();

	// Broadcast event
	OnHeroSelectionPhaseStarted.Broadcast();

	// Explicitly check if all players are already ready (e.g. all pre-selected from Frontend).
	// The delegate may have already fired during the lock calls above, but if not, check now.
	if (Manager->AreAllPlayersReady())
	{
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: All players pre-selected from Frontend — ending phase immediately"));
		HandleAllPlayersReady();
	}

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

void UBwayHeroSelectionPhaseComponent::ShowHeroSelectionUI_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: ShowHeroSelectionUI base impl (override in Blueprint)"));

	// Base C++ implementation: ask each owning client to create its local widget.
	if (HeroSelectionWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: No HeroSelectionWidgetClass configured"));
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(It->Get()))
		{
			// Skip showing UI if this player already locked their selection (e.g. from Frontend)
			if (ABwayPlayerState* PS = PC->GetPlayerState<ABwayPlayerState>())
			{
				if (PS->IsHeroLocked())
				{
					continue;
				}
			}

			PC->Client_ShowHeroSelection(HeroSelectionWidgetClass);
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Sent hero select widget RPC to %s"), *PC->GetName());
		}
	}
}

void UBwayHeroSelectionPhaseComponent::HideHeroSelectionUI_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: HideHeroSelectionUI base impl (override in Blueprint)"));

	// Base C++ implementation: remove hero selection widgets and restore game input
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(It->Get()))
		{
			PC->Client_HideHeroSelection();
		}
	}
}

void UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated(const FGameplayTag& InPhaseTag)
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: HeroSelection phase activated by Lyra phase system! (Tag: %s)"), *InPhaseTag.ToString());
	
	// Now start our hero selection logic
	StartHeroSelectionPhase();
}

void UBwayHeroSelectionPhaseComponent::EndPhaseAndProgressToNext()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// In Lyra's phase system, starting a new sibling phase automatically ends the
	// current one (phases are nested by tag — sibling phases cancel each other).
	// So we start the next phase, which auto-cancels HeroSelection.
	if (NextPhaseAbilityClass)
	{
		if (UWorld* World = GetWorld())
		{
			if (ULyraGamePhaseSubsystem* PhaseSubsystem = World->GetSubsystem<ULyraGamePhaseSubsystem>())
			{
				PhaseSubsystem->StartPhase(NextPhaseAbilityClass);
				UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Started next phase via %s"),
					*NextPhaseAbilityClass->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: NextPhaseAbilityClass is not set! Cannot progress beyond hero selection. Set it in BP_BW_GameState or in the component defaults."));
	}
}

void UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: All players ready - ending phase"));

	// End our phase logic (assigns defaults, hides UI, spawns heroes)
	EndHeroSelectionPhase();
	
	// Progress Lyra's phase to the next one (e.g. Warmup / Playing)
	EndPhaseAndProgressToNext();
}

// ============================================================
// Internal Helpers
// ============================================================

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

void UBwayHeroSelectionPhaseComponent::SpawnHeroForPlayer_Implementation(ABwayPlayerState* PlayerState)
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

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Spawning hero %s for player %s"), 
		*HeroData->DisplayName.ToString(), *PlayerState->GetPlayerName());

	// Trigger a respawn through game mode (which calls ApplyHeroDataToNewPawn)
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

	// Check if we have a default hero configured
	if (DefaultHeroData.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: No default hero configured — players without a selection will have no hero"));
		return;
	}

	// Load the default hero data asset
	UBwayHeroDataAsset* DefaultHero = DefaultHeroData.LoadSynchronous();
	if (!DefaultHero)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: Failed to load DefaultHeroData"));
		return;
	}

	// Get its PrimaryAssetId so we can assign via the standard path
	FPrimaryAssetId DefaultHeroId = DefaultHero->GetPrimaryAssetId();
	if (!DefaultHeroId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: DefaultHeroData has invalid PrimaryAssetId"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Default hero is %s (%s)"),
		*DefaultHero->DisplayName.ToString(), *DefaultHeroId.ToString());

	// Assign default to anyone without a selection
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
		{
			if (!BwayPS->GetSelectedHeroId().IsValid())
			{
				BwayPS->ServerSetSelectedHeroId(DefaultHeroId);
				BwayPS->ServerLockHeroSelection();

				UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Assigned default hero %s to %s"), 
					*DefaultHero->DisplayName.ToString(), *BwayPS->GetPlayerName());
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