// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeroSystems/BwayHeroSelectWidget.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "BwayPlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayFrontendStateSubsystem.h"
#include "HeroSystems/BwayHeroAbilityUILibrary.h"
#include "HeroSystems/BwayHeroPreviewActor.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "UI/Foundation/LyraButtonBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "CommonTextBlock.h"
#include "CommonInputModeTypes.h"


UBwayHeroSelectWidget::UBwayHeroSelectWidget(
    const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer) {}

TOptional<FUIInputConfig> UBwayHeroSelectWidget::GetDesiredInputConfig() const {
  return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

TSharedRef<SWidget> UBwayHeroSelectWidget::RebuildWidget() {
  UOverlay* Root = nullptr;
  if (WidgetTree && !WidgetTree->RootWidget) {
    Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HeroSelectRootOverlay"));
    WidgetTree->RootWidget = Root;
  } else if (WidgetTree) {
    if (UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
        ExistingRoot && ExistingRoot->GetChildrenCount() == 0) {
      Root = Cast<UOverlay>(ExistingRoot);
      if (!Root) {
        Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HeroSelectRootOverlay"));
        ExistingRoot->AddChild(Root);
      }
    }
  }

  if (Root) {
    bUsingFallbackLayout = true;
    const FLinearColor GoldColor(0.95f, 0.78f, 0.32f, 1.0f);
    const FLinearColor CyanColor(0.18f, 0.72f, 0.92f, 1.0f);
    const FLinearColor DarkPanel(0.03f, 0.06f, 0.11f, 0.90f);

    auto MakeTxt = [this](const FText& Txt, int32 Size, const FLinearColor& Col) {
      UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
      Block->SetText(Txt);
      Block->SetColorAndOpacity(FSlateColor(Col));
      FSlateFontInfo Font = Block->GetFont();
      Font.Size = Size;
      Block->SetFont(Font);
      return Block;
    };

    // Left-side Column: Selected Hero Header + 3-Column Skewed Portrait Grid + Currency + READY!
    UVerticalBox* LeftColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    UOverlaySlot* LeftSlot = Root->AddChildToOverlay(LeftColumn);
    LeftSlot->SetHorizontalAlignment(HAlign_Left);
    LeftSlot->SetVerticalAlignment(VAlign_Center);
    LeftSlot->SetPadding(FMargin(56.f, 32.f, 0.f, 32.f));

    // Top-left Selected Hero Header (Name, Role/Class subtitle, Level 99)
    UBorder* HeaderBanner = WidgetTree->ConstructWidget<UBorder>();
    HeaderBanner->SetBrushColor(DarkPanel);
    HeaderBanner->SetPadding(FMargin(20.f, 12.f));
    LeftColumn->AddChildToVerticalBox(HeaderBanner)->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));

    UHorizontalBox* HeaderHBox = WidgetTree->ConstructWidget<UHorizontalBox>();
    HeaderBanner->SetContent(HeaderHBox);

    UVerticalBox* NameRoleBox = WidgetTree->ConstructWidget<UVerticalBox>();
    FallbackHeroNameText = MakeTxt(NSLOCTEXT("BwayHeroSelect", "DefaultHeroName", "RAWLINS"), 32, FLinearColor::White);
    FallbackHeroClassText = MakeTxt(NSLOCTEXT("BwayHeroSelect", "DefaultHeroRole", "SHOOTER"), 16, GoldColor);
    NameRoleBox->AddChildToVerticalBox(FallbackHeroNameText);
    NameRoleBox->AddChildToVerticalBox(FallbackHeroClassText);
    HeaderHBox->AddChildToHorizontalBox(NameRoleBox)->SetPadding(FMargin(0.f, 0.f, 28.f, 0.f));

    UBorder* LevelBadge = WidgetTree->ConstructWidget<UBorder>();
    LevelBadge->SetBrushColor(FLinearColor(0.08f, 0.14f, 0.22f, 0.95f));
    LevelBadge->SetPadding(FMargin(12.f, 8.f));
    FallbackHeroLevelText = MakeTxt(NSLOCTEXT("BwayHeroSelect", "DefaultHeroLvl", "99"), 22, CyanColor);
    LevelBadge->SetContent(FallbackHeroLevelText);
    HeaderHBox->AddChildToHorizontalBox(LevelBadge)->SetVerticalAlignment(VAlign_Center);

    // 3-Column Skewed Hero Portrait Grid
    USizeBox* GridSizeBox = WidgetTree->ConstructWidget<USizeBox>();
    GridSizeBox->SetWidthOverride(420.f);
    FWidgetTransform SkewTransform;
    SkewTransform.Shear = FVector2D(-5.0f, 0.0f);
    GridSizeBox->SetRenderTransform(SkewTransform);

    FallbackPortraitGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
    FallbackPortraitGrid->SetSlotPadding(FMargin(6.f));
    GridSizeBox->AddChild(FallbackPortraitGrid);
    LeftColumn->AddChildToVerticalBox(GridSizeBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));

    // Currency Display + READY! Button
    UBorder* CurrencyBar = WidgetTree->ConstructWidget<UBorder>();
    CurrencyBar->SetBrushColor(DarkPanel);
    CurrencyBar->SetPadding(FMargin(16.f, 8.f));
    UTextBlock* CurrencyText = MakeTxt(NSLOCTEXT("BwayHeroSelect", "CurrencyReadout", "GOLD: 564100"), 15, GoldColor);
    CurrencyBar->SetContent(CurrencyText);
    LeftColumn->AddChildToVerticalBox(CurrencyBar)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    USizeBox* ReadyBtnSize = WidgetTree->ConstructWidget<USizeBox>();
    ReadyBtnSize->SetWidthOverride(280.f);
    FallbackReadyButton = WidgetTree->ConstructWidget<UButton>();
    FallbackReadyButton->SetBackgroundColor(GoldColor);
    UBorder* ReadyPad = WidgetTree->ConstructWidget<UBorder>();
    ReadyPad->SetBrushColor(FLinearColor::Transparent);
    ReadyPad->SetPadding(FMargin(24.f, 12.f));
    FallbackReadyLabel = MakeTxt(NSLOCTEXT("BwayHeroSelect", "ReadyBtn", "READY!"), 24, FLinearColor::White);
    FallbackReadyLabel->SetJustification(ETextJustify::Center);
    ReadyPad->SetContent(FallbackReadyLabel);
    FallbackReadyButton->AddChild(ReadyPad);
    FallbackReadyButton->OnClicked.AddDynamic(this, &ThisClass::OnFallbackReadyClicked);
    ReadyBtnSize->AddChild(FallbackReadyButton);
    LeftColumn->AddChildToVerticalBox(ReadyBtnSize);

    // Bottom-Right Sub-Panels (SKINS, TAUNTS, WEAPONS, ABILITIES toggle, ASSEMBLE TEAM 99 countdown)
    UVerticalBox* BottomRightColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    UOverlaySlot* BottomRightSlot = Root->AddChildToOverlay(BottomRightColumn);
    BottomRightSlot->SetHorizontalAlignment(HAlign_Right);
    BottomRightSlot->SetVerticalAlignment(VAlign_Bottom);
    BottomRightSlot->SetPadding(FMargin(0.f, 0.f, 56.f, 36.f));

    // Collapsible ABILITIES details panel
    FallbackAbilitiesPanel = WidgetTree->ConstructWidget<UBorder>();
    FallbackAbilitiesPanel->SetBrushColor(DarkPanel);
    FallbackAbilitiesPanel->SetPadding(FMargin(18.f, 14.f));
    FallbackAbilitiesPanel->SetVisibility(ESlateVisibility::Collapsed);
    FallbackAbilitiesText = MakeTxt(FText::GetEmpty(), 14, FLinearColor::White);
    FallbackAbilitiesText->SetAutoWrapText(true);
    FallbackAbilitiesPanel->SetContent(FallbackAbilitiesText);
    BottomRightColumn->AddChildToVerticalBox(FallbackAbilitiesPanel)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    // Customization & Abilities bar (SKINS | TAUNTS | WEAPONS | ABILITIES)
    UHorizontalBox* SubPanelsRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    BottomRightColumn->AddChildToVerticalBox(SubPanelsRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

    auto AddCosmeticTag = [this, SubPanelsRow, &MakeTxt, &DarkPanel](const FText& LabelText) {
      UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
      Box->SetBrushColor(DarkPanel);
      Box->SetPadding(FMargin(16.f, 10.f));
      Box->SetContent(MakeTxt(LabelText, 14, FLinearColor(0.78f, 0.84f, 0.92f, 1.f)));
      SubPanelsRow->AddChildToHorizontalBox(Box)->SetPadding(FMargin(4.f, 0.f));
    };
    AddCosmeticTag(NSLOCTEXT("BwayHeroSelect", "Sub_Skins", "SKINS"));
    AddCosmeticTag(NSLOCTEXT("BwayHeroSelect", "Sub_Taunts", "TAUNTS"));
    AddCosmeticTag(NSLOCTEXT("BwayHeroSelect", "Sub_Weapons", "WEAPONS"));

    UButton* AbilitiesToggleBtn = WidgetTree->ConstructWidget<UButton>();
    AbilitiesToggleBtn->SetBackgroundColor(FLinearColor(0.08f, 0.22f, 0.36f, 0.95f));
    UBorder* AbilPad = WidgetTree->ConstructWidget<UBorder>();
    AbilPad->SetBrushColor(FLinearColor::Transparent);
    AbilPad->SetPadding(FMargin(16.f, 10.f));
    AbilPad->SetContent(MakeTxt(NSLOCTEXT("BwayHeroSelect", "Sub_Abilities", "ABILITIES"), 14, CyanColor));
    AbilitiesToggleBtn->AddChild(AbilPad);
    AbilitiesToggleBtn->OnClicked.AddDynamic(this, &ThisClass::OnFallbackAbilitiesToggleClicked);
    SubPanelsRow->AddChildToHorizontalBox(AbilitiesToggleBtn)->SetPadding(FMargin(4.f, 0.f));

    // Bottom-Right 99-second countdown timer ("ASSEMBLE TEAM 99")
    UBorder* TimerFrame = WidgetTree->ConstructWidget<UBorder>();
    TimerFrame->SetBrushColor(DarkPanel);
    TimerFrame->SetPadding(FMargin(22.f, 10.f));
    FallbackTimerText = MakeTxt(NSLOCTEXT("BwayHeroSelect", "AssembleTeam99", "ASSEMBLE TEAM   99"), 24, GoldColor);
    FallbackTimerText->SetJustification(ETextJustify::Right);
    TimerFrame->SetContent(FallbackTimerText);
    BottomRightColumn->AddChildToVerticalBox(TimerFrame)->SetHorizontalAlignment(HAlign_Right);
  }
  return Super::RebuildWidget();
}

void UBwayHeroSelectWidget::NativeConstruct() {
  Super::NativeConstruct();
  // Preserve the Blueprint's concrete timer type used by its text-update graph.
  SelectionTimerWidget = GetWidgetFromName(TEXT("Widget_Timer"));
  TravelConfirmButton = Cast<ULyraButtonBase>(GetWidgetFromName(TEXT("Button_Lock")));

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
  CleanupPreviewActor();
  UnbindFromPlayerState();
  UnbindFromSelectionManager();
  UnbindFromSelectionPhase();

  // Clear timer
  if (UWorld *World = GetWorld()) {
    World->GetTimerManager().ClearTimer(SelectionTimerHandle);
  }

  Super::NativeDestruct();
}

void UBwayHeroSelectWidget::NativeOnActivated() {
  Super::NativeOnActivated();

  // Bind to systems when widget is activated
  RefreshSelectionManager();
  RefreshLocalPlayerState();
  BindToPlayerState();
  BindToSelectionManager();

  // Poll while active: the owning PlayerState can arrive after construction,
  // including selection screens without a countdown.
  {
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
  RefreshFallbackHeroSelectLayout();

  // If no hero is selected yet, pre-highlight the first available hero so the 3D preview and header populate
  if (!GetSelectedHeroId().IsValid()) {
    const TArray<FHeroDisplayInfo> Available = GetAvailableHeroes();
    for (const FHeroDisplayInfo& Info : Available) {
      if (Info.bIsAvailable && Info.HeroId.IsValid()) {
        SelectHero(Info.HeroId);
        break;
      }
    }
  }

  // Selection can replicate before this screen is activated.
  if (GetSelectedHeroId().IsValid()) {
    OnLocalSelectionChanged(GetSelectedHeroId());
    EnsureAndUpdatePreviewActor(GetSelectedHeroId());
  }
  if (IsSelectionLocked()) {
    OnLocalSelectionLocked(GetSelectedHeroId());
  } else {
    OnLocalSelectionUnlocked();
  }
  RefreshHeroDetails();
  RefreshFallbackHeroSelectLayout();
  RefreshTravelFailurePresentation();

  bIsInitialized = true;
  RequestRefreshFocus();
}

void UBwayHeroSelectWidget::NativeOnDeactivated() {
  CleanupPreviewActor();
  UnbindFromPlayerState();
  UnbindFromSelectionManager();
  UnbindFromSelectionPhase();

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
  RefreshSelectionManager();
  RefreshLocalPlayerState();
  if ((!LocalPlayerState || !SelectionManager) && GetWorld() && GetWorld()->GetNetMode() == NM_Client) {
    return false;
  }
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
    EnsureAndUpdatePreviewActor(HeroId);
    RefreshFallbackHeroSelectLayout();
    UE_LOG(LogTemp, Log,
           TEXT("BwayHeroSelectWidget: Selected hero %s (Server Route)"),
           *HeroId.ToString());
  } else if (FrontendSubsystem) {
    // Offline Front-end route
    FrontendSubsystem->SetSelectedHeroId(HeroId);
    OnLocalSelectionChanged(HeroId);
    EnsureAndUpdatePreviewActor(HeroId);
    RefreshHeroDetails();
    OnHeroListChanged(); // Force redraw the selection highlight locally
    RefreshFallbackHeroSelectLayout();
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
  RefreshSelectionManager();
  RefreshLocalPlayerState();
  if (SelectionPhase && SelectionPhase->HasPostSelectionTravelFailed() && IsSelectionLocked()) {
    if (auto* Controller = Cast<ABwayPlayerController>(GetOwningPlayer())) Controller->RetryHeroSelectionTravel();
    RefreshTravelFailurePresentation();
    return false; // Retry intent is not a second lock/confirmation in an authored graph.
  }
  if ((!LocalPlayerState || !SelectionManager) && GetWorld() && GetWorld()->GetNetMode() == NM_Client) {
    return false;
  }
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

  // Play the hero's lock-in voice line if configured on the HeroDataAsset
  if (UBwayHeroDataAsset* HeroData = GetHeroDataAsset(SelectedHero)) {
    if (HeroData->VoiceBank) {
      UGameplayStatics::PlaySound2D(this, HeroData->VoiceBank);
    }
  }
  RefreshFallbackHeroSelectLayout();

  UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Locked selection"));
  return true;
}

void UBwayHeroSelectWidget::ConfirmSelection() {
  RefreshSelectionManager();
  RefreshLocalPlayerState();
  if (SelectionPhase && SelectionPhase->HasPostSelectionTravelFailed() && IsSelectionLocked()) {
    if (auto* Controller = Cast<ABwayPlayerController>(GetOwningPlayer())) Controller->RetryHeroSelectionTravel();
    RefreshTravelFailurePresentation();
    return;
  }
  FPrimaryAssetId SelectedHero = GetSelectedHeroId();
  if (!SelectedHero.IsValid()) {
    UE_LOG(LogTemp, Warning,
           TEXT("BwayHeroSelectWidget: Cannot confirm - no hero selected"));
    return;
  }

  UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Confirming selection of %s"),
         *SelectedHero.ToString());

  const bool bAwaitPhaseAdvance = SelectionManager && (SelectionManager->IsSelectionActive()
      || (LocalPlayerState && GetOwningPlayer() && !GetOwningPlayer()->GetPawn()));

  // Lock the selection if not already locked
  if (!IsSelectionLocked()) {
    if (!LockSelection()) {
      return;
    }
  }

  // Wait for either the shared phase or the late joiner's server spawn gate.
  // Sending a lock RPC is intent, not confirmation that it was accepted.
  if (bAwaitPhaseAdvance) {
    return;
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
  if (!HeroId.IsValid()) {
    return false;
  }

  // The selection manager reports a selected hero as occupied by its owner.
  // Keep the owner's current choice selectable so refresh/reselection input
  // follows the same availability rule used by GetAvailableHeroes().
  if (HeroId == GetSelectedHeroId()) {
    return true;
  }

  if (!SelectionManager) {
    return true; // If no manager, assume all heroes available
  }

  int32 LocalTeam = GetLocalPlayerTeam();
  return SelectionManager->IsHeroAvailableForTeam(HeroId, LocalTeam);
}

float UBwayHeroSelectWidget::GetRemainingSelectionTime() const {
  if (!SelectionManager || !SelectionManager->IsSelectionActive() || SelectionManager->SelectionTimeLimit <= 0.0f) {
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

void UBwayHeroSelectWidget::RefreshSelectionManager() {
  RefreshSelectionPhase();
  AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
  UBwayHeroSelectionManager* CurrentManager = GS ? GS->FindComponentByClass<UBwayHeroSelectionManager>() : nullptr;
  if (SelectionManager == CurrentManager) {
    return;
  }
  UnbindFromSelectionManager();
  SelectionManager = CurrentManager;
  BindToSelectionManager();
  OnHeroListChanged();
  RequestRefreshFocus();
}

void UBwayHeroSelectWidget::RefreshSelectionPhase() {
  const auto* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
  auto* CurrentPhase = GS ? GS->HeroSelectionPhaseComponent.Get() : nullptr;
  if (SelectionPhase == CurrentPhase) return;
  UnbindFromSelectionPhase();
  SelectionPhase = CurrentPhase;
  if (SelectionPhase) SelectionPhase->OnTravelFailureChanged.AddUniqueDynamic(this, &ThisClass::RefreshTravelFailurePresentation);
  RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::UnbindFromSelectionPhase() {
  if (SelectionPhase) SelectionPhase->OnTravelFailureChanged.RemoveDynamic(this, &ThisClass::RefreshTravelFailurePresentation);
  SelectionPhase = nullptr;
}

void UBwayHeroSelectWidget::RefreshTravelFailurePresentation() {
  const bool bFailed = SelectionPhase && SelectionPhase->HasPostSelectionTravelFailed();
  if (!bFailed) {
    if (bShowingTravelFailure) {
      bShowingTravelFailure = false;
      bTravelRetryAvailable = false;
      if (IsSelectionLocked()) OnLocalSelectionLocked(GetSelectedHeroId());
      else OnLocalSelectionUnlocked();
      RefreshHeroDetails();
    }
    return;
  }
  const auto* Controller = Cast<ABwayPlayerController>(GetOwningPlayer());
  const bool bHost = Controller && Controller->HasAuthority() && Controller->IsLocalController() && Controller->GetLocalPlayer();
  const bool bCanRetry = Controller && Controller->CanRetryHeroSelectionTravel();
  if (TravelConfirmButton && IsSelectionLocked()) {
    TravelConfirmButton->SetButtonText(bHost
        ? NSLOCTEXT("BwayHeroSelect", "RetryTravel", "Retry travel")
        : NSLOCTEXT("BwayHeroSelect", "WaitForHostRetry", "Waiting for host"));
    TravelConfirmButton->SetIsEnabled(bCanRetry);
  }
  if (Text_HeroDetails) {
    Text_HeroDetails->SetText(!IsSelectionLocked()
        ? NSLOCTEXT("BwayHeroSelect", "TravelFailedSelect", "Match travel failed. Choose and lock your hero; the host can retry when everyone is ready.")
        : bCanRetry ? NSLOCTEXT("BwayHeroSelect", "TravelFailedRetry", "Match travel failed. Your selections are saved. Choose Retry travel to try again.")
        : bHost ? NSLOCTEXT("BwayHeroSelect", "TravelFailedPlayers", "Match travel failed. Waiting for all players to lock before you can retry.")
        : NSLOCTEXT("BwayHeroSelect", "TravelFailedWait", "Match travel failed. Your selection is saved. Waiting for the host to retry."));
  }
  const bool bFocusRetry = bCanRetry && !bTravelRetryAvailable;
  bShowingTravelFailure = true;
  bTravelRetryAvailable = bCanRetry;
  if (bFocusRetry) RequestRefreshFocus();
}

UWidget* UBwayHeroSelectWidget::NativeGetDesiredFocusTarget() const {
  if (bTravelRetryAvailable && TravelConfirmButton) return TravelConfirmButton;
  return Super::NativeGetDesiredFocusTarget();
}

void UBwayHeroSelectWidget::RefreshLocalPlayerState() {
  APlayerController* PC = GetOwningPlayer();
  ABwayPlayerState* CurrentState = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
  if (CurrentState == LocalPlayerState) {
    return;
  }
  UnbindFromPlayerState();
  LocalPlayerState = CurrentState;
  BindToPlayerState();
  if (LocalPlayerState) {
    if (GetSelectedHeroId().IsValid()) {
      OnLocalSelectionChanged(GetSelectedHeroId());
    }
    if (IsSelectionLocked()) {
      OnLocalSelectionLocked(GetSelectedHeroId());
    } else {
      OnLocalSelectionUnlocked();
    }
    RefreshHeroDetails();
    OnHeroListChanged();
    RefreshTravelFailurePresentation();
    RequestRefreshFocus();
  }
}

void UBwayHeroSelectWidget::BindToPlayerState() {
  if (!LocalPlayerState) {
    return;
  }

  // Bind to hero selection changed
  LocalPlayerState->OnSelectedHeroChanged.AddUniqueDynamic(
      this, &UBwayHeroSelectWidget::HandlePlayerSelectionChanged);

  // Bind to hero locked (fires when bHeroLocked replicates true)
  LocalPlayerState->OnHeroLocked.AddUniqueDynamic(
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
  SelectionManager->OnPlayerHeroSelectionChanged.AddUniqueDynamic(
      this, &UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged);
  SelectionManager->OnAllPlayersReady.AddUniqueDynamic(
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
  EnsureAndUpdatePreviewActor(NewHeroId);
  RefreshHeroDetails();

  // Refresh hero list (availability may have changed)
  OnHeroListChanged();
  RefreshFallbackHeroSelectLayout();
  RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::HandlePlayerHeroLocked(
    FPrimaryAssetId LockedHeroId) {
  UE_LOG(LogTemp, Log,
         TEXT("BwayHeroSelectWidget: Local player locked hero %s"),
         *LockedHeroId.ToString());

  // Notify blueprint
  OnLocalSelectionLocked(LockedHeroId);
  RefreshFallbackHeroSelectLayout();
  RefreshTravelFailurePresentation();
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
  RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::HandleAllPlayersReady() {
  UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: All players ready!"));

  // Notify blueprint
  OnAllPlayersReady();
  RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::UpdateSelectionTimer() {
  RefreshSelectionManager();
  RefreshLocalPlayerState();
  // A late show RPC or activation can follow the phase's one-shot hide request.
  // Reconcile durable server state once the approved player's pawn has arrived.
  // Keep shared selection and unlocked late arrivals on their selection screen.
  ABwayGameState* State = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
  UBwayRoundManagementComponent* Rounds = State ? State->GetRoundManagement() : nullptr;
  APlayerController* Controller = GetOwningPlayer();
  if (LocalPlayerState && LocalPlayerState->IsHeroLocked() && Controller && Controller->GetPawn() && Rounds
      && (Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Playing
          || Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::PostRound)) {
    UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Dismissing locked selection after gameplay pawn arrived player=%d"),
        LocalPlayerState->GetPlayerId());
    if (ABwayPlayerController* BwayController = Cast<ABwayPlayerController>(Controller)) {
      BwayController->Client_HideHeroSelection();
    } else {
      DeactivateWidget();
    }
    return;
  }
  const float RemainingTime = GetRemainingSelectionTime();
  OnSelectionTimerUpdated(RemainingTime);
  if (SelectionTimerWidget) {
    SelectionTimerWidget->SetVisibility(RemainingTime >= 0.0f
        ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
  }
  if (FallbackTimerText) {
    const int32 DisplaySecs = RemainingTime >= 0.0f ? FMath::CeilToInt(RemainingTime) : 99;
    FallbackTimerText->SetText(FText::Format(
        NSLOCTEXT("BwayHeroSelect", "AssembleTeamFmt", "ASSEMBLE TEAM   {0}"),
        FText::AsNumber(DisplaySecs)));
  }
}

void UBwayHeroSelectWidget::RefreshHeroDetails() {
  FHeroDisplayInfo Hero;
  const bool bHasHero = GetSelectedHeroDisplayInfo(Hero);
  if (bHasHero) {
    FText Details = FText::Format(
        NSLOCTEXT("BwayHeroSelect", "Stats", "Health: {0}   Attack: {1}\nArmor: {2}   Speed: {3}"),
        FText::AsNumber(Hero.Stats.MaxHealth), FText::AsNumber(Hero.Stats.BaseDamage),
        FText::AsNumber(Hero.Stats.Armor), FText::AsNumber(Hero.Stats.MoveSpeed));
    for (const FAbilityDisplayInfo& Ability : Hero.Abilities) {
      if (!Ability.AbilityName.IsEmpty()) {
        Details = FText::Format(NSLOCTEXT("BwayHeroSelect", "AbilityDetails", "{0}\n\n{1}\n{2}"),
            Details, Ability.AbilityName, Ability.Description);
      }
    }
    if (Text_HeroDetails) {
      Text_HeroDetails->SetText(Details);
    }
    if (FallbackAbilitiesText) {
      FallbackAbilitiesText->SetText(Details);
    }
  } else {
    const FText EmptyPrompt = NSLOCTEXT("BwayHeroSelect", "ChooseHero", "Choose a hero to view their stats and abilities.");
    if (Text_HeroDetails) {
      Text_HeroDetails->SetText(EmptyPrompt);
    }
    if (FallbackAbilitiesText) {
      FallbackAbilitiesText->SetText(EmptyPrompt);
    }
  }
}

void UBwayHeroSelectWidget::ToggleAbilitiesPanel() {
  bAbilitiesPanelVisible = !bAbilitiesPanelVisible;
  if (FallbackAbilitiesPanel) {
    FallbackAbilitiesPanel->SetVisibility(
        bAbilitiesPanelVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
  }
}

bool UBwayHeroSelectWidget::SelectHeroByIndex(int32 Index) {
  const TArray<FHeroDisplayInfo> Heroes = GetAvailableHeroes();
  if (!Heroes.IsValidIndex(Index)) {
    return false;
  }
  return SelectHero(Heroes[Index].HeroId);
}

void UBwayHeroSelectWidget::EnsureAndUpdatePreviewActor(FPrimaryAssetId HeroId) {
  UWorld* World = GetWorld();
  if (!World || !World->IsGameWorld()) {
    return;
  }

  UBwayHeroDataAsset* HeroData = GetHeroDataAsset(HeroId);
  if (!PreviewActor || !IsValid(PreviewActor)) {
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.ObjectFlags |= RF_Transient;
    PreviewActor = World->SpawnActor<ABwayHeroPreviewActor>(
        ABwayHeroPreviewActor::StaticClass(),
        FVector(0.0f, 0.0f, 1450.0f),
        FRotator(0.0f, -20.0f, 0.0f),
        SpawnParams);
  }

  if (PreviewActor) {
    PreviewActor->UpdatePreviewHero(HeroData);
    if (APlayerController* PC = GetOwningPlayer()) {
      if (!PC->GetPawn() && PC->GetViewTarget() != PreviewActor) {
        PC->SetViewTargetWithBlend(PreviewActor, 0.25f);
      }
    }
  }
}

void UBwayHeroSelectWidget::CleanupPreviewActor() {
  if (PreviewActor && IsValid(PreviewActor)) {
    if (APlayerController* PC = GetOwningPlayer()) {
      if (PC->GetViewTarget() == PreviewActor && PC->GetPawn()) {
        PC->SetViewTarget(PC->GetPawn());
      }
    }
    PreviewActor->Destroy();
  }
  PreviewActor = nullptr;
}

void UBwayHeroSelectWidget::RefreshFallbackHeroSelectLayout() {
  if (!bUsingFallbackLayout) {
    return;
  }

  FHeroDisplayInfo SelectedInfo;
  if (GetSelectedHeroDisplayInfo(SelectedInfo)) {
    if (FallbackHeroNameText) {
      FallbackHeroNameText->SetText(FText::FromString(SelectedInfo.DisplayName.ToString().ToUpper()));
    }
    if (FallbackHeroClassText) {
      FallbackHeroClassText->SetText(FText::FromString(SelectedInfo.ClassName.ToString().ToUpper()));
    }
  }

  const bool bLocked = IsSelectionLocked();
  if (FallbackReadyLabel) {
    FallbackReadyLabel->SetText(bLocked
        ? NSLOCTEXT("BwayHeroSelect", "LockedInState", "LOCKED IN")
        : NSLOCTEXT("BwayHeroSelect", "ReadyState", "READY!"));
  }
  if (FallbackReadyButton) {
    FallbackReadyButton->SetBackgroundColor(bLocked
        ? FLinearColor(0.18f, 0.56f, 0.28f, 1.0f)
        : FLinearColor(0.95f, 0.78f, 0.32f, 1.0f));
  }

  if (!FallbackPortraitGrid) {
    return;
  }

  const TArray<FHeroDisplayInfo> Heroes = GetAvailableHeroes();
  FallbackGridHeroIds.Reset();
  FallbackPortraitGrid->ClearChildren();

  for (int32 Idx = 0; Idx < Heroes.Num(); ++Idx) {
    const FHeroDisplayInfo& Info = Heroes[Idx];
    FallbackGridHeroIds.Add(Info.HeroId);

    UButton* PortraitBtn = WidgetTree->ConstructWidget<UButton>();
    PortraitBtn->SetBackgroundColor(Info.bIsSelected
        ? FLinearColor(0.98f, 0.82f, 0.30f, 1.0f)
        : (Info.bIsAvailable ? FLinearColor(0.08f, 0.14f, 0.24f, 0.94f) : FLinearColor(0.15f, 0.15f, 0.15f, 0.60f)));

    UBorder* Inner = WidgetTree->ConstructWidget<UBorder>();
    Inner->SetBrushColor(FLinearColor(0.03f, 0.05f, 0.09f, 0.92f));
    Inner->SetPadding(FMargin(8.f, 10.f));

    UVerticalBox* CellVBox = WidgetTree->ConstructWidget<UVerticalBox>();
    if (Info.bIsSelected || Info.SelectedByPlayerIndex >= 0) {
      UTextBlock* Badge = WidgetTree->ConstructWidget<UTextBlock>();
      const int32 PNum = Info.SelectedByPlayerIndex >= 0 ? (Info.SelectedByPlayerIndex + 1) : 1;
      Badge->SetText(FText::FromString(FString::Printf(TEXT("P%d"), PNum)));
      Badge->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.84f, 0.32f, 1.f)));
      FSlateFontInfo BadgeFont = Badge->GetFont();
      BadgeFont.Size = 12;
      Badge->SetFont(BadgeFont);
      CellVBox->AddChildToVerticalBox(Badge);
    }

    if (Info.Portrait) {
      UImage* PortraitImg = WidgetTree->ConstructWidget<UImage>();
      PortraitImg->SetBrushFromTexture(Info.Portrait);
      PortraitImg->SetDesiredSizeOverride(FVector2D(84.f, 84.f));
      CellVBox->AddChildToVerticalBox(PortraitImg)->SetHorizontalAlignment(HAlign_Center);
    }

    UTextBlock* NameLbl = WidgetTree->ConstructWidget<UTextBlock>();
    NameLbl->SetText(Info.DisplayName);
    NameLbl->SetJustification(ETextJustify::Center);
    FSlateFontInfo NameFont = NameLbl->GetFont();
    NameFont.Size = 13;
    NameLbl->SetFont(NameFont);
    CellVBox->AddChildToVerticalBox(NameLbl)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

    Inner->SetContent(CellVBox);
    PortraitBtn->AddChild(Inner);

    switch (Idx) {
      case 0: PortraitBtn->OnClicked.AddDynamic(this, &ThisClass::OnPortrait0Clicked); break;
      case 1: PortraitBtn->OnClicked.AddDynamic(this, &ThisClass::OnPortrait1Clicked); break;
      case 2: PortraitBtn->OnClicked.AddDynamic(this, &ThisClass::OnPortrait2Clicked); break;
      case 3: PortraitBtn->OnClicked.AddDynamic(this, &ThisClass::OnPortrait3Clicked); break;
      case 4: PortraitBtn->OnClicked.AddDynamic(this, &ThisClass::OnPortrait4Clicked); break;
      case 5: PortraitBtn->OnClicked.AddDynamic(this, &ThisClass::OnPortrait5Clicked); break;
      default: break;
    }

    const int32 Row = Idx / 3;
    const int32 Col = Idx % 3;
    FallbackPortraitGrid->AddChildToUniformGrid(PortraitBtn, Row, Col);
  }
}

void UBwayHeroSelectWidget::OnFallbackReadyClicked() { ConfirmSelection(); }
void UBwayHeroSelectWidget::OnFallbackAbilitiesToggleClicked() { ToggleAbilitiesPanel(); }
void UBwayHeroSelectWidget::OnPortrait0Clicked() { if (FallbackGridHeroIds.IsValidIndex(0)) SelectHero(FallbackGridHeroIds[0]); }
void UBwayHeroSelectWidget::OnPortrait1Clicked() { if (FallbackGridHeroIds.IsValidIndex(1)) SelectHero(FallbackGridHeroIds[1]); }
void UBwayHeroSelectWidget::OnPortrait2Clicked() { if (FallbackGridHeroIds.IsValidIndex(2)) SelectHero(FallbackGridHeroIds[2]); }
void UBwayHeroSelectWidget::OnPortrait3Clicked() { if (FallbackGridHeroIds.IsValidIndex(3)) SelectHero(FallbackGridHeroIds[3]); }
void UBwayHeroSelectWidget::OnPortrait4Clicked() { if (FallbackGridHeroIds.IsValidIndex(4)) SelectHero(FallbackGridHeroIds[4]); }
void UBwayHeroSelectWidget::OnPortrait5Clicked() { if (FallbackGridHeroIds.IsValidIndex(5)) SelectHero(FallbackGridHeroIds[5]); }
