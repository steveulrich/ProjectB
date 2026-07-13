// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeroSystems/BwayHeroSelectWidget.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayFrontendStateSubsystem.h"
#include "HeroSystems/BwayHeroAbilityUILibrary.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "TimerManager.h"


UBwayHeroSelectWidget::UBwayHeroSelectWidget(
    const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer) {}

void UBwayHeroSelectWidget::NativeConstruct() {
  Super::NativeConstruct();

  // Get references to required systems
  if (APlayerController *PC = GetOwningPlayer()) {
    LocalPlayerState = PC->GetPlayerState<ABwayPlayerState>();
  }

  if (UWorld *World = GetWorld()) {
    HeroRegistry = UBwayHeroRegistry::Get(World);

    if (AGameStateBase *GameState = World->GetGameState()) {
      SelectionManager =
          GameState->FindComponentByClass<UBwayHeroSelectionManager>();
    }
  }

  if (ULocalPlayer *LP = GetOwningLocalPlayer()) {
    FrontendSubsystem = LP->GetSubsystem<UBwayFrontendStateSubsystem>();
  }

  if (!LocalPlayerState && !FrontendSubsystem) {
    UE_LOG(LogTemp, Error,
           TEXT("BwayHeroSelectWidget: Failed to get local player state or frontend subsystem"));
  } else if (!LocalPlayerState) {
    UE_LOG(LogTemp, Log,
           TEXT("BwayHeroSelectWidget: No player state (Frontend mode - using FrontendSubsystem)"));
  }

  if (!HeroRegistry) {
    UE_LOG(LogTemp, Error,
           TEXT("BwayHeroSelectWidget: Failed to get hero registry"));
  }

  if (!SelectionManager) {
    UE_LOG(LogTemp, Warning,
           TEXT("BwayHeroSelectWidget: No selection manager found (this is OK "
                "if not in selection phase)"));
  }
}

void UBwayHeroSelectWidget::NativeDestruct() {
  UnbindFromPlayerState();
  UnbindFromSelectionManager();

  // Clear timer
  if (UWorld *World = GetWorld()) {
    World->GetTimerManager().ClearTimer(SelectionTimerHandle);
  }

  Super::NativeDestruct();
}

void UBwayHeroSelectWidget::NativeOnActivated() {
  Super::NativeOnActivated();

  // Bind to systems when widget is activated
  BindToPlayerState();
  BindToSelectionManager();

  // Start selection timer if there's a time limit
  if (SelectionManager && SelectionManager->SelectionTimeLimit > 0.0f) {
    if (UWorld *World = GetWorld()) {
      World->GetTimerManager().SetTimer(
          SelectionTimerHandle, this,
          &UBwayHeroSelectWidget::UpdateSelectionTimer,
          0.1f, // Update 10 times per second
          true);
    }
  }

  // Trigger initial refresh
  OnHeroListChanged();

  bIsInitialized = true;
}

void UBwayHeroSelectWidget::NativeOnDeactivated() {
  UnbindFromPlayerState();
  UnbindFromSelectionManager();

  // Clear timer
  if (UWorld *World = GetWorld()) {
    World->GetTimerManager().ClearTimer(SelectionTimerHandle);
  }

  bIsInitialized = false;

  Super::NativeOnDeactivated();
}

// ========== PUBLIC API ==========

TArray<FHeroDisplayInfo> UBwayHeroSelectWidget::GetAvailableHeroes() {
  TArray<FHeroDisplayInfo> DisplayInfos;

  if (!HeroRegistry) {
    UE_LOG(LogTemp, Error,
           TEXT("BwayHeroSelectWidget: No hero registry available"));
    return DisplayInfos;
  }

  // Get all hero soft references
  TArray<TSoftObjectPtr<UBwayHeroDataAsset>> HeroSoftRefs =
      HeroRegistry->GetAllHeroSoftObjects();

  int32 LocalTeam = GetLocalPlayerTeam();
  FPrimaryAssetId CurrentSelection = GetSelectedHeroId();
  TMap<uint8, int32> ClassCounts;
  const TArray<FPlayerHeroSelectionState> PlayerSelections =
      SelectionManager ? SelectionManager->GetAllPlayerSelections() : TArray<FPlayerHeroSelectionState>();

  // Load and process each hero
  for (const TSoftObjectPtr<UBwayHeroDataAsset> &SoftRef : HeroSoftRefs) {
    // Sync load the hero data
    UBwayHeroDataAsset *HeroData = HeroRegistry->LoadHeroSync(SoftRef);
    if (!HeroData) {
      continue;
    }

    // Create display info
    FHeroDisplayInfo DisplayInfo;
    DisplayInfo.HeroId = HeroData->GetPrimaryAssetId();
    DisplayInfo.DisplayName = HeroData->DisplayName;
    DisplayInfo.Portrait = HeroData->Portrait;
    DisplayInfo.Stats = HeroData->HeroStats;

    // Populate class information
    DisplayInfo.HeroClass = HeroData->HeroClass;
    DisplayInfo.ClassName = HeroData->GetClassDisplayName();
    int32& ClassCount = ClassCounts.FindOrAdd(static_cast<uint8>(DisplayInfo.HeroClass));
    DisplayInfo.HeroClassIndex = ClassCount++;

    // Populate ability display info from ability set grants + CDO display data
    DisplayInfo.Abilities = UBwayHeroAbilityUILibrary::ResolveAbilityBarForHero(HeroData);

    // Check availability
    if (SelectionManager) {
      DisplayInfo.bIsAvailable = (DisplayInfo.HeroId == CurrentSelection) ||
          SelectionManager->IsHeroAvailableForTeam(DisplayInfo.HeroId, LocalTeam);
    } else {
      DisplayInfo.bIsAvailable = true;
    }

    // Check if selected by local player
    DisplayInfo.bIsSelected = (DisplayInfo.HeroId == CurrentSelection);

    // Check if locked by local player
    DisplayInfo.bIsLocked = IsSelectionLocked() && DisplayInfo.bIsSelected;

    // Check which player has selected this hero (for P1/P2 indicators)
    DisplayInfo.SelectedByPlayerIndex = -1;
    for (int32 SelectionIndex = 0; SelectionIndex < PlayerSelections.Num(); ++SelectionIndex) {
      const FPlayerHeroSelectionState& Selection = PlayerSelections[SelectionIndex];
      if (Selection.SelectedHeroId == DisplayInfo.HeroId) {
        if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState)) {
          DisplayInfo.SelectedByPlayerIndex = BwayPS->GetPlayerNum();
        } else {
          DisplayInfo.SelectedByPlayerIndex = SelectionIndex;
        }
        break;
      }
    }

    DisplayInfos.Add(DisplayInfo);
  }

  return DisplayInfos;
}

FPrimaryAssetId UBwayHeroSelectWidget::GetSelectedHeroId() const {
  if (LocalPlayerState) {
    return LocalPlayerState->GetSelectedHeroId();
  }

  if (FrontendSubsystem) {
    return FrontendSubsystem->GetSelectedHeroId();
  }

  return FPrimaryAssetId();
}

bool UBwayHeroSelectWidget::IsSelectionLocked() const {
  if (LocalPlayerState) {
    return LocalPlayerState->IsHeroLocked();
  }
  return false;
}

bool UBwayHeroSelectWidget::SelectHero(FPrimaryAssetId HeroId) {
  if (!IsHeroAvailable(HeroId)) {
    UE_LOG(LogTemp, Warning,
           TEXT("BwayHeroSelectWidget: Hero %s is not available"),
           *HeroId.ToString());
    return false;
  }

  if (LocalPlayerState) {
    if (IsSelectionLocked()) {
      UE_LOG(LogTemp, Warning,
             TEXT("BwayHeroSelectWidget: Cannot select hero - selection is "
                  "locked"));
      return false;
    }

    if (FrontendSubsystem) {
      FrontendSubsystem->SetSelectedHeroId(HeroId);
    }

    // Send selection to server
    LocalPlayerState->ServerSetSelectedHeroId(HeroId);
    UE_LOG(LogTemp, Log,
           TEXT("BwayHeroSelectWidget: Selected hero %s (Server Route)"),
           *HeroId.ToString());
  } else if (FrontendSubsystem) {
    // Offline Front-end route
    FrontendSubsystem->SetSelectedHeroId(HeroId);
    OnLocalSelectionChanged(HeroId);
    OnHeroListChanged(); // Force redraw the selection highlight locally
    UE_LOG(LogTemp, Log,
           TEXT("BwayHeroSelectWidget: Selected hero %s (Frontend Route)"),
           *HeroId.ToString());
  } else {
    UE_LOG(LogTemp, Error,
           TEXT("BwayHeroSelectWidget: Cannot select hero - neither player "
                "state nor frontend subsystem found"));
    return false;
  }

  return true;
}

bool UBwayHeroSelectWidget::LockSelection() {
  if (!LocalPlayerState) {
    if (FrontendSubsystem) {
      // Offline frontend mode - no server lock needed
      UE_LOG(LogTemp, Log,
             TEXT("BwayHeroSelectWidget: Locked selection (Frontend Route)"));
      return true;
    }
    UE_LOG(
        LogTemp, Error,
        TEXT("BwayHeroSelectWidget: Cannot lock selection - no player state"));
    return false;
  }

  if (IsSelectionLocked()) {
    UE_LOG(LogTemp, Warning,
           TEXT("BwayHeroSelectWidget: Selection already locked"));
    return true;
  }

  FPrimaryAssetId SelectedHero = GetSelectedHeroId();
  if (!SelectedHero.IsValid()) {
    UE_LOG(LogTemp, Warning,
           TEXT("BwayHeroSelectWidget: Cannot lock - no hero selected"));
    return false;
  }

  // Send lock request to server
  LocalPlayerState->ServerLockHeroSelection();

  UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Locked selection"));
  return true;
}

void UBwayHeroSelectWidget::ConfirmSelection() {
  FPrimaryAssetId SelectedHero = GetSelectedHeroId();
  if (!SelectedHero.IsValid()) {
    UE_LOG(LogTemp, Warning,
           TEXT("BwayHeroSelectWidget: Cannot confirm - no hero selected"));
    return;
  }

  UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Confirming selection of %s"),
         *SelectedHero.ToString());

  // Lock the selection if not already locked
  if (!IsSelectionLocked()) {
    LockSelection();
  }

  // Broadcast the confirmation event (dev tools listen for this)
  OnHeroConfirmed.Broadcast();

  // Deactivate/close the widget
  DeactivateWidget();
}

int32 UBwayHeroSelectWidget::GetLocalPlayerTeam() const {
  if (LocalPlayerState) {
    if (const ABwayGameState* BwayGS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr) {
      return BwayGS->GetPlayerTeam(LocalPlayerState);
    }
  }
  return -1;
}

bool UBwayHeroSelectWidget::IsHeroAvailable(FPrimaryAssetId HeroId) const {
  if (!SelectionManager) {
    return true; // If no manager, assume all heroes available
  }

  int32 LocalTeam = GetLocalPlayerTeam();
  return SelectionManager->IsHeroAvailableForTeam(HeroId, LocalTeam);
}

float UBwayHeroSelectWidget::GetRemainingSelectionTime() const {
  if (!SelectionManager || SelectionManager->SelectionTimeLimit <= 0.0f) {
    return -1.0f; // No time limit
  }

  return SelectionManager->GetSelectionTimeRemaining();
}

void UBwayHeroSelectWidget::GetReadyPlayerCount(int32 &OutReady,
                                                int32 &OutTotal) const {
  if (SelectionManager) {
    OutReady = SelectionManager->GetNumPlayersReady();
    OutTotal = SelectionManager->GetTotalPlayers();
  } else {
    OutReady = 0;
    OutTotal = 0;
  }
}

UBwayHeroDataAsset *
UBwayHeroSelectWidget::GetHeroDataAsset(FPrimaryAssetId HeroId) const {
  if (!HeroId.IsValid()) {
    return nullptr;
  }

  return UBwayHeroRegistry::GetHeroDataById(HeroId);
}

bool UBwayHeroSelectWidget::GetHeroDisplayInfo(
    FPrimaryAssetId HeroId, FHeroDisplayInfo &OutDisplayInfo) const {
  UBwayHeroDataAsset *HeroData = GetHeroDataAsset(HeroId);
  if (!HeroData) {
    return false;
  }

  OutDisplayInfo.HeroId = HeroData->GetPrimaryAssetId();
  OutDisplayInfo.DisplayName = HeroData->DisplayName;
  OutDisplayInfo.Portrait = HeroData->Portrait;
  OutDisplayInfo.Stats = HeroData->HeroStats;
  OutDisplayInfo.HeroClass = HeroData->HeroClass;
  OutDisplayInfo.ClassName = HeroData->GetClassDisplayName();
  OutDisplayInfo.Abilities = UBwayHeroAbilityUILibrary::ResolveAbilityBarForHero(HeroData);
  OutDisplayInfo.HeroClassIndex = 0;

  int32 LocalTeam = GetLocalPlayerTeam();
  FPrimaryAssetId CurrentSelection = GetSelectedHeroId();

  // Check availability
  if (SelectionManager) {
    OutDisplayInfo.bIsAvailable = (OutDisplayInfo.HeroId == CurrentSelection) ||
        SelectionManager->IsHeroAvailableForTeam(OutDisplayInfo.HeroId, LocalTeam);
  } else {
    OutDisplayInfo.bIsAvailable = true;
  }

  // Check if selected by local player
  OutDisplayInfo.bIsSelected = (OutDisplayInfo.HeroId == CurrentSelection);
  OutDisplayInfo.bIsLocked = IsSelectionLocked() && OutDisplayInfo.bIsSelected;
  OutDisplayInfo.SelectedByPlayerIndex = -1;
  if (SelectionManager) {
    const TArray<FPlayerHeroSelectionState> PlayerSelections = SelectionManager->GetAllPlayerSelections();
    for (int32 SelectionIndex = 0; SelectionIndex < PlayerSelections.Num(); ++SelectionIndex) {
      const FPlayerHeroSelectionState& Selection = PlayerSelections[SelectionIndex];
      if (Selection.SelectedHeroId == OutDisplayInfo.HeroId) {
        if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState)) {
          OutDisplayInfo.SelectedByPlayerIndex = BwayPS->GetPlayerNum();
        } else {
          OutDisplayInfo.SelectedByPlayerIndex = SelectionIndex;
        }
        break;
      }
    }
  }

  return true;
}

bool UBwayHeroSelectWidget::GetSelectedHeroDisplayInfo(
    FHeroDisplayInfo &OutDisplayInfo) const {
  FPrimaryAssetId SelectedId = GetSelectedHeroId();
  if (!SelectedId.IsValid()) {
    return false;
  }

  return GetHeroDisplayInfo(SelectedId, OutDisplayInfo);
}

// ========== BINDING ==========

void UBwayHeroSelectWidget::BindToPlayerState() {
  if (!LocalPlayerState) {
    return;
  }

  // Bind to hero selection changed
  LocalPlayerState->OnSelectedHeroChanged.AddDynamic(
      this, &UBwayHeroSelectWidget::HandlePlayerSelectionChanged);

  // Bind to hero locked (fires when bHeroLocked replicates true)
  LocalPlayerState->OnHeroLocked.AddDynamic(
      this, &UBwayHeroSelectWidget::HandlePlayerHeroLocked);

  UE_LOG(LogTemp, Verbose, TEXT("BwayHeroSelectWidget: Bound to player state"));
}

void UBwayHeroSelectWidget::UnbindFromPlayerState() {
  if (!LocalPlayerState) {
    return;
  }

  // Unbind from hero selection changed
  LocalPlayerState->OnSelectedHeroChanged.RemoveDynamic(
      this, &UBwayHeroSelectWidget::HandlePlayerSelectionChanged);

  // Unbind from hero locked
  LocalPlayerState->OnHeroLocked.RemoveDynamic(
      this, &UBwayHeroSelectWidget::HandlePlayerHeroLocked);

  UE_LOG(LogTemp, Verbose,
         TEXT("BwayHeroSelectWidget: Unbound from player state"));
}

void UBwayHeroSelectWidget::BindToSelectionManager() {
  if (!SelectionManager) {
    return;
  }

  // Bind to manager events
  SelectionManager->OnPlayerHeroSelectionChanged.AddDynamic(
      this, &UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged);
  SelectionManager->OnAllPlayersReady.AddDynamic(
      this, &UBwayHeroSelectWidget::HandleAllPlayersReady);

  UE_LOG(LogTemp, Verbose,
         TEXT("BwayHeroSelectWidget: Bound to selection manager"));
}

void UBwayHeroSelectWidget::UnbindFromSelectionManager() {
  if (!SelectionManager) {
    return;
  }

  // Unbind from manager events
  SelectionManager->OnPlayerHeroSelectionChanged.RemoveDynamic(
      this, &UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged);
  SelectionManager->OnAllPlayersReady.RemoveDynamic(
      this, &UBwayHeroSelectWidget::HandleAllPlayersReady);

  UE_LOG(LogTemp, Verbose,
         TEXT("BwayHeroSelectWidget: Unbound from selection manager"));
}

// ========== DELEGATE HANDLERS ==========

void UBwayHeroSelectWidget::HandlePlayerSelectionChanged(
    FPrimaryAssetId NewHeroId) {
  UE_LOG(LogTemp, Log,
         TEXT("BwayHeroSelectWidget: Local player selection changed to %s"),
         *NewHeroId.ToString());

  // Notify blueprint
  OnLocalSelectionChanged(NewHeroId);

  // Refresh hero list (availability may have changed)
  OnHeroListChanged();
}

void UBwayHeroSelectWidget::HandlePlayerHeroLocked(
    FPrimaryAssetId LockedHeroId) {
  UE_LOG(LogTemp, Log,
         TEXT("BwayHeroSelectWidget: Local player locked hero %s"),
         *LockedHeroId.ToString());

  // Notify blueprint
  OnLocalSelectionLocked(LockedHeroId);
}

void UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged(
    APlayerState *PlayerState, FPrimaryAssetId HeroId, int32 TeamIndex) {
  // Only care about players on our team (they affect hero availability)
  if (TeamIndex == GetLocalPlayerTeam()) {
    UE_LOG(LogTemp, Log,
           TEXT("BwayHeroSelectWidget: Team member selected hero %s"),
           *HeroId.ToString());

    // Refresh hero list (availability changed)
    OnHeroListChanged();
  }

  // Notify blueprint about any player selection
  OnOtherPlayerSelectionChanged(PlayerState, HeroId, TeamIndex);
}

void UBwayHeroSelectWidget::HandleAllPlayersReady() {
  UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: All players ready!"));

  // Notify blueprint
  OnAllPlayersReady();
}

void UBwayHeroSelectWidget::UpdateSelectionTimer() {
  OnSelectionTimerUpdated(GetRemainingSelectionTime());
}
