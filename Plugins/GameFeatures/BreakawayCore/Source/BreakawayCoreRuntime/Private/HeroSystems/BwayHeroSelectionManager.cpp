// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeroSystems/BwayHeroSelectionManager.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "GameFramework/PlayerState.h"
#include "Player/LyraPlayerBotController.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "AIController.h"

UBwayHeroSelectionManager::UBwayHeroSelectionManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UBwayHeroSelectionManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBwayHeroSelectionManager, PlayerSelections);
	DOREPLIFETIME(UBwayHeroSelectionManager, SelectionTimeRemaining);
	DOREPLIFETIME(UBwayHeroSelectionManager, bSelectionActive);
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

void UBwayHeroSelectionManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner()->HasAuthority() || !bSelectionActive || SelectionTimeLimit <= 0.0f)
	{
		return;
	}

	SelectionTimeRemaining = FMath::Max(0.0f, GetWorld()->GetTimerManager().GetTimerRemaining(SelectionTimerHandle));
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
	SelectionTimeRemaining = SelectionTimeLimit;

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
	SelectionTimeRemaining = 0.0f;

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
	SelectionTimeRemaining = 0.0f;

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
		if (!Selection.bIsLocked)
		{
			if (Selection.SelectedHeroId.IsValid())
			{
				Selection.bIsLocked = true;

				if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
				{
					BwayPS->ServerLockHeroSelection_Implementation();
				}

				OnPlayerHeroLocked.Broadcast(Selection.PlayerState, Selection.SelectedHeroId);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionManager: Player %s has no selection after resolve step"),
					Selection.PlayerState ? *Selection.PlayerState->GetPlayerName() : TEXT("Unknown"));
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Force locked all players with valid selections"));

	CheckAllPlayersReady();
}

void UBwayHeroSelectionManager::SynchronizePlayerSelectionState(APlayerState* PlayerState)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	UpdatePlayerSelection(PlayerState);
}

void UBwayHeroSelectionManager::OnRep_PlayerSelections()
{
	for (const FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (Selection.PlayerState)
		{
			OnPlayerHeroSelectionChanged.Broadcast(Selection.PlayerState, Selection.SelectedHeroId, Selection.TeamIndex);
			if (Selection.bIsLocked && Selection.SelectedHeroId.IsValid())
			{
				OnPlayerHeroLocked.Broadcast(Selection.PlayerState, Selection.SelectedHeroId);
			}
		}
	}
}

// ========== PRIVATE HELPERS ==========

void UBwayHeroSelectionManager::BindToPlayerState(ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	PlayerState->OnSelectedHeroChanged.AddDynamic(this, &UBwayHeroSelectionManager::OnPlayerSelectedHero);
	PlayerState->OnHeroLocked.AddDynamic(this, &UBwayHeroSelectionManager::OnPlayerLockedHero);
}

void UBwayHeroSelectionManager::UnbindFromPlayerState(ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	PlayerState->OnSelectedHeroChanged.RemoveDynamic(this, &UBwayHeroSelectionManager::OnPlayerSelectedHero);
	PlayerState->OnHeroLocked.RemoveDynamic(this, &UBwayHeroSelectionManager::OnPlayerLockedHero);
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

void UBwayHeroSelectionManager::OnPlayerLockedHero(FPrimaryAssetId LockedHeroId)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	for (FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
		{
			UpdatePlayerSelection(BwayPS);
		}
	}
}

void UBwayHeroSelectionManager::OnSelectionTimeout()
{
	UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionManager: Selection time expired"));

	if (bAutoLockOnTimeout)
	{
		FPrimaryAssetId FallbackHeroId;
		if (ABwayGameState* GameState = GetBwayGameState())
		{
			if (const UBwayHeroSelectionPhaseComponent* HeroSelectionPhase = GameState->HeroSelectionPhaseComponent)
			{
				FallbackHeroId = HeroSelectionPhase->ResolveFallbackHeroId();
			}
		}

		ResolveMissingSelectionsAndLockAll(FallbackHeroId);
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
			bool bOldLocked = Selection.bIsLocked;
			
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
			if (Selection.bIsLocked && !bOldLocked)
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

bool UBwayHeroSelectionManager::AssignRandomHeroToPlayer(ABwayPlayerState* PlayerState, FPrimaryAssetId FallbackHeroId, bool bLockImmediately)
{
	if (!GetOwner()->HasAuthority() || !PlayerState)
	{
		return false;
	}

	if (PlayerState->GetSelectedHeroId().IsValid() && (!bLockImmediately || PlayerState->IsHeroLocked()))
	{
		return true;
	}

	const FPrimaryAssetId ResolvedFallbackHeroId = ResolveFallbackHeroId(FallbackHeroId);
	int32 TeamIndex = -1;
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		TeamIndex = GameState->GetPlayerTeam(PlayerState);
	}

	const FPrimaryAssetId HeroToAssign = PlayerState->GetSelectedHeroId().IsValid()
		? PlayerState->GetSelectedHeroId()
		: PickRandomHeroForTeam(TeamIndex, ResolvedFallbackHeroId);

	if (!HeroToAssign.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionManager: Could not assign a hero to %s"), *PlayerState->GetPlayerName());
		return false;
	}

	if (PlayerState->GetSelectedHeroId() != HeroToAssign)
	{
		PlayerState->ServerSetSelectedHeroId(HeroToAssign);
	}

	if (bLockImmediately && !PlayerState->IsHeroLocked())
	{
		PlayerState->ServerLockHeroSelection_Implementation();
	}

	SynchronizePlayerSelectionState(PlayerState);

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Assigned hero %s to %s (Lock=%s)"),
		*HeroToAssign.ToString(),
		*PlayerState->GetPlayerName(),
		bLockImmediately ? TEXT("true") : TEXT("false"));

	return true;
}

bool UBwayHeroSelectionManager::AssignHeroToPlayer(ABwayPlayerState* PlayerState, FPrimaryAssetId HeroId, bool bLockImmediately)
{
	if (!GetOwner()->HasAuthority() || !PlayerState || !HeroId.IsValid())
	{
		return false;
	}

	if (PlayerState->GetSelectedHeroId().IsValid() && (!bLockImmediately || PlayerState->IsHeroLocked()))
	{
		return true;
	}

	if (ABwayGameState* GameState = GetBwayGameState())
	{
		const int32 TeamIndex = GameState->GetPlayerTeam(PlayerState);
		if (TeamIndex >= 0 && !IsHeroAvailableForTeam(HeroId, TeamIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionManager: Cannot assign duplicate hero %s to team %d"),
				*HeroId.ToString(), TeamIndex + 1);
			return false;
		}
	}

	if (PlayerState->GetSelectedHeroId() != HeroId)
	{
		PlayerState->ServerSetSelectedHeroId_Implementation(HeroId);
	}

	if (bLockImmediately && !PlayerState->IsHeroLocked())
	{
		PlayerState->ServerLockHeroSelection_Implementation();
	}

	SynchronizePlayerSelectionState(PlayerState);

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionManager: Assigned hero %s to %s (Lock=%s)"),
		*HeroId.ToString(),
		*PlayerState->GetPlayerName(),
		bLockImmediately ? TEXT("true") : TEXT("false"));

	return true;
}

void UBwayHeroSelectionManager::AssignRandomHeroToPlayers(bool bOnlyBots, FPrimaryAssetId FallbackHeroId, bool bLockImmediately)
{
	ABwayGameState* GameState = GetBwayGameState();
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PS : GameState->PlayerArray)
	{
		ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
		if (!BwayPS)
		{
			continue;
		}

		if (bOnlyBots && !IsBotPlayerState(BwayPS))
		{
			continue;
		}

		if (!BwayPS->GetSelectedHeroId().IsValid() || (bLockImmediately && !BwayPS->IsHeroLocked()))
		{
			AssignRandomHeroToPlayer(BwayPS, FallbackHeroId, bLockImmediately);
		}
	}
}

void UBwayHeroSelectionManager::ResolveMissingSelectionsAndLockAll(FPrimaryAssetId FallbackHeroId)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	const FPrimaryAssetId ResolvedFallbackHeroId = ResolveFallbackHeroId(FallbackHeroId);

	for (FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (Selection.bIsLocked)
		{
			continue;
		}

		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
		{
			if (!BwayPS->GetSelectedHeroId().IsValid())
			{
				AssignRandomHeroToPlayer(BwayPS, ResolvedFallbackHeroId, /*bLockImmediately=*/false);
			}
		}
	}

	ForceLockAllPlayers();
}

FPrimaryAssetId UBwayHeroSelectionManager::ResolveFallbackHeroId(FPrimaryAssetId PreferredFallbackHeroId) const
{
	if (PreferredFallbackHeroId.IsValid())
	{
		return PreferredFallbackHeroId;
	}

	if (UBwayHeroRegistry* HeroRegistry = UBwayHeroRegistry::Get(this))
	{
		for (const TSoftObjectPtr<UBwayHeroDataAsset>& HeroSoftObject : HeroRegistry->GetAllHeroSoftObjects())
		{
			if (UBwayHeroDataAsset* CandidateHero = HeroRegistry->LoadHeroSync(HeroSoftObject))
			{
				const FPrimaryAssetId CandidateId = CandidateHero->GetPrimaryAssetId();
				if (CandidateId.IsValid())
				{
					return CandidateId;
				}
			}
		}
	}

	return FPrimaryAssetId();
}

FPrimaryAssetId UBwayHeroSelectionManager::PickRandomHeroForTeam(int32 TeamIndex, FPrimaryAssetId FallbackHeroId) const
{
	TArray<FPrimaryAssetId> AvailableHeroIds;
	for (const FPrimaryAssetId& HeroId : GetRegisteredHeroIds())
	{
		if (HeroId.IsValid() && IsHeroAvailableForTeam(HeroId, TeamIndex))
		{
			AvailableHeroIds.Add(HeroId);
		}
	}

	if (AvailableHeroIds.Num() > 0)
	{
		const int32 RandomIndex = FMath::RandRange(0, AvailableHeroIds.Num() - 1);
		return AvailableHeroIds[RandomIndex];
	}

	return ResolveFallbackHeroId(FallbackHeroId);
}

TArray<FPrimaryAssetId> UBwayHeroSelectionManager::GetRegisteredHeroIds() const
{
	TArray<FPrimaryAssetId> HeroIds;

	if (UBwayHeroRegistry* HeroRegistry = UBwayHeroRegistry::Get(this))
	{
		for (const TSoftObjectPtr<UBwayHeroDataAsset>& HeroSoftObject : HeroRegistry->GetAllHeroSoftObjects())
		{
			if (UBwayHeroDataAsset* HeroData = HeroRegistry->LoadHeroSync(HeroSoftObject))
			{
				const FPrimaryAssetId HeroId = HeroData->GetPrimaryAssetId();
				if (HeroId.IsValid())
				{
					HeroIds.AddUnique(HeroId);
				}
			}
		}
	}

	return HeroIds;
}

bool UBwayHeroSelectionManager::IsBotPlayerState(const ABwayPlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return false;
	}

	return Cast<ALyraPlayerBotController>(PlayerState->GetOwner()) != nullptr
		|| Cast<AAIController>(PlayerState->GetOwner()) != nullptr
		|| PlayerState->GetPlayerController() == nullptr;
}

ABwayGameState* UBwayHeroSelectionManager::GetBwayGameState() const
{
	return Cast<ABwayGameState>(GetOwner());
}
