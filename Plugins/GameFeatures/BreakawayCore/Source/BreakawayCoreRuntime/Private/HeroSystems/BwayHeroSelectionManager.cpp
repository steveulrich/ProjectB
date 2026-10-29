// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeroSystems/BwayHeroSelectionManager.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
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
	DOREPLIFETIME(UBwayHeroSelectionManager, StagingBotSelections);
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
	if (auto* PS = Cast<ABwayPlayerState>(PlayerState); PS && PS->IsHeroLocked()
		&& !UBwayHeroRegistry::GetHeroDataById(PS->GetSelectedHeroId()))
	{
		// A carried lock is useful only with a resolvable hero. Preserve valid
		// locks, but let an invalid carried selection make a new choice.
		PS->UnlockHeroSelection();
	}

	// Check if already registered
	for (const FPlayerHeroSelectionState& Selection : PlayerSelections)
	{
		if (Selection.PlayerState == PlayerState)
		{
			return;
		}
	}

	// Create new selection state
	FPlayerHeroSelectionState NewSelection;
	NewSelection.PlayerState = PlayerState;
	NewSelection.bIsLocked = false;
	if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState))
	{
		NewSelection.SelectedHeroId = BwayPS->GetSelectedHeroId();
		NewSelection.bIsLocked = BwayPS->IsHeroLocked();
	}

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

	ReconcileStagingBotSelections();
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
			ReconcileStagingBotSelections();
			// Finish player/bot replacement before evaluating the remaining roster.
			if (bSelectionActive && GetWorld() && !GetWorld()->bIsTearingDown)
			{
				GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					if (bSelectionActive) CheckAllPlayersReady();
				}));
			}
			
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

	// PlayerSelections is a staging/UI projection. Gameplay and late joins must
	// also reserve heroes, even when no selection phase is active.
	const ABwayGameState* GameState = GetBwayGameState();
	if (!GameState) return false;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState);
		if (BwayPS && !BwayPS->IsInactive() && GameState->GetPlayerTeam(BwayPS) == TeamIndex
			&& BwayPS->GetSelectedHeroId().IsValid() && BwayPS->GetSelectedHeroId() == HeroId)
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

	RefreshSelectionDeadline();
	ReconcileStagingBotSelections();
}

void UBwayHeroSelectionManager::RefreshSelectionDeadline()
{
	if (!GetOwner()->HasAuthority() || !bSelectionActive || !GetWorld() || GetWorld()->bIsTearingDown) return;
	GetWorld()->GetTimerManager().ClearTimer(SelectionTimerHandle);
	SelectionTimeRemaining = FMath::Max(0.f, SelectionTimeLimit);
	// Start selection timer if configured, preserving every current choice and lock.
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

	StagingBotSelections.Reset();
	OnStagingBotSelectionsChanged.Broadcast();

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

	ReconcileStagingBotSelections();
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

void UBwayHeroSelectionManager::OnRep_StagingBotSelections()
{
	OnStagingBotSelectionsChanged.Broadcast();
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
			const int32 OldTeamIndex = Selection.TeamIndex;
			if (const ABwayGameState* GameState = GetBwayGameState())
			{
				Selection.TeamIndex = GameState->GetPlayerTeam(PlayerState);
			}
			
			Selection.SelectedHeroId = BwayPS->GetSelectedHeroId();
			Selection.bIsLocked = BwayPS->IsHeroLocked();

			// Broadcast change if hero changed
			if (OldHeroId != Selection.SelectedHeroId || OldTeamIndex != Selection.TeamIndex)
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

			// Reconcile pre-assigned staging bots before checking readiness so bot picks yield to humans immediately
			ReconcileStagingBotSelections();

			// Check if all ready
			CheckAllPlayersReady();
			break;
		}
	}
}

void UBwayHeroSelectionManager::CheckAllPlayersReady()
{
	if (!bSelectionActive)
	{
		return;
	}

	static bool bAutoAssigningBotHeroes = false;
	if (!bAutoAssigningBotHeroes && !AreAllPlayersReady())
	{
		bool bHasHumanPlayer = false;
		bool bAllHumansReady = true;
		bool bHasUnlockedBot = false;
		for (const FPlayerHeroSelectionState& Selection : PlayerSelections)
		{
			const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState);
			if (!BwayPS || BwayPS->IsInactive() || BwayPS->IsOnlyASpectator())
			{
				continue;
			}
			if (IsBotPlayerState(BwayPS))
			{
				if (!Selection.bIsLocked || !Selection.SelectedHeroId.IsValid())
				{
					bHasUnlockedBot = true;
				}
			}
			else
			{
				bHasHumanPlayer = true;
				if (!Selection.bIsLocked || !Selection.SelectedHeroId.IsValid())
				{
					bAllHumansReady = false;
				}
			}
		}

		if (bHasHumanPlayer && bAllHumansReady && bHasUnlockedBot)
		{
			TGuardValue<bool> AutoAssignGuard(bAutoAssigningBotHeroes, true);
			AssignRandomHeroToPlayers(/*bOnlyBots=*/true, FPrimaryAssetId(), /*bLockImmediately=*/true);
		}
	}

	if (bSelectionActive && AreAllPlayersReady())
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
		if (!PlayerState->TrySetSelectedHeroId(HeroToAssign)) return false;
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
		if (PlayerState->GetSelectedHeroId() != HeroId && TeamIndex >= 0 && !IsHeroAvailableForTeam(HeroId, TeamIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionManager: Cannot assign duplicate hero %s to team %d"),
				*HeroId.ToString(), TeamIndex + 1);
			return false;
		}
	}

	if (PlayerState->GetSelectedHeroId() != HeroId)
	{
		if (!PlayerState->TrySetSelectedHeroId(HeroId)) return false;
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
	TArray<FPrimaryAssetId> UnclaimedByStagingBots;
	for (const FPrimaryAssetId& HeroId : GetRegisteredHeroIds())
	{
		if (HeroId.IsValid() && IsHeroAvailableForTeam(HeroId, TeamIndex))
		{
			AvailableHeroIds.Add(HeroId);
			const bool bHeldByStagingBot = StagingBotSelections.ContainsByPredicate(
				[TeamIndex, &HeroId](const FBwayStagingBotSelection& BotSel)
				{
					return BotSel.TeamIndex == TeamIndex && BotSel.SelectedHeroId == HeroId;
				});
			if (!bHeldByStagingBot)
			{
				UnclaimedByStagingBots.Add(HeroId);
			}
		}
	}

	if (UnclaimedByStagingBots.Num() > 0)
	{
		const int32 RandomIndex = FMath::RandRange(0, UnclaimedByStagingBots.Num() - 1);
		return UnclaimedByStagingBots[RandomIndex];
	}

	if (AvailableHeroIds.Num() > 0)
	{
		const int32 RandomIndex = FMath::RandRange(0, AvailableHeroIds.Num() - 1);
		return AvailableHeroIds[RandomIndex];
	}

	return ResolveFallbackHeroId(FallbackHeroId);
}

void UBwayHeroSelectionManager::SetForceStagingBotPreAssignmentForTest(bool bEnable, int32 TestMaxBots)
{
	bForceStagingBotPreAssignmentForTest = bEnable;
	ForcedStagingMaxBotsForTest = FMath::Clamp(TestMaxBots, 0, 8);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ReconcileStagingBotSelections();
	}
}

bool UBwayHeroSelectionManager::ShouldPreAssignStagingBots() const
{
	if (bForceStagingBotPreAssignmentForTest)
	{
		return true;
	}
	return UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this);
}

void UBwayHeroSelectionManager::ReconcileStagingBotSelections()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (!ShouldPreAssignStagingBots() || !bSelectionActive)
	{
		if (StagingBotSelections.Num() > 0)
		{
			StagingBotSelections.Reset();
			OnStagingBotSelectionsChanged.Broadcast();
		}
		return;
	}

	const ABwayGameState* GameState = GetBwayGameState();
	int32 HumansPerTeam[2] = {0, 0};
	TSet<FPrimaryAssetId> HumanChosenHeroes[2];
	int32 TotalActiveHumans = 0;

	if (GameState)
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
			if (!BwayPS || BwayPS->IsInactive() || BwayPS->IsOnlyASpectator() || BwayPS->IsABot())
			{
				continue;
			}
			++TotalActiveHumans;
			int32 TeamIdx = GameState->GetPlayerTeam(BwayPS);
			if (TeamIdx < 0 || TeamIdx > 1)
			{
				TeamIdx = (HumansPerTeam[0] <= HumansPerTeam[1]) ? 0 : 1;
			}
			++HumansPerTeam[TeamIdx];
			if (BwayPS->GetSelectedHeroId().IsValid())
			{
				HumanChosenHeroes[TeamIdx].Add(BwayPS->GetSelectedHeroId());
			}
		}
	}

	int32 MaxBotsAllowed = 0;
	if (bForceStagingBotPreAssignmentForTest)
	{
		MaxBotsAllowed = FMath::Clamp(ForcedStagingMaxBotsForTest, 0, FMath::Max(0, 8 - TotalActiveHumans));
	}
	else
	{
		int32 UrlNumBots = -1;
		if (UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionInt(this, TEXT("NumBots"), UrlNumBots))
		{
			MaxBotsAllowed = FMath::Clamp(UrlNumBots, 0, FMath::Max(0, 8 - TotalActiveHumans));
		}
		else
		{
			MaxBotsAllowed = FMath::Max(0, 8 - FMath::Max(1, TotalActiveHumans));
		}
	}

	int32 DesiredBotsPerTeam[2] = {0, 0};
	for (int32 Assigned = 0; Assigned < MaxBotsAllowed; ++Assigned)
	{
		const int32 TotalTeam0 = HumansPerTeam[0] + DesiredBotsPerTeam[0];
		const int32 TotalTeam1 = HumansPerTeam[1] + DesiredBotsPerTeam[1];
		const bool bCanAddTeam0 = TotalTeam0 < 4;
		const bool bCanAddTeam1 = TotalTeam1 < 4;
		if (!bCanAddTeam0 && !bCanAddTeam1)
		{
			break;
		}
		if (bCanAddTeam0 && (!bCanAddTeam1 || TotalTeam0 <= TotalTeam1))
		{
			++DesiredBotsPerTeam[0];
		}
		else
		{
			++DesiredBotsPerTeam[1];
		}
	}

	TArray<FPrimaryAssetId> SortedHeroIds = GetRegisteredHeroIds();
	SortedHeroIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
	{
		return A.ToString() < B.ToString();
	});

	const bool bLockBots = AreAllPlayersReady();
	TArray<FBwayStagingBotSelection> Rebuilt;
	int32 RunningBotNumber = 1;

	for (int32 TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
	{
		TArray<FPrimaryAssetId> PreservedTeamBotHeroes;
		for (const FBwayStagingBotSelection& Existing : StagingBotSelections)
		{
			if (Existing.TeamIndex == TeamIdx
				&& Existing.SelectedHeroId.IsValid()
				&& !HumanChosenHeroes[TeamIdx].Contains(Existing.SelectedHeroId)
				&& !PreservedTeamBotHeroes.Contains(Existing.SelectedHeroId)
				&& SortedHeroIds.Contains(Existing.SelectedHeroId))
			{
				PreservedTeamBotHeroes.Add(Existing.SelectedHeroId);
			}
		}

		TSet<FPrimaryAssetId> TeamUsedHeroes = HumanChosenHeroes[TeamIdx];
		for (int32 SlotIdx = 0; SlotIdx < DesiredBotsPerTeam[TeamIdx]; ++SlotIdx)
		{
			FPrimaryAssetId AssignedHeroId;
			if (PreservedTeamBotHeroes.IsValidIndex(SlotIdx) && !TeamUsedHeroes.Contains(PreservedTeamBotHeroes[SlotIdx]))
			{
				AssignedHeroId = PreservedTeamBotHeroes[SlotIdx];
			}
			else if (SortedHeroIds.Num() > 0)
			{
				const int32 StartOffset = (TeamIdx * 2 + SlotIdx) % SortedHeroIds.Num();
				for (int32 Offset = 0; Offset < SortedHeroIds.Num(); ++Offset)
				{
					const FPrimaryAssetId& Candidate = SortedHeroIds[(StartOffset + Offset) % SortedHeroIds.Num()];
					if (!TeamUsedHeroes.Contains(Candidate))
					{
						AssignedHeroId = Candidate;
						break;
					}
				}
				if (!AssignedHeroId.IsValid())
				{
					AssignedHeroId = SortedHeroIds[StartOffset];
				}
			}

			if (AssignedHeroId.IsValid())
			{
				TeamUsedHeroes.Add(AssignedHeroId);
			}

			FBwayStagingBotSelection Entry;
			Entry.TeamIndex = TeamIdx;
			Entry.BotSlotIndex = SlotIdx;
			Entry.BotDisplayName = FString::Printf(TEXT("Breakaway Bot %d"), RunningBotNumber++);
			Entry.SelectedHeroId = AssignedHeroId;
			Entry.bIsLocked = bLockBots;
			Rebuilt.Add(Entry);
		}
	}

	bool bChanged = (Rebuilt.Num() != StagingBotSelections.Num());
	if (!bChanged)
	{
		for (int32 i = 0; i < Rebuilt.Num(); ++i)
		{
			if (Rebuilt[i].TeamIndex != StagingBotSelections[i].TeamIndex
				|| Rebuilt[i].BotSlotIndex != StagingBotSelections[i].BotSlotIndex
				|| Rebuilt[i].BotDisplayName != StagingBotSelections[i].BotDisplayName
				|| Rebuilt[i].SelectedHeroId != StagingBotSelections[i].SelectedHeroId
				|| Rebuilt[i].bIsLocked != StagingBotSelections[i].bIsLocked)
			{
				bChanged = true;
				break;
			}
		}
	}

	if (bChanged)
	{
		StagingBotSelections = MoveTemp(Rebuilt);
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->ForceNetUpdate();
		}
		OnStagingBotSelectionsChanged.Broadcast();
	}
}

FString UBwayHeroSelectionManager::BuildStagingBotHeroesOptionForTeam(int32 TeamIndex) const
{
	TArray<FString> Tokens;
	for (const FBwayStagingBotSelection& BotSel : StagingBotSelections)
	{
		if (BotSel.TeamIndex == TeamIndex && BotSel.SelectedHeroId.IsValid())
		{
			// Serialize PrimaryAssetName only (no ':') because AGameModeBase::CanServerTravel rejects URLs containing ':' or '\'.
			Tokens.Add(BotSel.SelectedHeroId.PrimaryAssetName.ToString());
		}
	}
	return FString::Join(Tokens, TEXT(","));
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
