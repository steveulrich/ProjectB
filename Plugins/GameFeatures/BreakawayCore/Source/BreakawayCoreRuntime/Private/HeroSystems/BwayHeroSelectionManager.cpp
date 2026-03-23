// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeroSystems/BwayHeroSelectionManager.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "Engine/World.h"

UBwayHeroSelectionManager::UBwayHeroSelectionManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void UBwayHeroSelectionManager::BeginPlay()
{
	Super::BeginPlay();

	// Only the server manages hero selection
	if (GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Initialized on server"));
	}
}

void UBwayHeroSelectionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear any active timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SelectionTimerHandle);
	}

	// Unbind from all player states
	for (const FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
		{
			UnbindFromPlayerState(BwayPS);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// ========== PLAYER REGISTRATION ==========

void UBwayHeroSelectionManager::RegisterPlayer(APlayerState* PlayerState)
{
	if (!PlayerState || !GetOwner()->HasAuthority())
	{
		return;
	}

	// Check if already registered
	for (const FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (Selection.PlayerState == PlayerState)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionManager: Player %s already registered"), 
				*PlayerState->GetPlayerName());
			return;
		}
	}

	// Create new selection state
	FPlayerHeroSelectionState NewSelection;
	NewSelection.PlayerState = PlayerState;
	NewSelection.bIsLocked = false;

	// Get team from game state
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		NewSelection.TeamIndex = GameState->GetPlayerTeam(PlayerState);
	}

	PlayerSelections.Add(NewSelection);

	// Bind to player state updates
	if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState))
	{
		BindToPlayerState(BwayPS);
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Registered player %s (Team %d)"), 
		*PlayerState->GetPlayerName(), NewSelection.TeamIndex);
}

void UBwayHeroSelectionManager::UnregisterPlayer(APlayerState* PlayerState)
{
	if (!PlayerState || !GetOwner()->HasAuthority())
	{
		return;
	}

	// Find and remove the player
	for (int32 i = PlayerSelections.Num() - 1; i >= 0; --i)
	{
		if (PlayerSelections[i].PlayerState == PlayerState)
		{
			// Unbind from player state
			if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState))
			{
				UnbindFromPlayerState(BwayPS);
			}

			PlayerSelections.RemoveAt(i);
			
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Unregistered player %s"), 
				*PlayerState->GetPlayerName());
			break;
		}
	}
}

// ========== SELECTION QUERIES ==========

TArray<FPlayerHeroSelectionState> UBwayHeroSelectionManager::GetAllPlayerSelections() const
{
	return PlayerSelections;
}

FPlayerHeroSelectionState UBwayHeroSelectionManager::GetPlayerSelection(APlayerState* PlayerState) const
{
	for (const FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (Selection.PlayerState == PlayerState)
		{
			return Selection;
		}
	}

	return FPlayerHeroSelectionState();
}

bool UBwayHeroSelectionManager::IsHeroAvailableForTeam(FPrimaryAssetId HeroId, int32 TeamIndex) const
{
	// If duplicates are allowed, always return true
	if (bAllowDuplicateHeroes)
	{
		return true;
	}

	// Check if any player on the same team has already selected this hero
	for (const FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (Selection.TeamIndex == TeamIndex && 
			Selection.SelectedHeroId.IsValid() && 
			Selection.SelectedHeroId == HeroId)
		{
			return false;
		}
	}

	return true;
}

bool UBwayHeroSelectionManager::AreAllPlayersReady() const
{
	if (PlayerSelections.Num() == 0)
	{
		return false;
	}

	for (const FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		// Check if player has selected and locked a hero
		if (!Selection.bIsLocked || !Selection.SelectedHeroId.IsValid())
		{
			return false;
		}
	}

	return true;
}

int32 UBwayHeroSelectionManager::GetNumPlayersReady() const
{
	int32 Count = 0;
	for (const FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (Selection.bIsLocked && Selection.SelectedHeroId.IsValid())
		{
			Count++;
		}
	}
	return Count;
}

// ========== PHASE CONTROL ==========

void UBwayHeroSelectionManager::StartHeroSelection()
{
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionManager: StartHeroSelection called on client"));
		return;
	}

	bSelectionActive = true;

	// Reset all player selections
	for (FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		Selection.SelectedHeroId = FPrimaryAssetId();
		Selection.bIsLocked = false;

		// Unlock player states
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
		{
			BwayPS->UnlockHeroSelection();
		}
	}

	// Start selection timer if configured
	if (SelectionTimeLimit > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			SelectionTimerHandle,
			this,
			&UBwayHeroSelectionManager::OnSelectionTimeout,
			SelectionTimeLimit,
			false
		);

		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Hero selection started (%.0f seconds)"), 
			SelectionTimeLimit);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Hero selection started (no time limit)"));
	}
}

bool UBwayHeroSelectionManager::EndHeroSelection()
{
	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionManager: EndHeroSelection called on client"));
		return false;
	}

	bSelectionActive = false;

	// Clear timer
	GetWorld()->GetTimerManager().ClearTimer(SelectionTimerHandle);

	// Validate all selections
	bool bAllValid = true;
	for (const FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (!Selection.bIsLocked || !Selection.SelectedHeroId.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionManager: Player %s has invalid selection"), 
				Selection.PlayerState ? *Selection.PlayerState->GetPlayerName() : TEXT("Unknown"));
			bAllValid = false;
		}
	}

	if (bAllValid)
	{
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Hero selection ended - all selections valid"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionManager: Hero selection ended - some selections invalid"));
	}

	return bAllValid;
}

void UBwayHeroSelectionManager::ResetHeroSelection()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Clear timer
	GetWorld()->GetTimerManager().ClearTimer(SelectionTimerHandle);

	bSelectionActive = false;

	// Reset all selections
	for (FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		Selection.SelectedHeroId = FPrimaryAssetId();
		Selection.bIsLocked = false;

		// Unlock player states
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
		{
			BwayPS->UnlockHeroSelection();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Hero selection reset"));
}

void UBwayHeroSelectionManager::ForceLockAllPlayers()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	for (FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (!Selection.bIsLocked && Selection.SelectedHeroId.IsValid())
		{
			Selection.bIsLocked = true;

			if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
			{
				BwayPS->ServerLockHeroSelection();
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Force locked all players"));

	CheckAllPlayersReady();
}

// ========== PRIVATE HELPERS ==========

void UBwayHeroSelectionManager::BindToPlayerState(ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	// Bind to selection changed delegate
	PlayerState->OnSelectedHeroChanged.AddDynamic(this, &UBwayHeroSelectionManager::OnPlayerSelectedHero);
}

void UBwayHeroSelectionManager::UnbindFromPlayerState(ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	// Unbind from selection changed delegate
	PlayerState->OnSelectedHeroChanged.RemoveDynamic(this, &UBwayHeroSelectionManager::OnPlayerSelectedHero);
}

void UBwayHeroSelectionManager::OnPlayerSelectedHero(FPrimaryAssetId NewHeroId)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Find which player changed their selection
	// Note: This is called via delegate, so we need to find the player from context
	// In a real implementation, you'd pass the PlayerState through the delegate
	// For now, we'll update all selections
	for (FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
		{
			UpdatePlayerSelection(Selection.PlayerState);
		}
	}
}

void UBwayHeroSelectionManager::OnSelectionTimeout()
{
	UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionManager: Selection time expired"));

	if (bAutoLockOnTimeout)
	{
		ForceLockAllPlayers();
	}
}

void UBwayHeroSelectionManager::UpdatePlayerSelection(APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState);
	if (!BwayPS)
	{
		return;
	}

	// Find the selection state
	for (FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (Selection.PlayerState == PlayerState)
		{
			// Update from player state
			FPrimaryAssetId OldHeroId = Selection.SelectedHeroId;
			Selection.SelectedHeroId = BwayPS->GetSelectedHeroId();
			Selection.bIsLocked = BwayPS->IsHeroLocked();

			// Broadcast change if hero changed
			if (OldHeroId != Selection.SelectedHeroId)
			{
				OnPlayerHeroSelectionChanged.Broadcast(PlayerState, Selection.SelectedHeroId, Selection.TeamIndex);
				
				UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Player %s selected hero %s"), 
					*PlayerState->GetPlayerName(), *Selection.SelectedHeroId.ToString());
			}

			// Broadcast lock if newly locked
			if (Selection.bIsLocked && OldHeroId != Selection.SelectedHeroId)
			{
				OnPlayerHeroLocked.Broadcast(PlayerState, Selection.SelectedHeroId);
				
				UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Player %s locked hero %s"), 
					*PlayerState->GetPlayerName(), *Selection.SelectedHeroId.ToString());
			}

			// Check if all ready
			CheckAllPlayersReady();
			break;
		}
	}
}

void UBwayHeroSelectionManager::CheckAllPlayersReady()
{
	if (AreAllPlayersReady())
	{
		OnAllPlayersReady.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: All players ready!"));
	}
}

ABwayGameState* UBwayHeroSelectionManager::GetBwayGameState() const
{
	return Cast<ABwayGameState>(GetOwner());
}
