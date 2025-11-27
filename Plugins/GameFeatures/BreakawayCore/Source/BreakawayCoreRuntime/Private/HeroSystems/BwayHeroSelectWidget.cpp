// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeroSystems/BwayHeroSelectWidget.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

UBwayHeroSelectWidget::UBwayHeroSelectWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayHeroSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Get references to required systems
	if (APlayerController* PC = GetOwningPlayer())
	{
		LocalPlayerState = PC->GetPlayerState<ABwayPlayerState>();
	}

	if (UWorld* World = GetWorld())
	{
		HeroRegistry = UBwayHeroRegistry::Get(World);

		if (AGameStateBase* GameState = World->GetGameState())
		{
			SelectionManager = GameState->FindComponentByClass<UBwayHeroSelectionManager>();
		}
	}

	if (!LocalPlayerState)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectWidget: Failed to get local player state"));
	}

	if (!HeroRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectWidget: Failed to get hero registry"));
	}

	if (!SelectionManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: No selection manager found (this is OK if not in selection phase)"));
	}
}

void UBwayHeroSelectWidget::NativeDestruct()
{
	UnbindFromPlayerState();
	UnbindFromSelectionManager();

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionTimerHandle);
	}

	Super::NativeDestruct();
}

void UBwayHeroSelectWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Bind to systems when widget is activated
	BindToPlayerState();
	BindToSelectionManager();

	// Start selection timer if there's a time limit
	if (SelectionManager && SelectionManager->SelectionTimeLimit > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				SelectionTimerHandle,
				this,
				&UBwayHeroSelectWidget::UpdateSelectionTimer,
				0.1f,  // Update 10 times per second
				true
			);
		}
	}

	// Trigger initial refresh
	OnHeroListChanged();

	bIsInitialized = true;
}

void UBwayHeroSelectWidget::NativeOnDeactivated()
{
	UnbindFromPlayerState();
	UnbindFromSelectionManager();

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionTimerHandle);
	}

	bIsInitialized = false;

	Super::NativeOnDeactivated();
}

// ========== PUBLIC API ==========

TArray<FHeroDisplayInfo> UBwayHeroSelectWidget::GetAvailableHeroes()
{
	TArray<FHeroDisplayInfo> DisplayInfos;

	if (!HeroRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectWidget: No hero registry available"));
		return DisplayInfos;
	}

	// Get all hero soft references
	TArray<TSoftObjectPtr<UBwayHeroDataAsset>> HeroSoftRefs = HeroRegistry->GetAllHeroSoftObjects();

	int32 LocalTeam = GetLocalPlayerTeam();
	FPrimaryAssetId CurrentSelection = GetSelectedHeroId();

	// Load and process each hero
	for (const TSoftObjectPtr<UBwayHeroDataAsset>& SoftRef : HeroSoftRefs)
	{
		// Sync load the hero data
		UBwayHeroDataAsset* HeroData = HeroRegistry->LoadHeroSync(SoftRef);
		if (!HeroData)
		{
			continue;
		}

		// Create display info
		FHeroDisplayInfo DisplayInfo;
		DisplayInfo.HeroId = HeroData->GetPrimaryAssetId();
		DisplayInfo.DisplayName = HeroData->DisplayName;
		DisplayInfo.Portrait = HeroData->Portrait;
		DisplayInfo.Stats = HeroData->HeroStats;

		// Check availability
		if (SelectionManager)
		{
			DisplayInfo.bIsAvailable = SelectionManager->IsHeroAvailableForTeam(DisplayInfo.HeroId, LocalTeam);
		}
		else
		{
			DisplayInfo.bIsAvailable = true;
		}

		// Check if selected by local player
		DisplayInfo.bIsSelected = (DisplayInfo.HeroId == CurrentSelection);

		// Check if locked
		DisplayInfo.bIsLocked = IsSelectionLocked();

		DisplayInfos.Add(DisplayInfo);
	}

	return DisplayInfos;
}

FPrimaryAssetId UBwayHeroSelectWidget::GetSelectedHeroId() const
{
	if (LocalPlayerState)
	{
		return LocalPlayerState->GetSelectedHeroId();
	}
	return FPrimaryAssetId();
}

bool UBwayHeroSelectWidget::IsSelectionLocked() const
{
	if (LocalPlayerState)
	{
		return LocalPlayerState->IsHeroLocked();
	}
	return false;
}

bool UBwayHeroSelectWidget::SelectHero(FPrimaryAssetId HeroId)
{
	if (!LocalPlayerState)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectWidget: Cannot select hero - no player state"));
		return false;
	}

	if (IsSelectionLocked())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: Cannot select hero - selection is locked"));
		return false;
	}

	if (!IsHeroAvailable(HeroId))
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: Hero %s is not available"), *HeroId.ToString());
		return false;
	}

	// Send selection to server
	LocalPlayerState->ServerSetSelectedHeroId(HeroId);

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Selected hero %s"), *HeroId.ToString());
	return true;
}

bool UBwayHeroSelectWidget::LockSelection()
{
	if (!LocalPlayerState)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectWidget: Cannot lock selection - no player state"));
		return false;
	}

	if (IsSelectionLocked())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: Selection already locked"));
		return true;
	}

	FPrimaryAssetId SelectedHero = GetSelectedHeroId();
	if (!SelectedHero.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: Cannot lock - no hero selected"));
		return false;
	}

	// Send lock request to server
	LocalPlayerState->ServerLockHeroSelection();

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Locked selection"));
	return true;
}

int32 UBwayHeroSelectWidget::GetLocalPlayerTeam() const
{
	if (LocalPlayerState)
	{
		return LocalPlayerState->GetTeamId();
	}
	return -1;
}

bool UBwayHeroSelectWidget::IsHeroAvailable(FPrimaryAssetId HeroId) const
{
	if (!SelectionManager)
	{
		return true;  // If no manager, assume all heroes available
	}

	int32 LocalTeam = GetLocalPlayerTeam();
	return SelectionManager->IsHeroAvailableForTeam(HeroId, LocalTeam);
}

float UBwayHeroSelectWidget::GetRemainingSelectionTime() const
{
	if (!SelectionManager || SelectionManager->SelectionTimeLimit <= 0.0f)
	{
		return -1.0f;  // No time limit
	}

	// This would need to be tracked by the selection manager
	// For now, return -1 to indicate unknown
	// TODO: Add time tracking to selection manager
	return -1.0f;
}

void UBwayHeroSelectWidget::GetReadyPlayerCount(int32& OutReady, int32& OutTotal) const
{
	if (SelectionManager)
	{
		OutReady = SelectionManager->GetNumPlayersReady();
		OutTotal = SelectionManager->GetTotalPlayers();
	}
	else
	{
		OutReady = 0;
		OutTotal = 0;
	}
}

// ========== BINDING ==========

void UBwayHeroSelectWidget::BindToPlayerState()
{
	if (!LocalPlayerState)
	{
		return;
	}

	// Bind to hero selection changed
	LocalPlayerState->OnSelectedHeroChanged.AddDynamic(this, &UBwayHeroSelectWidget::HandlePlayerSelectionChanged);

	UE_LOG(LogTemp, Verbose, TEXT("BwayHeroSelectWidget: Bound to player state"));
}

void UBwayHeroSelectWidget::UnbindFromPlayerState()
{
	if (!LocalPlayerState)
	{
		return;
	}

	// Unbind from hero selection changed
	LocalPlayerState->OnSelectedHeroChanged.RemoveDynamic(this, &UBwayHeroSelectWidget::HandlePlayerSelectionChanged);

	UE_LOG(LogTemp, Verbose, TEXT("BwayHeroSelectWidget: Unbound from player state"));
}

void UBwayHeroSelectWidget::BindToSelectionManager()
{
	if (!SelectionManager)
	{
		return;
	}

	// Bind to manager events
	SelectionManager->OnPlayerHeroSelectionChanged.AddDynamic(this, &UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged);
	SelectionManager->OnAllPlayersReady.AddDynamic(this, &UBwayHeroSelectWidget::HandleAllPlayersReady);

	UE_LOG(LogTemp, Verbose, TEXT("BwayHeroSelectWidget: Bound to selection manager"));
}

void UBwayHeroSelectWidget::UnbindFromSelectionManager()
{
	if (!SelectionManager)
	{
		return;
	}

	// Unbind from manager events
	SelectionManager->OnPlayerHeroSelectionChanged.RemoveDynamic(this, &UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged);
	SelectionManager->OnAllPlayersReady.RemoveDynamic(this, &UBwayHeroSelectWidget::HandleAllPlayersReady);

	UE_LOG(LogTemp, Verbose, TEXT("BwayHeroSelectWidget: Unbound from selection manager"));
}

// ========== DELEGATE HANDLERS ==========

void UBwayHeroSelectWidget::HandlePlayerSelectionChanged(const FPrimaryAssetId& NewHeroId)
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Local player selection changed to %s"), *NewHeroId.ToString());

	// Notify blueprint
	OnLocalSelectionChanged(NewHeroId);

	// Refresh hero list (availability may have changed)
	OnHeroListChanged();
}

void UBwayHeroSelectWidget::HandlePlayerHeroLocked(FPrimaryAssetId LockedHeroId)
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Local player locked hero %s"), *LockedHeroId.ToString());

	// Notify blueprint
	OnLocalSelectionLocked(LockedHeroId);
}

void UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged(APlayerState* PlayerState, FPrimaryAssetId HeroId, int32 TeamIndex)
{
	// Only care about players on our team (they affect hero availability)
	if (TeamIndex == GetLocalPlayerTeam())
	{
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Team member selected hero %s"), *HeroId.ToString());
		
		// Refresh hero list (availability changed)
		OnHeroListChanged();
	}

	// Notify blueprint about any player selection
	OnOtherPlayerSelectionChanged(PlayerState, HeroId, TeamIndex);
}

void UBwayHeroSelectWidget::HandleAllPlayersReady()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: All players ready!"));

	// Notify blueprint
	OnAllPlayersReady();
}

void UBwayHeroSelectWidget::UpdateSelectionTimer()
{
	// This would need time tracking in selection manager
	// For now, just call the BP event with a placeholder
	// TODO: Implement proper time tracking
	OnSelectionTimerUpdated(0.0f);
}
