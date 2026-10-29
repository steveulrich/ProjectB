// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayFrontEndFlowWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "CommonSessionSubsystem.h"
#include "CommonTextBlock.h"
#include "CommonUIExtensions.h"
#include "CommonUserTypes.h"
#include "Components/Border.h"
#include "Components/CircularThrobber.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Containers/Ticker.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/BwayExperienceLibrary.h"
#include "GameModes/BwayUserFacingExperienceDefinition.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "UI/BwayCommonButton.h"
#include "UObject/UObjectIterator.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AI/BwayRelicBotController.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "GameModes/LyraExperienceDefinition.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroPreviewActor.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectWidget.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "UI/BwayUpgradeShopWidget.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayFrontEndFlowWidgets)

namespace BwayFrontEndUI
{
	const FLinearColor GoldAccent(0.86f, 0.68f, 0.24f, 1.0f);
	const FLinearColor GoldBright(0.98f, 0.84f, 0.42f, 1.0f);
	const FLinearColor CyanAccent(0.18f, 0.72f, 0.92f, 1.0f);
	const FLinearColor BlueTeamHeader(0.10f, 0.36f, 0.68f, 0.92f);
	const FLinearColor RedTeamHeader(0.68f, 0.16f, 0.14f, 0.92f);
	const FLinearColor SpecTeamHeader(0.22f, 0.26f, 0.32f, 0.92f);
	const FLinearColor DarkBackdrop(0.02f, 0.03f, 0.06f, 0.94f);
	const FLinearColor PanelBg(0.05f, 0.08f, 0.13f, 0.96f);
	const FLinearColor CardBg(0.07f, 0.12f, 0.20f, 0.95f);
	const FLinearColor CardFocusedBg(0.14f, 0.25f, 0.42f, 0.99f);
	const FLinearColor PrimaryNormalBg(0.56f, 0.41f, 0.12f, 0.96f);
	const FLinearColor PrimaryFocusedBg(0.76f, 0.56f, 0.16f, 1.00f);
	const FLinearColor SlotBg(0.04f, 0.07f, 0.12f, 0.90f);
	const FLinearColor MutedText(0.64f, 0.70f, 0.78f, 1.0f);
	const FLinearColor ErrorBorderColor(0.84f, 0.22f, 0.18f, 0.96f);
	const FLinearColor ErrorBgColor(0.20f, 0.05f, 0.06f, 0.95f);
	const FLinearColor WarningColor(0.96f, 0.64f, 0.20f, 1.0f);

	UOverlay* EnsureRootOverlay(UWidgetTree* WidgetTree)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		if (!WidgetTree->RootWidget)
		{
			UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
			WidgetTree->RootWidget = Root;
			return Root;
		}
		if (UOverlay* ExistingOverlay = Cast<UOverlay>(WidgetTree->RootWidget))
		{
			if (ExistingOverlay->GetChildrenCount() == 0)
			{
				return ExistingOverlay;
			}
			return nullptr;
		}
		if (UPanelWidget* ExistingPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
		{
			if (ExistingPanel->GetChildrenCount() == 0)
			{
				UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
				ExistingPanel->AddChild(Root);
				return Root;
			}
		}
		return nullptr;
	}

	UTextBlock* MakeText(
		UWidgetTree* Tree,
		const FText& Text,
		int32 FontSize = 18,
		const FLinearColor& Color = FLinearColor::White,
		FName WidgetName = NAME_None)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
		Block->SetText(Text);
		Block->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = FontSize;
		Block->SetFont(Font);
		return Block;
	}

	UBwayCommonButton* MakeCommonButton(
		UWidgetTree* Tree,
		FName WidgetName,
		const FText& LabelText,
		int32 FontSize,
		bool bPrimaryAccent,
		const FLinearColor& TextColor = FLinearColor::White,
		FMargin Padding = FMargin(18.f, 11.f))
	{
		UBwayCommonButton* Btn = Tree->ConstructWidget<UBwayCommonButton>(UBwayCommonButton::StaticClass(), WidgetName);
		Btn->SetLabelText(LabelText);
		Btn->SetFontSize(FontSize);
		Btn->SetContentPadding(Padding);
		if (bPrimaryAccent)
		{
			Btn->SetButtonColors(PrimaryNormalBg, PrimaryFocusedBg, GoldBright, FLinearColor::White);
		}
		else
		{
			Btn->SetButtonColors(CardBg, CardFocusedBg, GoldBright, TextColor);
		}
		return Btn;
	}

	TSubclassOf<UCommonActivatableWidget> ResolveScreenClass(
		const TSoftClassPtr<UCommonActivatableWidget>& ConfiguredClass,
		const TCHAR* DefaultBpPath,
		TSubclassOf<UCommonActivatableWidget> NativeFallback)
	{
		if (!ConfiguredClass.IsNull())
		{
			if (UClass* Loaded = ConfiguredClass.LoadSynchronous())
			{
				return Loaded;
			}
		}
		if (DefaultBpPath && *DefaultBpPath)
		{
			const FSoftClassPath SoftPath(DefaultBpPath);
			if (UClass* BpClass = SoftPath.TryLoadClass<UCommonActivatableWidget>())
			{
				return BpClass;
			}
		}
		return NativeFallback;
	}

	UHorizontalBox* BuildPersistentPartyHeaderBar(UWidgetTree* Tree, const FString& PlayerName, TObjectPtr<UTextBlock>* OutPlayerNameBlock = nullptr)
	{
		UHorizontalBox* HeaderRow = Tree->ConstructWidget<UHorizontalBox>();

		{
			UBorder* BuildTagBox = Tree->ConstructWidget<UBorder>();
			BuildTagBox->SetBrushColor(FLinearColor(0.04f, 0.06f, 0.10f, 0.90f));
			BuildTagBox->SetPadding(FMargin(14.f, 6.f));
			UTextBlock* BuildTagText = MakeText(Tree, NSLOCTEXT("BwayFE", "HeaderModeTag", "4v4 RELIC ARENA"), 13, GoldBright);
			BuildTagBox->SetContent(BuildTagText);
			HeaderRow->AddChildToHorizontalBox(BuildTagBox)->SetPadding(FMargin(4.f, 0.f));
		}

		{
			UBorder* ProfileBox = Tree->ConstructWidget<UBorder>();
			ProfileBox->SetBrushColor(FLinearColor(0.08f, 0.14f, 0.24f, 0.95f));
			ProfileBox->SetPadding(FMargin(16.f, 6.f));
			UTextBlock* ProfileText = MakeText(
				Tree,
				FText::FromString(FString::Printf(TEXT("WARRIOR: %s"), *PlayerName)),
				13,
				FLinearColor::White);
			ProfileBox->SetContent(ProfileText);
			if (OutPlayerNameBlock)
			{
				*OutPlayerNameBlock = ProfileText;
			}
			HeaderRow->AddChildToHorizontalBox(ProfileBox)->SetPadding(FMargin(4.f, 0.f));
		}

		return HeaderRow;
	}

	UWorld* FindActiveRuntimeWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
				&& World->GetGameInstance() && !World->bIsTearingDown)
			{
				return World;
			}
		}
		return nullptr;
	}

	template <typename TWidget>
	TWidget* FindActiveWidgetForPlayer(APlayerController* PC)
	{
		for (TObjectIterator<TWidget> It; It; ++It)
		{
			if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated())
			{
				return *It;
			}
		}
		return nullptr;
	}

	UCommonActivatableWidget* PushBreakawayMainMenu(APlayerController* PC)
	{
		if (!PC || !PC->GetLocalPlayer())
		{
			return nullptr;
		}
		if (UBwayMainMenuWidget* Existing = FindActiveWidgetForPlayer<UBwayMainMenuWidget>(PC))
		{
			return Existing;
		}
		const TSubclassOf<UCommonActivatableWidget> MainMenuClass = ResolveScreenClass(
			TSoftClassPtr<UCommonActivatableWidget>(),
			TEXT("/BreakawayCore/UI/FrontEnd/W_MainMenu.W_MainMenu_C"),
			UBwayMainMenuWidget::StaticClass());
		const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
		return UCommonUIExtensions::PushContentToLayer_ForPlayer(PC->GetLocalPlayer(), MenuLayer, MainMenuClass);
	}

	/**
	 * Ensures L_LyraFrontEnd never leaves stock Lyra/Shooter W_LyraFrontEnd_C tiles active.
	 * If B_LyraFrontendStateComponent already points directly to W_MainMenu, this is a zero-cost
	 * guard; if W_LyraFrontEnd_C is ever pushed, it immediately deactivates & collapses it and
	 * activates W_MainMenu so only Breakaway flows exist on UI.Layer.Menu.
	 */
	struct FCleanFrontEndMenuGuard
	{
		FCleanFrontEndMenuGuard()
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FCleanFrontEndMenuGuard::Tick), 0.1f);
		}

		bool Tick(float)
		{
			if (!GEngine)
			{
				return true;
			}

			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game)
					|| !World->GetGameInstance() || World->bIsTearingDown
					|| !World->GetMapName().Contains(TEXT("L_LyraFrontEnd")))
				{
					continue;
				}

				for (FConstPlayerControllerIterator PCIt = World->GetPlayerControllerIterator(); PCIt; ++PCIt)
				{
					APlayerController* PC = PCIt->Get();
					if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer())
					{
						continue;
					}

					bool bRemovedStockFrontEnd = false;
					for (TObjectIterator<UCommonActivatableWidget> It; It; ++It)
					{
						if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated()
							&& It->GetClass()->GetName() == TEXT("W_LyraFrontEnd_C"))
						{
							It->DeactivateWidget();
							It->SetVisibility(ESlateVisibility::Collapsed);
							bRemovedStockFrontEnd = true;
						}
					}

					if (bRemovedStockFrontEnd)
					{
						UCommonActivatableWidget* Pushed = PushBreakawayMainMenu(PC);
						UE_LOG(LogTemp, Display, TEXT("[BWAY_FE_FLOW] ReplacedStockFrontEnd map=%s widget=%s"),
							*World->GetMapName(), *GetNameSafe(Pushed));
					}
				}
			}
			return true;
		}
	};

	static FCleanFrontEndMenuGuard GCleanFrontEndMenuGuard;
} // namespace BwayFrontEndUI

// ============================================================================
// UBwayMainMenuWidget (W_MainMenu)
// ============================================================================

UBwayMainMenuWidget::UBwayMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = false;
	SettingsScreenClass = TSoftClassPtr<UCommonActivatableWidget>(
		FSoftClassPath(TEXT("/Game/UI/Settings/W_LyraSettingScreen.W_LyraSettingScreen_C")));
}

TOptional<FUIInputConfig> UBwayMainMenuWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

TSharedRef<SWidget> UBwayMainMenuWidget::RebuildWidget()
{
	if (UOverlay* Root = BwayFrontEndUI::EnsureRootOverlay(WidgetTree))
	{
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MainMenuBackdrop"));
		Backdrop->SetBrushColor(FLinearColor(0.02f, 0.04f, 0.08f, 0.84f));
		UOverlaySlot* BgSlot = Root->AddChildToOverlay(Backdrop);
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);

		UHorizontalBox* PartyBar = BwayFrontEndUI::BuildPersistentPartyHeaderBar(WidgetTree, TEXT("Player1"), &ProfileNameText);
		UOverlaySlot* TopRightSlot = Root->AddChildToOverlay(PartyBar);
		TopRightSlot->SetHorizontalAlignment(HAlign_Right);
		TopRightSlot->SetVerticalAlignment(VAlign_Top);
		TopRightSlot->SetPadding(FMargin(0.f, 24.f, 36.f, 0.f));

		UVerticalBox* LeftNavColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftNavColumn"));
		UOverlaySlot* LeftSlot = Root->AddChildToOverlay(LeftNavColumn);
		LeftSlot->SetHorizontalAlignment(HAlign_Left);
		LeftSlot->SetVerticalAlignment(VAlign_Center);
		LeftSlot->SetPadding(FMargin(72.f, 0.f, 0.f, 0.f));

		UTextBlock* LogoTitle = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "BreakawayLogo", "BREAKAWAY"),
			46,
			BwayFrontEndUI::GoldBright,
			TEXT("Text_LogoTitle"));
		LeftNavColumn->AddChildToVerticalBox(LogoTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

		UTextBlock* Subtitle = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "BreakawaySubtitle", "MYTHIC 4v4 RELIC COMBAT  •  DORADO"),
			14,
			BwayFrontEndUI::CyanAccent,
			TEXT("Text_LogoSubtitle"));
		LeftNavColumn->AddChildToVerticalBox(Subtitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));

		auto AddNavButton = [this, LeftNavColumn](
			FName Name,
			const FText& Label,
			int32 FontSize,
			bool bPrimary,
			const FLinearColor& TextColor) -> UBwayCommonButton*
		{
			USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
			SizeBox->SetWidthOverride(340.f);
			UBwayCommonButton* Btn = BwayFrontEndUI::MakeCommonButton(
				WidgetTree,
				Name,
				Label,
				FontSize,
				bPrimary,
				TextColor,
				FMargin(22.f, 13.f));
			Btn->SetJustification(ETextJustify::Left);
			SizeBox->AddChild(Btn);
			LeftNavColumn->AddChildToVerticalBox(SizeBox)->SetPadding(FMargin(0.f, 5.f));
			return Btn;
		};

		HostLanMatchButton = AddNavButton(
			TEXT("Button_HostLanMatch"),
			NSLOCTEXT("BwayFE", "Nav_HostLanMatch", "Host LAN Match"),
			20,
			true,
			FLinearColor::White);
		FindLanMatchButton = AddNavButton(
			TEXT("Button_FindLanMatch"),
			NSLOCTEXT("BwayFE", "Nav_FindLanMatch", "Find LAN Match"),
			19,
			false,
			BwayFrontEndUI::GoldBright);
		DevPracticeButton = AddNavButton(
			TEXT("Button_DevPractice"),
			NSLOCTEXT("BwayFE", "Nav_DevPractice", "Dev Practice / Bot Match"),
			18,
			false,
			BwayFrontEndUI::GoldBright);
		SettingsButton = AddNavButton(
			TEXT("Button_Settings"),
			NSLOCTEXT("BwayFE", "Nav_Settings", "Settings"),
			18,
			false,
			FLinearColor::White);
		QuitButton = AddNavButton(
			TEXT("Button_Quit"),
			NSLOCTEXT("BwayFE", "Nav_Quit", "Quit"),
			18,
			false,
			BwayFrontEndUI::MutedText);

		StatusBannerText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "MainMenuHint", "Host or discover a 4v4 LAN session, or practice offline with bots."),
			13,
			BwayFrontEndUI::MutedText,
			TEXT("Text_StatusBanner"));
		LeftNavColumn->AddChildToVerticalBox(StatusBannerText)->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
	}
	return Super::RebuildWidget();
}

void UBwayMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HostLanMatchButton)
	{
		HostLanMatchButton->OnClicked().RemoveAll(this);
		HostLanMatchButton->OnClicked().AddWeakLambda(this, [this]() { ClickHostLanMatch(); });
	}
	if (FindLanMatchButton)
	{
		FindLanMatchButton->OnClicked().RemoveAll(this);
		FindLanMatchButton->OnClicked().AddWeakLambda(this, [this]() { ClickFindLanMatch(); });
	}
	if (DevPracticeButton)
	{
		DevPracticeButton->OnClicked().RemoveAll(this);
		DevPracticeButton->OnClicked().AddWeakLambda(this, [this]() { ClickDevPractice(); });
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked().RemoveAll(this);
		SettingsButton->OnClicked().AddUObject(this, &ThisClass::ClickSettings);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked().RemoveAll(this);
		QuitButton->OnClicked().AddUObject(this, &ThisClass::ClickQuit);
	}
}

void UBwayMainMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UBwayFrontendStateSubsystem* FrontendState = LP->GetSubsystem<UBwayFrontendStateSubsystem>())
		{
			if (ProfileNameText)
			{
				ProfileNameText->SetText(FText::FromString(FString::Printf(TEXT("WARRIOR: %s"), *FrontendState->GetResolvedLocalPlayerName())));
			}
		}
	}
	RequestRefreshFocus();
}

UWidget* UBwayMainMenuWidget::NativeGetDesiredFocusTarget() const
{
	return HostLanMatchButton ? static_cast<UWidget*>(HostLanMatchButton) : Super::NativeGetDesiredFocusTarget();
}

TArray<FString> UBwayMainMenuWidget::GetVisibleMenuButtonLabels() const
{
	TArray<FString> Labels;
	const UBwayCommonButton* Buttons[] = {
		HostLanMatchButton,
		FindLanMatchButton,
		DevPracticeButton,
		SettingsButton,
		QuitButton
	};
	for (const UBwayCommonButton* Btn : Buttons)
	{
		if (Btn && Btn->GetVisibility() != ESlateVisibility::Collapsed && Btn->GetVisibility() != ESlateVisibility::Hidden)
		{
			Labels.Add(Btn->GetLabelText().ToString());
		}
	}
	return Labels;
}

UCommonActivatableWidget* UBwayMainMenuWidget::PushLobbyWithOnlineMode(bool bIsLanMatch, bool bForceBotsOn)
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}

	if (UBwayFrontendStateSubsystem* FrontendState = LP->GetSubsystem<UBwayFrontendStateSubsystem>())
	{
		FrontendState->SetLobbyOnlineMode(bIsLanMatch);
		if (bForceBotsOn)
		{
			const FBwayMatchLobbySettings& Cur = FrontendState->GetLobbySettings();
			FrontendState->ApplyMatchSettingsModal(Cur.SelectedMapIndex, true, Cur.BotDifficulty);
		}
		FrontendState->InitializeDefaultLobbyRoster();
	}

	const TSubclassOf<UCommonActivatableWidget> LobbyClass = BwayFrontEndUI::ResolveScreenClass(
		MatchLobbyWidgetClass,
		TEXT("/BreakawayCore/UI/FrontEnd/W_MatchLobby.W_MatchLobby_C"),
		UBwayMatchLobbyWidget::StaticClass());

	const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
	return UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, MenuLayer, LobbyClass);
}

UCommonActivatableWidget* UBwayMainMenuWidget::ClickHostLanMatch()
{
	return PushLobbyWithOnlineMode(/*bIsLanMatch=*/true, /*bForceBotsOn=*/false);
}

UCommonActivatableWidget* UBwayMainMenuWidget::ClickFindLanMatch()
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}

	const TSubclassOf<UCommonActivatableWidget> BrowserClass = BwayFrontEndUI::ResolveScreenClass(
		LocalMatchBrowserWidgetClass,
		TEXT("/BreakawayCore/UI/FrontEnd/W_LocalMatchBrowser.W_LocalMatchBrowser_C"),
		UBwayLocalMatchBrowserWidget::StaticClass());

	const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
	return UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, MenuLayer, BrowserClass);
}

UCommonActivatableWidget* UBwayMainMenuWidget::ClickDevPractice()
{
	return PushLobbyWithOnlineMode(/*bIsLanMatch=*/false, /*bForceBotsOn=*/true);
}

UCommonActivatableWidget* UBwayMainMenuWidget::OpenSettingsScreen()
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}

	const TSubclassOf<UCommonActivatableWidget> TargetSettingsClass = BwayFrontEndUI::ResolveScreenClass(
		SettingsScreenClass,
		TEXT("/Game/UI/Settings/W_LyraSettingScreen.W_LyraSettingScreen_C"),
		nullptr);
	if (!TargetSettingsClass)
	{
		if (StatusBannerText)
		{
			StatusBannerText->SetText(NSLOCTEXT("BwayFE", "SettingsUnavailableHint", "Settings screen asset could not be loaded."));
		}
		return nullptr;
	}

	const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
	return UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, MenuLayer, TargetSettingsClass);
}

void UBwayMainMenuWidget::ClickSettings()
{
	OpenSettingsScreen();
}

void UBwayMainMenuWidget::ClickQuit()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}

// ============================================================================
// UBwayModeSelectionWidget (Legacy Blueprint Parent Compatibility)
// ============================================================================

UBwayModeSelectionWidget::UBwayModeSelectionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> UBwayModeSelectionWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

TSharedRef<SWidget> UBwayModeSelectionWidget::RebuildWidget()
{
	if (UOverlay* Root = BwayFrontEndUI::EnsureRootOverlay(WidgetTree))
	{
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(BwayFrontEndUI::DarkBackdrop);
		UOverlaySlot* BgSlot = Root->AddChildToOverlay(Backdrop);
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);

		UVerticalBox* CenterBox = WidgetTree->ConstructWidget<UVerticalBox>();
		UOverlaySlot* CenterSlot = Root->AddChildToOverlay(CenterBox);
		CenterSlot->SetHorizontalAlignment(HAlign_Center);
		CenterSlot->SetVerticalAlignment(VAlign_Center);

		LocalCardButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree,
			TEXT("Button_LocalMode"),
			NSLOCTEXT("BwayFE", "Mode_Local", "LOCAL LAN"),
			20,
			true);
		CenterBox->AddChildToVerticalBox(LocalCardButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

		ModeDescriptionText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "LocalHoverDesc", "Host a local server. For dev testing only."),
			15,
			FLinearColor::White);
		ModeDescriptionText->SetJustification(ETextJustify::Center);
		CenterBox->AddChildToVerticalBox(ModeDescriptionText)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

		CancelButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree,
			TEXT("Button_CancelMode"),
			NSLOCTEXT("BwayFE", "Btn_Cancel", "CANCEL"),
			16,
			false,
			BwayFrontEndUI::GoldBright);
		CenterBox->AddChildToVerticalBox(CancelButton);
	}
	return Super::RebuildWidget();
}

void UBwayModeSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (LocalCardButton)
	{
		LocalCardButton->OnClicked().RemoveAll(this);
		LocalCardButton->OnClicked().AddWeakLambda(this, [this]() { ClickLocalMode(); });
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &ThisClass::ClickCancel);
	}
}

void UBwayModeSelectionWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	SetHoveredMode(TEXT("LOCAL"));
	RequestRefreshFocus();
}

UWidget* UBwayModeSelectionWidget::NativeGetDesiredFocusTarget() const
{
	return LocalCardButton ? static_cast<UWidget*>(LocalCardButton) : Super::NativeGetDesiredFocusTarget();
}

void UBwayModeSelectionWidget::SetHoveredMode(FName ModeId)
{
	ActiveHoveredModeId = ModeId;
	if (ModeDescriptionText)
	{
		ModeDescriptionText->SetText(GetCurrentModeDescription());
	}
}

FText UBwayModeSelectionWidget::GetCurrentModeDescription() const
{
	return NSLOCTEXT("BwayFE", "Desc_Local", "Host a local server. For dev testing only.");
}

UCommonActivatableWidget* UBwayModeSelectionWidget::SelectMode(FName ModeId)
{
	SetHoveredMode(ModeId);
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}
	const TSubclassOf<UCommonActivatableWidget> BrowserClass = BwayFrontEndUI::ResolveScreenClass(
		LocalMatchBrowserWidgetClass,
		TEXT("/BreakawayCore/UI/FrontEnd/W_LocalMatchBrowser.W_LocalMatchBrowser_C"),
		UBwayLocalMatchBrowserWidget::StaticClass());
	const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
	return UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, MenuLayer, BrowserClass);
}

void UBwayModeSelectionWidget::ClickCancel()
{
	DeactivateWidget();
}

// ============================================================================
// UBwayLocalMatchBrowserWidget (W_LocalMatchBrowser)
// ============================================================================

UBwayLocalMatchBrowserWidget::UBwayLocalMatchBrowserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> UBwayLocalMatchBrowserWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

TSharedRef<SWidget> UBwayLocalMatchBrowserWidget::RebuildWidget()
{
	if (UOverlay* Root = BwayFrontEndUI::EnsureRootOverlay(WidgetTree))
	{
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BrowserBackdrop"));
		Backdrop->SetBrushColor(BwayFrontEndUI::DarkBackdrop);
		UOverlaySlot* BgSlot = Root->AddChildToOverlay(Backdrop);
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);

		UHorizontalBox* PartyBar = BwayFrontEndUI::BuildPersistentPartyHeaderBar(WidgetTree, TEXT("Player1"));
		UOverlaySlot* TopRightSlot = Root->AddChildToOverlay(PartyBar);
		TopRightSlot->SetHorizontalAlignment(HAlign_Right);
		TopRightSlot->SetVerticalAlignment(VAlign_Top);
		TopRightSlot->SetPadding(FMargin(0.f, 24.f, 36.f, 0.f));

		USizeBox* BrowserBoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BrowserPanelSize"));
		BrowserBoxSize->SetWidthOverride(1020.f);
		BrowserBoxSize->SetHeightOverride(600.f);
		UOverlaySlot* CenterSlot = Root->AddChildToOverlay(BrowserBoxSize);
		CenterSlot->SetHorizontalAlignment(HAlign_Center);
		CenterSlot->SetVerticalAlignment(VAlign_Center);

		UBorder* BrowserPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BrowserPanelFrame"));
		BrowserPanel->SetBrushColor(BwayFrontEndUI::PanelBg);
		BrowserPanel->SetPadding(FMargin(28.f, 24.f));
		BrowserBoxSize->AddChild(BrowserPanel);

		UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BrowserMainVBox"));
		BrowserPanel->SetContent(MainVBox);

		// Header Row: Title + REFRESH button
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		MainVBox->AddChildToVerticalBox(HeaderRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

		UTextBlock* TitleText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "LocalBrowserTitle", "LAN SERVER BROWSER"),
			24,
			BwayFrontEndUI::GoldBright,
			TEXT("Text_BrowserTitle"));
		UHorizontalBoxSlot* TitleSlot = HeaderRow->AddChildToHorizontalBox(TitleText);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);

		RefreshButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree,
			TEXT("Button_Refresh"),
			NSLOCTEXT("BwayFE", "Btn_Refresh", "REFRESH"),
			15,
			false,
			BwayFrontEndUI::CyanAccent,
			FMargin(18.f, 9.f));
		HeaderRow->AddChildToHorizontalBox(RefreshButton);

		// Error / Retry Banner (Collapsed unless a session is full, unreachable, or search fails)
		ErrorBannerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ErrorBannerBorder"));
		ErrorBannerBorder->SetBrushColor(BwayFrontEndUI::ErrorBorderColor);
		ErrorBannerBorder->SetPadding(FMargin(2.f));
		ErrorBannerBorder->SetVisibility(ESlateVisibility::Collapsed);
		MainVBox->AddChildToVerticalBox(ErrorBannerBorder)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

		UBorder* ErrorInner = WidgetTree->ConstructWidget<UBorder>();
		ErrorInner->SetBrushColor(BwayFrontEndUI::ErrorBgColor);
		ErrorInner->SetPadding(FMargin(16.f, 10.f));
		ErrorBannerBorder->SetContent(ErrorInner);

		UHorizontalBox* ErrorRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		ErrorInner->SetContent(ErrorRow);

		ErrorBannerText = BwayFrontEndUI::MakeText(
			WidgetTree,
			FText::GetEmpty(),
			14,
			FLinearColor(1.0f, 0.86f, 0.84f, 1.0f),
			TEXT("Text_ErrorBanner"));
		ErrorBannerText->SetAutoWrapText(true);
		UHorizontalBoxSlot* ErrorTextSlot = ErrorRow->AddChildToHorizontalBox(ErrorBannerText);
		ErrorTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ErrorTextSlot->SetVerticalAlignment(VAlign_Center);
		ErrorTextSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

		RetryButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree,
			TEXT("Button_Retry"),
			NSLOCTEXT("BwayFE", "Btn_Retry", "RETRY"),
			14,
			true,
			FLinearColor::White,
			FMargin(18.f, 8.f));
		ErrorRow->AddChildToHorizontalBox(RetryButton)->SetVerticalAlignment(VAlign_Center);

		// Column Headers Bar
		UBorder* ColumnsHeaderBorder = WidgetTree->ConstructWidget<UBorder>();
		ColumnsHeaderBorder->SetBrushColor(FLinearColor(0.08f, 0.13f, 0.21f, 0.94f));
		ColumnsHeaderBorder->SetPadding(FMargin(18.f, 9.f));
		MainVBox->AddChildToVerticalBox(ColumnsHeaderBorder)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

		UHorizontalBox* ColumnsHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		ColumnsHeaderBorder->SetContent(ColumnsHeaderRow);

		auto AddColumnHeader = [&](const FText& Label, float FillWeight, ETextJustify::Type Justify)
		{
			UTextBlock* ColText = BwayFrontEndUI::MakeText(WidgetTree, Label, 13, BwayFrontEndUI::MutedText);
			ColText->SetJustification(Justify);
			UHorizontalBoxSlot* ColSlot = ColumnsHeaderRow->AddChildToHorizontalBox(ColText);
			if (FillWeight > 0.f)
			{
				FSlateChildSize FillSize(ESlateSizeRule::Fill);
				FillSize.Value = FillWeight;
				ColSlot->SetSize(FillSize);
			}
			ColSlot->SetVerticalAlignment(VAlign_Center);
		};

		AddColumnHeader(NSLOCTEXT("BwayFE", "Col_SessionName", "SESSION NAME"), 0.38f, ETextJustify::Left);
		AddColumnHeader(NSLOCTEXT("BwayFE", "Col_Map", "MAP"), 0.16f, ETextJustify::Center);
		AddColumnHeader(NSLOCTEXT("BwayFE", "Col_Players", "PLAYERS"), 0.14f, ETextJustify::Center);
		AddColumnHeader(NSLOCTEXT("BwayFE", "Col_Ping", "PING"), 0.20f, ETextJustify::Center);
		{
			USizeBox* ActionHeaderSize = WidgetTree->ConstructWidget<USizeBox>();
			ActionHeaderSize->SetWidthOverride(130.f);
			UTextBlock* ActionCol = BwayFrontEndUI::MakeText(
				WidgetTree, NSLOCTEXT("BwayFE", "Col_Action", "ACTION"), 13, BwayFrontEndUI::MutedText);
			ActionCol->SetJustification(ETextJustify::Center);
			ActionHeaderSize->AddChild(ActionCol);
			ColumnsHeaderRow->AddChildToHorizontalBox(ActionHeaderSize)->SetVerticalAlignment(VAlign_Center);
		}

		// Central Server List Area
		UBorder* ServerListArea = WidgetTree->ConstructWidget<UBorder>();
		ServerListArea->SetBrushColor(FLinearColor(0.02f, 0.04f, 0.07f, 0.92f));
		ServerListArea->SetPadding(FMargin(16.f, 16.f));
		UVerticalBoxSlot* ListSlot = MainVBox->AddChildToVerticalBox(ServerListArea);
		ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ListSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));

		UVerticalBox* ListContentVBox = WidgetTree->ConstructWidget<UVerticalBox>();
		ServerListArea->SetContent(ListContentVBox);

		// Searching spinner + primary status readout
		SearchingStateBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SearchingStateBox"));
		UVerticalBoxSlot* SearchingSlot = ListContentVBox->AddChildToVerticalBox(SearchingStateBox);
		SearchingSlot->SetHorizontalAlignment(HAlign_Center);
		SearchingSlot->SetPadding(FMargin(0.f, 70.f, 0.f, 10.f));

		SearchingSpinner = WidgetTree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass(), TEXT("SearchingSpinner"));
		SearchingSpinner->SetNumberOfPieces(8);
		SearchingSpinner->SetPeriod(0.75f);
		SearchingSpinner->SetRadius(16.f);
		SearchingStateBox->AddChildToHorizontalBox(SearchingSpinner)->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));

		ServerListStatusText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "SearchingLanSessions", "SEARCHING FOR LAN SESSIONS..."),
			19,
			BwayFrontEndUI::GoldBright,
			TEXT("Text_ServerListStatus"));
		ServerListStatusText->SetJustification(ETextJustify::Center);
		SearchingStateBox->AddChildToHorizontalBox(ServerListStatusText)->SetVerticalAlignment(VAlign_Center);

		ServerListSubStatusText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "SearchingLanSub", "Scanning local subnet for Breakaway Dorado hosts..."),
			13,
			BwayFrontEndUI::MutedText,
			TEXT("Text_ServerListSubStatus"));
		ServerListSubStatusText->SetJustification(ETextJustify::Center);
		ListContentVBox->AddChildToVerticalBox(ServerListSubStatusText)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

		ServerRowsScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ServerRowsScrollBox"));
		UVerticalBoxSlot* ScrollSlot = ListContentVBox->AddChildToVerticalBox(ServerRowsScrollBox);
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		ServerRowsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ServerRowsBox"));
		ServerRowsScrollBox->AddChild(ServerRowsBox);

		// Bottom Action Bar: BACK and HOST LAN MATCH
		UHorizontalBox* BottomRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		MainVBox->AddChildToVerticalBox(BottomRow);

		BackButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree,
			TEXT("Button_Back"),
			NSLOCTEXT("BwayFE", "Btn_Back", "BACK"),
			16,
			false,
			BwayFrontEndUI::MutedText);
		BottomRow->AddChildToHorizontalBox(BackButton);

		USpacer* BottomFlex = WidgetTree->ConstructWidget<USpacer>();
		BottomRow->AddChildToHorizontalBox(BottomFlex)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		USizeBox* HostBtnSize = WidgetTree->ConstructWidget<USizeBox>();
		HostBtnSize->SetWidthOverride(260.f);
		HostMatchButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree,
			TEXT("Button_HostMatch"),
			NSLOCTEXT("BwayFE", "Btn_HostLanMatch", "HOST LAN MATCH"),
			18,
			true,
			FLinearColor::White);
		HostBtnSize->AddChild(HostMatchButton);
		BottomRow->AddChildToHorizontalBox(HostBtnSize);

		// Fullscreen CREATING SERVER... Overlay
		CreatingServerOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CreatingServerOverlay"));
		CreatingServerOverlay->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.05f, 0.96f));
		CreatingServerOverlay->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* CreatingSlot = Root->AddChildToOverlay(CreatingServerOverlay);
		CreatingSlot->SetHorizontalAlignment(HAlign_Fill);
		CreatingSlot->SetVerticalAlignment(VAlign_Fill);

		UVerticalBox* CreatingBox = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* CreatingText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "CreatingServer", "PREPARING LAN LOBBY..."),
			26,
			BwayFrontEndUI::GoldBright);
		CreatingText->SetJustification(ETextJustify::Center);
		CreatingBox->AddChildToVerticalBox(CreatingText);
		CreatingServerOverlay->SetContent(CreatingBox);
		CreatingServerOverlay->SetHorizontalAlignment(HAlign_Center);
		CreatingServerOverlay->SetVerticalAlignment(VAlign_Center);
	}
	return Super::RebuildWidget();
}

void UBwayLocalMatchBrowserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (RefreshButton)
	{
		RefreshButton->OnClicked().RemoveAll(this);
		RefreshButton->OnClicked().AddUObject(this, &ThisClass::RefreshServerList);
	}
	if (RetryButton)
	{
		RetryButton->OnClicked().RemoveAll(this);
		RetryButton->OnClicked().AddUObject(this, &ThisClass::ClickRetrySearch);
	}
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &ThisClass::ClickBack);
	}
	if (HostMatchButton)
	{
		HostMatchButton->OnClicked().RemoveAll(this);
		HostMatchButton->OnClicked().AddUObject(this, &ThisClass::ClickHostMatch);
	}
}

void UBwayLocalMatchBrowserWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	bIsCreatingServer = false;
	bPendingJoinAttempt = false;
	if (CreatingServerOverlay)
	{
		CreatingServerOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UCommonSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UCommonSessionSubsystem>())
			{
				if (JoinSessionCompleteHandle.IsValid())
				{
					SessionSubsystem->OnJoinSessionCompleteEvent.Remove(JoinSessionCompleteHandle);
				}
				JoinSessionCompleteHandle = SessionSubsystem->OnJoinSessionCompleteEvent.AddUObject(
					this, &ThisClass::HandleJoinSessionComplete);
			}
		}
	}

	RefreshServerList();
}

void UBwayLocalMatchBrowserWidget::NativeOnDeactivated()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CreatingServerTimerHandle);
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UCommonSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UCommonSessionSubsystem>())
			{
				if (JoinSessionCompleteHandle.IsValid())
				{
					SessionSubsystem->OnJoinSessionCompleteEvent.Remove(JoinSessionCompleteHandle);
					JoinSessionCompleteHandle.Reset();
				}
			}
		}
	}
	bPendingJoinAttempt = false;
	Super::NativeOnDeactivated();
}

UWidget* UBwayLocalMatchBrowserWidget::NativeGetDesiredFocusTarget() const
{
	if (IsErrorBannerVisible() && RetryButton && RetryButton->GetIsEnabled())
	{
		return RetryButton;
	}
	for (UBwayCommonButton* JoinBtn : RowJoinButtons)
	{
		if (JoinBtn && JoinBtn->GetIsEnabled())
		{
			return JoinBtn;
		}
	}
	if (RefreshButton && RefreshButton->GetIsEnabled())
	{
		return RefreshButton;
	}
	if (BackButton && BackButton->GetIsEnabled())
	{
		return BackButton;
	}
	return HostMatchButton ? static_cast<UWidget*>(HostMatchButton) : Super::NativeGetDesiredFocusTarget();
}

bool UBwayLocalMatchBrowserWidget::IsSpinnerVisible() const
{
	return SearchingSpinner
		&& SearchingStateBox
		&& SearchingStateBox->GetVisibility() != ESlateVisibility::Collapsed
		&& SearchingSpinner->GetVisibility() != ESlateVisibility::Collapsed;
}

bool UBwayLocalMatchBrowserWidget::IsErrorBannerVisible() const
{
	return ErrorBannerBorder && ErrorBannerBorder->GetVisibility() != ESlateVisibility::Collapsed;
}

FBwayServerBrowserRowViewModel UBwayLocalMatchBrowserWidget::BuildRowViewModelFromSearchResult(
	const UCommonSession_SearchResult* SearchResult,
	int32 Index)
{
	FBwayServerBrowserRowViewModel Row;
	if (!SearchResult)
	{
		Row.SessionName = FString::Printf(TEXT("Breakaway LAN Match #%d"), Index + 1);
		Row.MapName = TEXT("Dorado");
		Row.FormattedPlayerCount = FText::FromString(TEXT("1/8"));
		Row.FormattedPing = FText::FromString(TEXT("1 ms"));
		return Row;
	}

	Row.SessionId = SearchResult->GetDescription();

	bool bFoundMode = false;
	SearchResult->GetStringSetting(TEXT("GAMEMODE"), Row.PlaylistName, bFoundMode);

	FString RawMapName;
	bool bFoundMap = false;
	SearchResult->GetStringSetting(TEXT("MAPNAME"), RawMapName, bFoundMap);

	if (RawMapName.Contains(TEXT("Dorado"))
		|| RawMapName.Contains(TEXT("HeroSelect_Staging"))
		|| Row.PlaylistName.Contains(TEXT("CTR"))
		|| RawMapName.IsEmpty())
	{
		Row.MapName = TEXT("Dorado");
	}
	else
	{
		FString ShortMap = FPackageName::GetShortName(RawMapName);
		ShortMap.RemoveFromStart(TEXT("L_BW_"));
		ShortMap.RemoveFromStart(TEXT("L_"));
		Row.MapName = ShortMap.IsEmpty() ? TEXT("Dorado") : ShortMap;
	}

	const FString ShortSuffix = Row.SessionId.Len() >= 4 ? Row.SessionId.Right(4).ToUpper() : FString::FromInt(Index + 1);
	Row.SessionName = FString::Printf(TEXT("Breakaway LAN Match [%s]"), *ShortSuffix);

	const int32 ReportedMax = SearchResult->GetMaxPublicConnections();
	Row.MaxPlayers = (ReportedMax > 0 && ReportedMax <= 8) ? ReportedMax : 8;
	Row.OpenPublicConnections = FMath::Max(0, SearchResult->GetNumOpenPublicConnections());
	Row.bIsFull = (Row.OpenPublicConnections <= 0);
	Row.CurrentPlayers = Row.bIsFull
		? Row.MaxPlayers
		: FMath::Clamp(Row.MaxPlayers - Row.OpenPublicConnections, 1, FMath::Max(1, Row.MaxPlayers - 1));
	Row.FormattedPlayerCount = FText::FromString(FString::Printf(TEXT("%d/%d"), Row.CurrentPlayers, Row.MaxPlayers));

	Row.PingMs = SearchResult->GetPingInMs();
	Row.bIsUnreachable = (Row.PingMs < 0 || Row.PingMs >= 9999);
	Row.FormattedPing = Row.bIsUnreachable
		? NSLOCTEXT("BwayFE", "PingUnreachable", "UNREACHABLE")
		: FText::FromString(FString::Printf(TEXT("%d ms"), Row.PingMs));

	return Row;
}

void UBwayLocalMatchBrowserWidget::RefreshServerList()
{
	bPendingJoinAttempt = false;
	LastErrorMessage = FText::GetEmpty();
	DiscoveredSessions.Reset();
	RowViewModels.Reset();
	RowJoinButtons.Reset();
	if (ServerRowsBox)
	{
		ServerRowsBox->ClearChildren();
	}

	ViewState = EBwayServerBrowserViewState::Searching;
	RefreshBrowserStatePresentation();

	bool bStartedSearch = false;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UCommonSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UCommonSessionSubsystem>())
			{
				ActiveSearchRequest = SessionSubsystem->CreateOnlineSearchSessionRequest();
				if (ActiveSearchRequest)
				{
					ActiveSearchRequest->OnlineMode = ECommonSessionOnlineMode::LAN;
					ActiveSearchRequest->bUseLobbies = false;
					ActiveSearchRequest->OnSearchFinished.AddUObject(this, &ThisClass::HandleSessionSearchFinished);
					if (APlayerController* OwningPC = GetOwningPlayer())
					{
						bStartedSearch = true;
						SessionSubsystem->FindSessions(OwningPC, ActiveSearchRequest);
					}
				}
			}
		}
	}

	if (!bStartedSearch && ViewState == EBwayServerBrowserViewState::Searching)
	{
		// In headless widget tests without a live LocalPlayer/OnlineSubsystem, keep Searching observable until
		// ApplySyntheticBrowserStateForAudit or HandleSessionSearchFinished transitions the state.
		RequestRefreshFocus();
	}
}

void UBwayLocalMatchBrowserWidget::ClickRetrySearch()
{
	RefreshServerList();
}

void UBwayLocalMatchBrowserWidget::HandleSessionSearchFinished(bool bSucceeded, const FText& ErrorMessage)
{
	DiscoveredSessions.Reset();
	RowViewModels.Reset();

	if (!bSucceeded)
	{
		ViewState = EBwayServerBrowserViewState::Error;
		LastErrorMessage = ErrorMessage.IsEmpty()
			? NSLOCTEXT("BwayFE", "SearchFailedDefault", "LAN discovery failed. Check your network adapter and press Retry.")
			: ErrorMessage;
		RebuildRowWidgets();
		RefreshBrowserStatePresentation();
		RequestRefreshFocus();
		return;
	}

	if (ActiveSearchRequest && !ActiveSearchRequest->Results.IsEmpty())
	{
		for (UCommonSession_SearchResult* Result : ActiveSearchRequest->Results)
		{
			if (!Result)
			{
				continue;
			}
			const int32 RowIndex = DiscoveredSessions.Add(Result);
			RowViewModels.Add(BuildRowViewModelFromSearchResult(Result, RowIndex));
		}
	}

	ViewState = RowViewModels.IsEmpty()
		? EBwayServerBrowserViewState::Empty
		: EBwayServerBrowserViewState::Populated;
	LastErrorMessage = FText::GetEmpty();

	RebuildRowWidgets();
	RefreshBrowserStatePresentation();
	RequestRefreshFocus();
}

void UBwayLocalMatchBrowserWidget::ApplySyntheticBrowserStateForAudit(
	EBwayServerBrowserViewState NewState,
	const TArray<FBwayServerBrowserRowViewModel>& SyntheticRows,
	const FText& ErrorText)
{
	DiscoveredSessions.Reset();
	RowViewModels = SyntheticRows;
	for (FBwayServerBrowserRowViewModel& Row : RowViewModels)
	{
		if (Row.MaxPlayers <= 0)
		{
			Row.MaxPlayers = 8;
		}
		if (Row.MapName.IsEmpty())
		{
			Row.MapName = TEXT("Dorado");
		}
		if (Row.FormattedPlayerCount.IsEmpty())
		{
			Row.FormattedPlayerCount = FText::FromString(FString::Printf(TEXT("%d/%d"), Row.CurrentPlayers, Row.MaxPlayers));
		}
		if (Row.FormattedPing.IsEmpty())
		{
			Row.FormattedPing = Row.bIsUnreachable
				? NSLOCTEXT("BwayFE", "PingUnreachable", "UNREACHABLE")
				: FText::FromString(FString::Printf(TEXT("%d ms"), Row.PingMs));
		}
	}
	ViewState = NewState;
	LastErrorMessage = ErrorText;

	RebuildRowWidgets();
	RefreshBrowserStatePresentation();
	RequestRefreshFocus();
}

void UBwayLocalMatchBrowserWidget::RebuildRowWidgets()
{
	RowJoinButtons.Reset();
	if (!ServerRowsBox || !WidgetTree)
	{
		return;
	}

	ServerRowsBox->ClearChildren();

	for (int32 RowIndex = 0; RowIndex < RowViewModels.Num(); ++RowIndex)
	{
		const FBwayServerBrowserRowViewModel& Row = RowViewModels[RowIndex];

		UBorder* RowCard = WidgetTree->ConstructWidget<UBorder>();
		RowCard->SetBrushColor(FLinearColor(0.05f, 0.09f, 0.16f, 0.95f));
		RowCard->SetPadding(FMargin(16.f, 10.f));

		UHorizontalBox* RowHBox = WidgetTree->ConstructWidget<UHorizontalBox>();
		RowCard->SetContent(RowHBox);

		auto AddRowCell = [&](const FText& ValueText, float FillWeight, ETextJustify::Type Justify, const FLinearColor& Color)
		{
			UTextBlock* CellText = BwayFrontEndUI::MakeText(WidgetTree, ValueText, 15, Color);
			CellText->SetJustification(Justify);
			UHorizontalBoxSlot* CellSlot = RowHBox->AddChildToHorizontalBox(CellText);
			FSlateChildSize FillSize(ESlateSizeRule::Fill);
			FillSize.Value = FillWeight;
			CellSlot->SetSize(FillSize);
			CellSlot->SetVerticalAlignment(VAlign_Center);
		};

		AddRowCell(FText::FromString(Row.SessionName), 0.38f, ETextJustify::Left, FLinearColor::White);
		AddRowCell(FText::FromString(Row.MapName), 0.16f, ETextJustify::Center, BwayFrontEndUI::GoldBright);
		AddRowCell(
			Row.FormattedPlayerCount,
			0.14f,
			ETextJustify::Center,
			Row.bIsFull ? BwayFrontEndUI::WarningColor : FLinearColor::White);
		AddRowCell(
			Row.FormattedPing,
			0.20f,
			ETextJustify::Center,
			Row.bIsUnreachable ? BwayFrontEndUI::ErrorBorderColor : BwayFrontEndUI::CyanAccent);

		USizeBox* JoinSize = WidgetTree->ConstructWidget<USizeBox>();
		JoinSize->SetWidthOverride(130.f);
		const FText JoinLabel = Row.bIsFull
			? NSLOCTEXT("BwayFE", "Btn_JoinFull", "FULL (8/8)")
			: (Row.bIsUnreachable
				? NSLOCTEXT("BwayFE", "Btn_JoinOffline", "OFFLINE")
				: NSLOCTEXT("BwayFE", "Btn_Join", "JOIN"));
		UBwayCommonButton* JoinBtn = BwayFrontEndUI::MakeCommonButton(
			WidgetTree,
			*FString::Printf(TEXT("Button_JoinRow_%d"), RowIndex),
			JoinLabel,
			14,
			!Row.bIsFull && !Row.bIsUnreachable,
			(Row.bIsFull || Row.bIsUnreachable) ? BwayFrontEndUI::WarningColor : FLinearColor::White,
			FMargin(12.f, 8.f));
		const int32 CapturedRowIndex = RowIndex;
		JoinBtn->OnClicked().AddWeakLambda(this, [this, CapturedRowIndex]()
		{
			RequestJoinRow(CapturedRowIndex);
		});
		JoinSize->AddChild(JoinBtn);
		RowHBox->AddChildToHorizontalBox(JoinSize)->SetVerticalAlignment(VAlign_Center);

		ServerRowsBox->AddChildToVerticalBox(RowCard)->SetPadding(FMargin(0.f, 4.f));
		RowJoinButtons.Add(JoinBtn);
	}
}

void UBwayLocalMatchBrowserWidget::RefreshBrowserStatePresentation()
{
	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(ViewState != EBwayServerBrowserViewState::Searching);
	}

	const bool bHasError = (ViewState == EBwayServerBrowserViewState::Error) || !LastErrorMessage.IsEmpty();
	if (ErrorBannerBorder && ErrorBannerText)
	{
		ErrorBannerBorder->SetVisibility(bHasError ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ErrorBannerText->SetText(LastErrorMessage);
	}

	const bool bHasRows = !RowViewModels.IsEmpty();
	if (ServerRowsScrollBox)
	{
		ServerRowsScrollBox->SetVisibility(bHasRows ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (!SearchingStateBox || !SearchingSpinner || !ServerListStatusText || !ServerListSubStatusText)
	{
		return;
	}

	if (ViewState == EBwayServerBrowserViewState::Searching)
	{
		SearchingStateBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SearchingSpinner->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ServerListStatusText->SetText(NSLOCTEXT("BwayFE", "SearchingLanSessions", "SEARCHING FOR LAN SESSIONS..."));
		ServerListSubStatusText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ServerListSubStatusText->SetText(NSLOCTEXT("BwayFE", "SearchingLanSub", "Scanning local subnet for Breakaway Dorado hosts..."));
	}
	else if (bHasRows)
	{
		SearchingStateBox->SetVisibility(ESlateVisibility::Collapsed);
		SearchingSpinner->SetVisibility(ESlateVisibility::Collapsed);
		ServerListSubStatusText->SetVisibility(ESlateVisibility::Collapsed);
	}
	else if (ViewState == EBwayServerBrowserViewState::Error)
	{
		SearchingStateBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SearchingSpinner->SetVisibility(ESlateVisibility::Collapsed);
		ServerListStatusText->SetText(NSLOCTEXT("BwayFE", "UnableToReachLan", "UNABLE TO JOIN OR DISCOVER SESSION"));
		ServerListSubStatusText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ServerListSubStatusText->SetText(NSLOCTEXT("BwayFE", "ErrorRetryHint", "Use Retry above to scan again, or Host LAN Match below."));
	}
	else
	{
		SearchingStateBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SearchingSpinner->SetVisibility(ESlateVisibility::Collapsed);
		ServerListStatusText->SetText(NSLOCTEXT("BwayFE", "NoLanSessionsFound", "NO LAN SESSIONS FOUND"));
		ServerListSubStatusText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ServerListSubStatusText->SetText(NSLOCTEXT("BwayFE", "EmptyBrowserHint", "Press Refresh to scan again, or select Host LAN Match to start a Dorado lobby."));
	}
}

void UBwayLocalMatchBrowserWidget::ShowActionableJoinError(const FText& ErrorText)
{
	LastErrorMessage = ErrorText;
	if (RowViewModels.IsEmpty())
	{
		ViewState = EBwayServerBrowserViewState::Error;
	}
	RefreshBrowserStatePresentation();
	RequestRefreshFocus();
}

bool UBwayLocalMatchBrowserWidget::RequestJoinRow(int32 RowIndex)
{
	if (!RowViewModels.IsValidIndex(RowIndex))
	{
		ShowActionableJoinError(NSLOCTEXT("BwayFE", "ErrInvalidRow", "Selected LAN session is no longer available. Press Retry to refresh."));
		return false;
	}

	const FBwayServerBrowserRowViewModel& Row = RowViewModels[RowIndex];
	if (Row.bIsFull)
	{
		ShowActionableJoinError(FText::Format(
			NSLOCTEXT("BwayFE", "ErrSessionFullFmt", "Session '{0}' is full ({1} players). Press Retry to check for open slots."),
			FText::FromString(Row.SessionName),
			Row.FormattedPlayerCount));
		return false;
	}

	if (Row.bIsUnreachable)
	{
		ShowActionableJoinError(FText::Format(
			NSLOCTEXT("BwayFE", "ErrSessionUnreachableFmt", "Session '{0}' is unreachable on LAN. Verify the host is still online and press Retry."),
			FText::FromString(Row.SessionName)));
		return false;
	}

	LastErrorMessage = FText::GetEmpty();
	RefreshBrowserStatePresentation();

	if (DiscoveredSessions.IsValidIndex(RowIndex) && DiscoveredSessions[RowIndex])
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UCommonSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UCommonSessionSubsystem>())
				{
					bPendingJoinAttempt = true;
					SessionSubsystem->JoinSession(GetOwningPlayer(), DiscoveredSessions[RowIndex]);
					return true;
				}
			}
		}
	}

	// Synthetic audit row with open capacity & reachable ping
	return true;
}

void UBwayLocalMatchBrowserWidget::HandleJoinSessionComplete(const FOnlineResultInformation& Result)
{
	if (!bPendingJoinAttempt)
	{
		return;
	}
	bPendingJoinAttempt = false;

	if (!Result.bWasSuccessful)
	{
		const FText Detail = Result.ErrorText.IsEmpty()
			? NSLOCTEXT("BwayFE", "JoinFailedDefault", "Failed to join LAN session. The match may be full (8/8) or unreachable.")
			: Result.ErrorText;
		ShowActionableJoinError(Detail);
	}
}

void UBwayLocalMatchBrowserWidget::ClickHostMatch()
{
	bIsCreatingServer = true;
	if (CreatingServerOverlay)
	{
		CreatingServerOverlay->SetVisibility(ESlateVisibility::Visible);
	}

	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UBwayFrontendStateSubsystem* FrontendState = LP->GetSubsystem<UBwayFrontendStateSubsystem>())
		{
			FrontendState->SetLobbyOnlineMode(true);
			FrontendState->InitializeDefaultLobbyRoster();
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CreatingServerTimerHandle);
		World->GetTimerManager().SetTimer(
			CreatingServerTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				CompleteCreatingServerAndOpenLobby();
			}),
			0.25f,
			false);
	}
	else
	{
		CompleteCreatingServerAndOpenLobby();
	}
}

UCommonActivatableWidget* UBwayLocalMatchBrowserWidget::CompleteCreatingServerAndOpenLobby()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CreatingServerTimerHandle);
	}
	bIsCreatingServer = false;
	if (CreatingServerOverlay)
	{
		CreatingServerOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}

	const TSubclassOf<UCommonActivatableWidget> LobbyClass = BwayFrontEndUI::ResolveScreenClass(
		MatchLobbyWidgetClass,
		TEXT("/BreakawayCore/UI/FrontEnd/W_MatchLobby.W_MatchLobby_C"),
		UBwayMatchLobbyWidget::StaticClass());

	const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
	return UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, MenuLayer, LobbyClass);
}

void UBwayLocalMatchBrowserWidget::ClickBack()
{
	DeactivateWidget();
}

// ============================================================================
// UBwayMatchSettingsModalWidget (W_MatchSettingsModal)
// ============================================================================

UBwayMatchSettingsModalWidget::UBwayMatchSettingsModalWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> UBwayMatchSettingsModalWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

UBwayFrontendStateSubsystem* UBwayMatchSettingsModalWidget::GetFrontendSubsystem() const
{
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		return LP->GetSubsystem<UBwayFrontendStateSubsystem>();
	}
	return nullptr;
}

TSharedRef<SWidget> UBwayMatchSettingsModalWidget::RebuildWidget()
{
	if (UOverlay* Root = BwayFrontEndUI::EnsureRootOverlay(WidgetTree))
	{
		UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>();
		Dimmer->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.85f));
		UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dimmer);
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);

		USizeBox* ModalSize = WidgetTree->ConstructWidget<USizeBox>();
		ModalSize->SetWidthOverride(760.f);
		UOverlaySlot* ModalSlot = Root->AddChildToOverlay(ModalSize);
		ModalSlot->SetHorizontalAlignment(HAlign_Center);
		ModalSlot->SetVerticalAlignment(VAlign_Center);

		UBorder* ModalFrame = WidgetTree->ConstructWidget<UBorder>();
		ModalFrame->SetBrushColor(BwayFrontEndUI::PanelBg);
		ModalFrame->SetPadding(FMargin(32.f, 28.f));
		ModalSize->AddChild(ModalFrame);

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>();
		ModalFrame->SetContent(VBox);

		UHorizontalBox* CarouselRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* CarouselSlot = VBox->AddChildToVerticalBox(CarouselRow);
		CarouselSlot->SetHorizontalAlignment(HAlign_Center);
		CarouselSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

		PrevMapButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree, TEXT("Button_PrevMap"), FText::FromString(TEXT("<")), 22, false, BwayFrontEndUI::CyanAccent);
		CarouselRow->AddChildToHorizontalBox(PrevMapButton)->SetVerticalAlignment(VAlign_Center);

		USizeBox* MapCardSize = WidgetTree->ConstructWidget<USizeBox>();
		MapCardSize->SetWidthOverride(420.f);
		MapCardSize->SetHeightOverride(180.f);
		UBorder* MapCardBorder = WidgetTree->ConstructWidget<UBorder>();
		MapCardBorder->SetBrushColor(FLinearColor(0.10f, 0.16f, 0.24f, 0.96f));
		MapCardBorder->SetPadding(FMargin(20.f));

		UVerticalBox* MapCardVBox = WidgetTree->ConstructWidget<UVerticalBox>();
		MapNameText = BwayFrontEndUI::MakeText(
			WidgetTree, NSLOCTEXT("BwayFE", "DefaultMap", "The Shrine of Auros (Dorado)"), 22, BwayFrontEndUI::GoldBright);
		MapNameText->SetJustification(ETextJustify::Center);
		MapCardVBox->AddChildToVerticalBox(MapNameText)->SetPadding(FMargin(0.f, 28.f, 0.f, 10.f));

		MapDescText = BwayFrontEndUI::MakeText(WidgetTree, FText::GetEmpty(), 13, BwayFrontEndUI::MutedText);
		MapDescText->SetJustification(ETextJustify::Center);
		MapDescText->SetAutoWrapText(true);
		MapCardVBox->AddChildToVerticalBox(MapDescText);

		MapCardBorder->SetContent(MapCardVBox);
		MapCardSize->AddChild(MapCardBorder);
		CarouselRow->AddChildToHorizontalBox(MapCardSize)->SetPadding(FMargin(16.f, 0.f));

		NextMapButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree, TEXT("Button_NextMap"), FText::FromString(TEXT(">")), 22, false, BwayFrontEndUI::CyanAccent);
		CarouselRow->AddChildToHorizontalBox(NextMapButton)->SetVerticalAlignment(VAlign_Center);

		// ENABLE BOTS selector row
		{
			UHorizontalBox* BotsRow = WidgetTree->ConstructWidget<UHorizontalBox>();
			VBox->AddChildToVerticalBox(BotsRow)->SetPadding(FMargin(40.f, 16.f, 40.f, 8.f));

			UTextBlock* BotsLabel = BwayFrontEndUI::MakeText(
				WidgetTree, NSLOCTEXT("BwayFE", "EnableBotsLabel", "ENABLE BOTS"), 18, BwayFrontEndUI::MutedText);
			UHorizontalBoxSlot* LabelSlot = BotsRow->AddChildToHorizontalBox(BotsLabel);
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);

			EnableBotsButton = BwayFrontEndUI::MakeCommonButton(
				WidgetTree, TEXT("Button_EnableBots"), FText::FromString(TEXT("<  TRUE  >")), 17, false);
			BotsRow->AddChildToHorizontalBox(EnableBotsButton);
		}

		// BOT DIFFICULTY selector row
		{
			UHorizontalBox* DiffRow = WidgetTree->ConstructWidget<UHorizontalBox>();
			VBox->AddChildToVerticalBox(DiffRow)->SetPadding(FMargin(40.f, 8.f, 40.f, 28.f));

			UTextBlock* DiffLabel = BwayFrontEndUI::MakeText(
				WidgetTree, NSLOCTEXT("BwayFE", "BotDifficultyLabel", "BOT DIFFICULTY"), 18, BwayFrontEndUI::MutedText);
			UHorizontalBoxSlot* LabelSlot = DiffRow->AddChildToHorizontalBox(DiffLabel);
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LabelSlot->SetVerticalAlignment(VAlign_Center);

			BotDifficultyButton = BwayFrontEndUI::MakeCommonButton(
				WidgetTree, TEXT("Button_BotDifficulty"), FText::FromString(TEXT("<  NORMAL  >")), 17, false);
			DiffRow->AddChildToHorizontalBox(BotDifficultyButton);
		}

		// ACCEPT and CANCEL buttons
		UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* ActionSlot = VBox->AddChildToVerticalBox(ActionRow);
		ActionSlot->SetHorizontalAlignment(HAlign_Center);

		AcceptButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree, TEXT("Button_Accept"), NSLOCTEXT("BwayFE", "Btn_Accept", "ACCEPT"), 18, true);
		ActionRow->AddChildToHorizontalBox(AcceptButton)->SetPadding(FMargin(12.f, 0.f));

		CancelButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree, TEXT("Button_Cancel"), NSLOCTEXT("BwayFE", "Btn_Cancel", "CANCEL"), 18, false, BwayFrontEndUI::MutedText);
		ActionRow->AddChildToHorizontalBox(CancelButton)->SetPadding(FMargin(12.f, 0.f));
	}
	return Super::RebuildWidget();
}

void UBwayMatchSettingsModalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PrevMapButton)
	{
		PrevMapButton->OnClicked().RemoveAll(this);
		PrevMapButton->OnClicked().AddWeakLambda(this, [this]() { CycleMap(-1); });
	}
	if (NextMapButton)
	{
		NextMapButton->OnClicked().RemoveAll(this);
		NextMapButton->OnClicked().AddWeakLambda(this, [this]() { CycleMap(1); });
	}
	if (EnableBotsButton)
	{
		EnableBotsButton->OnClicked().RemoveAll(this);
		EnableBotsButton->OnClicked().AddUObject(this, &ThisClass::ToggleEnableBots);
	}
	if (BotDifficultyButton)
	{
		BotDifficultyButton->OnClicked().RemoveAll(this);
		BotDifficultyButton->OnClicked().AddWeakLambda(this, [this]() { CycleBotDifficulty(1); });
	}
	if (AcceptButton)
	{
		AcceptButton->OnClicked().RemoveAll(this);
		AcceptButton->OnClicked().AddUObject(this, &ThisClass::AcceptSettings);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().RemoveAll(this);
		CancelButton->OnClicked().AddUObject(this, &ThisClass::CancelSettings);
	}
}

void UBwayMatchSettingsModalWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (const UBwayFrontendStateSubsystem* Subsystem = GetFrontendSubsystem())
	{
		const FBwayMatchLobbySettings& Current = Subsystem->GetLobbySettings();
		WorkingMapIndex = Current.SelectedMapIndex;
		bWorkingBotsEnabled = Current.bBotsEnabled;
		WorkingBotDifficulty = Current.BotDifficulty;
	}
	RefreshModalDisplay();
	RequestRefreshFocus();
}

UWidget* UBwayMatchSettingsModalWidget::NativeGetDesiredFocusTarget() const
{
	return AcceptButton ? static_cast<UWidget*>(AcceptButton) : Super::NativeGetDesiredFocusTarget();
}

void UBwayMatchSettingsModalWidget::RefreshModalDisplay()
{
	if (const UBwayFrontendStateSubsystem* Subsystem = GetFrontendSubsystem())
	{
		const TArray<FBwayMapOptionEntry>& Maps = Subsystem->GetAvailableMaps();
		if (Maps.IsValidIndex(WorkingMapIndex))
		{
			if (MapNameText) MapNameText->SetText(Maps[WorkingMapIndex].DisplayName);
			if (MapDescText) MapDescText->SetText(Maps[WorkingMapIndex].Description);
		}
	}
	if (EnableBotsButton)
	{
		EnableBotsButton->SetLabelText(FText::FromString(bWorkingBotsEnabled ? TEXT("<  TRUE  >") : TEXT("<  FALSE  >")));
	}
	if (BotDifficultyButton)
	{
		const FText DiffText = UBwayFrontendStateSubsystem::GetBotDifficultyDisplayText(WorkingBotDifficulty);
		BotDifficultyButton->SetLabelText(FText::Format(NSLOCTEXT("BwayFE", "DiffFmt", "<  {0}  >"), DiffText));
	}
}

void UBwayMatchSettingsModalWidget::CycleMap(int32 Direction)
{
	if (const UBwayFrontendStateSubsystem* Subsystem = GetFrontendSubsystem())
	{
		const int32 NumMaps = Subsystem->GetAvailableMaps().Num();
		if (NumMaps > 0)
		{
			WorkingMapIndex = ((WorkingMapIndex + Direction) % NumMaps + NumMaps) % NumMaps;
		}
	}
	RefreshModalDisplay();
}

void UBwayMatchSettingsModalWidget::ToggleEnableBots()
{
	bWorkingBotsEnabled = !bWorkingBotsEnabled;
	RefreshModalDisplay();
}

void UBwayMatchSettingsModalWidget::SetWorkingEnableBots(bool bEnable)
{
	bWorkingBotsEnabled = bEnable;
	RefreshModalDisplay();
}

void UBwayMatchSettingsModalWidget::CycleBotDifficulty(int32 Direction)
{
	WorkingBotDifficulty = (WorkingBotDifficulty == EBwayBotDifficulty::Normal)
		? EBwayBotDifficulty::Elite
		: EBwayBotDifficulty::Normal;
	RefreshModalDisplay();
}

void UBwayMatchSettingsModalWidget::SetWorkingBotDifficulty(EBwayBotDifficulty Difficulty)
{
	WorkingBotDifficulty = Difficulty;
	RefreshModalDisplay();
}

void UBwayMatchSettingsModalWidget::AcceptSettings()
{
	if (UBwayFrontendStateSubsystem* Subsystem = GetFrontendSubsystem())
	{
		Subsystem->ApplyMatchSettingsModal(WorkingMapIndex, bWorkingBotsEnabled, WorkingBotDifficulty);
	}
	DeactivateWidget();
}

void UBwayMatchSettingsModalWidget::CancelSettings()
{
	DeactivateWidget();
}

// ============================================================================
// UBwayMatchLobbyWidget (W_MatchLobby)
// ============================================================================

UBwayMatchLobbyWidget::UBwayMatchLobbyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

TOptional<FUIInputConfig> UBwayMatchLobbyWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

UBwayFrontendStateSubsystem* UBwayMatchLobbyWidget::GetFrontendSubsystem() const
{
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		return LP->GetSubsystem<UBwayFrontendStateSubsystem>();
	}
	return nullptr;
}

TSharedRef<SWidget> UBwayMatchLobbyWidget::RebuildWidget()
{
	if (UOverlay* Root = BwayFrontEndUI::EnsureRootOverlay(WidgetTree))
	{
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(BwayFrontEndUI::DarkBackdrop);
		UOverlaySlot* BgSlot = Root->AddChildToOverlay(Backdrop);
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);

		UHorizontalBox* PartyBar = BwayFrontEndUI::BuildPersistentPartyHeaderBar(WidgetTree, TEXT("Player1"));
		UOverlaySlot* TopRightSlot = Root->AddChildToOverlay(PartyBar);
		TopRightSlot->SetHorizontalAlignment(HAlign_Right);
		TopRightSlot->SetVerticalAlignment(VAlign_Top);
		TopRightSlot->SetPadding(FMargin(0.f, 24.f, 36.f, 0.f));

		UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>();
		UOverlaySlot* MainSlot = Root->AddChildToOverlay(MainLayout);
		MainSlot->SetHorizontalAlignment(HAlign_Fill);
		MainSlot->SetVerticalAlignment(VAlign_Fill);
		MainSlot->SetPadding(FMargin(56.f, 36.f, 56.f, 32.f));

		UHorizontalBox* TopBannerRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		MainLayout->AddChildToVerticalBox(TopBannerRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));

		// Left Host Controls Box
		UBorder* HostControlsPanel = WidgetTree->ConstructWidget<UBorder>();
		HostControlsPanel->SetBrushColor(BwayFrontEndUI::PanelBg);
		HostControlsPanel->SetPadding(FMargin(18.f, 16.f));
		TopBannerRow->AddChildToHorizontalBox(HostControlsPanel)->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));

		UVerticalBox* HostControlsVBox = WidgetTree->ConstructWidget<UVerticalBox>();
		HostControlsPanel->SetContent(HostControlsVBox);

		MatchOptionsButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree,
			TEXT("Button_MatchOptions"),
			NSLOCTEXT("BwayFE", "Btn_MatchOptions", "MATCH OPTIONS"),
			15,
			false,
			BwayFrontEndUI::CyanAccent);
		HostControlsVBox->AddChildToVerticalBox(MatchOptionsButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

		InvitesSelectorButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree,
			TEXT("Button_InvitesSelector"),
			NSLOCTEXT("BwayFE", "Btn_InvitesHostOnly", "< INVITES (HOST ONLY) >"),
			13,
			false,
			BwayFrontEndUI::CyanAccent);
		HostControlsVBox->AddChildToVerticalBox(InvitesSelectorButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

		InvitePlayerButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree,
			TEXT("Button_InvitePlayer"),
			NSLOCTEXT("BwayFE", "Btn_InvitePlayer", "LAN DISCOVERY READY"),
			13,
			false,
			BwayFrontEndUI::MutedText);
		HostControlsVBox->AddChildToVerticalBox(InvitePlayerButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

		SummaryOnlineModeText = BwayFrontEndUI::MakeText(
			WidgetTree, NSLOCTEXT("BwayFE", "Sum_Mode", "Session Type: LAN (8 MAX)"), 13, BwayFrontEndUI::GoldBright);
		HostControlsVBox->AddChildToVerticalBox(SummaryOnlineModeText)->SetPadding(FMargin(0.f, 2.f));

		SummaryBotsEnabledText = BwayFrontEndUI::MakeText(
			WidgetTree, NSLOCTEXT("BwayFE", "Sum_Bots", "Bots Enabled: TRUE"), 13, BwayFrontEndUI::MutedText);
		HostControlsVBox->AddChildToVerticalBox(SummaryBotsEnabledText)->SetPadding(FMargin(0.f, 2.f));

		SummaryBotDifficultyText = BwayFrontEndUI::MakeText(
			WidgetTree, NSLOCTEXT("BwayFE", "Sum_Diff", "Bot Difficulty: NORMAL"), 13, BwayFrontEndUI::MutedText);
		HostControlsVBox->AddChildToVerticalBox(SummaryBotDifficultyText)->SetPadding(FMargin(0.f, 2.f));

		SummaryInvitesText = BwayFrontEndUI::MakeText(
			WidgetTree, NSLOCTEXT("BwayFE", "Sum_Inv", "Invites: HOST ONLY"), 13, BwayFrontEndUI::MutedText);
		HostControlsVBox->AddChildToVerticalBox(SummaryInvitesText)->SetPadding(FMargin(0.f, 2.f));

		// Map Header Banner
		UBorder* MapBannerPanel = WidgetTree->ConstructWidget<UBorder>();
		MapBannerPanel->SetBrushColor(FLinearColor(0.08f, 0.13f, 0.20f, 0.90f));
		MapBannerPanel->SetPadding(FMargin(32.f, 24.f));
		UHorizontalBoxSlot* BannerSlot = TopBannerRow->AddChildToHorizontalBox(MapBannerPanel);
		BannerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UVerticalBox* BannerVBox = WidgetTree->ConstructWidget<UVerticalBox>();
		MapBannerPanel->SetContent(BannerVBox);

		LobbyModeBadgeText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "HostLanMatchBadge", "HOST LAN MATCH  •  DORADO (8 PLAYERS)"),
			15,
			BwayFrontEndUI::GoldAccent,
			TEXT("Text_LobbyModeBadge"));
		BannerVBox->AddChildToVerticalBox(LobbyModeBadgeText)->SetPadding(FMargin(0.f, 8.f, 0.f, 6.f));

		MapHeaderText = BwayFrontEndUI::MakeText(
			WidgetTree, NSLOCTEXT("BwayFE", "ShrineHeader", "THE SHRINE OF AUROS — DORADO"), 34, FLinearColor::White);
		BannerVBox->AddChildToVerticalBox(MapHeaderText);

		// Middle Section: 3-Column Team Roster Grid (BLUE TEAM, RED TEAM, SPECTATORS)
		UHorizontalBox* RosterColumnsRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* RosterSlot = MainLayout->AddChildToVerticalBox(RosterColumnsRow);
		RosterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RosterSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));

		BlueSlotButtons.Reset();
		RedSlotButtons.Reset();
		SpectatorSlotButtons.Reset();

		auto BuildTeamColumn = [this, RosterColumnsRow](
			const FText& HeaderTitle,
			const FLinearColor& HeaderColor,
			const TCHAR* Prefix,
			TArray<TObjectPtr<UBwayCommonButton>>& OutButtons)
		{
			UVerticalBox* ColBox = WidgetTree->ConstructWidget<UVerticalBox>();
			UHorizontalBoxSlot* ColSlot = RosterColumnsRow->AddChildToHorizontalBox(ColBox);
			ColSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ColSlot->SetPadding(FMargin(8.f, 0.f));

			UBorder* HeaderBar = WidgetTree->ConstructWidget<UBorder>();
			HeaderBar->SetBrushColor(HeaderColor);
			HeaderBar->SetPadding(FMargin(16.f, 10.f));
			UTextBlock* HeaderText = BwayFrontEndUI::MakeText(WidgetTree, HeaderTitle, 17, FLinearColor::White);
			HeaderBar->SetContent(HeaderText);
			ColBox->AddChildToVerticalBox(HeaderBar)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

			for (int32 i = 0; i < 4; ++i)
			{
				UBwayCommonButton* SlotBtn = BwayFrontEndUI::MakeCommonButton(
					WidgetTree,
					*FString::Printf(TEXT("Button_%sSlot_%d"), Prefix, i),
					NSLOCTEXT("BwayFE", "ClickToJoin", "click to join"),
					14,
					false,
					BwayFrontEndUI::MutedText,
					FMargin(16.f, 10.f));
				SlotBtn->SetJustification(ETextJustify::Left);
				ColBox->AddChildToVerticalBox(SlotBtn)->SetPadding(FMargin(0.f, 4.f));
				OutButtons.Add(SlotBtn);
			}
		};

		BuildTeamColumn(NSLOCTEXT("BwayFE", "Col_BlueTeam", "BLUE TEAM"), BwayFrontEndUI::BlueTeamHeader, TEXT("Blue"), BlueSlotButtons);
		BuildTeamColumn(NSLOCTEXT("BwayFE", "Col_RedTeam", "RED TEAM"), BwayFrontEndUI::RedTeamHeader, TEXT("Red"), RedSlotButtons);
		BuildTeamColumn(NSLOCTEXT("BwayFE", "Col_Spectators", "SPECTATORS"), BwayFrontEndUI::SpecTeamHeader, TEXT("Spec"), SpectatorSlotButtons);

		// Bottom Bar: Leave, Match Options, and bottom-right START
		UHorizontalBox* BottomBar = WidgetTree->ConstructWidget<UHorizontalBox>();
		MainLayout->AddChildToVerticalBox(BottomBar);

		LeaveButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree, TEXT("Button_LeaveLobby"), NSLOCTEXT("BwayFE", "Btn_Leave", "BACK"), 16, false, FLinearColor::White);
		BottomBar->AddChildToHorizontalBox(LeaveButton)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));

		SettingsButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree, TEXT("Button_LobbySettings"), NSLOCTEXT("BwayFE", "Btn_LobbyOptions", "MATCH OPTIONS"), 16, false, FLinearColor::White);
		BottomBar->AddChildToHorizontalBox(SettingsButton);

		USpacer* FlexSpacer = WidgetTree->ConstructWidget<USpacer>();
		BottomBar->AddChildToHorizontalBox(FlexSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		USizeBox* StartSize = WidgetTree->ConstructWidget<USizeBox>();
		StartSize->SetWidthOverride(300.f);
		StartButton = BwayFrontEndUI::MakeCommonButton(
			WidgetTree, TEXT("Button_StartMatch"), NSLOCTEXT("BwayFE", "Btn_StartLan", "HOST LAN SESSION"), 20, true, FLinearColor::White);
		StartSize->AddChild(StartButton);
		BottomBar->AddChildToHorizontalBox(StartSize);
	}
	return Super::RebuildWidget();
}

void UBwayMatchLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MatchOptionsButton)
	{
		MatchOptionsButton->OnClicked().RemoveAll(this);
		MatchOptionsButton->OnClicked().AddWeakLambda(this, [this]() { OpenMatchOptions(); });
	}
	if (InvitesSelectorButton)
	{
		InvitesSelectorButton->OnClicked().RemoveAll(this);
		InvitesSelectorButton->OnClicked().AddUObject(this, &ThisClass::ClickCycleInviteMode);
	}
	if (InvitePlayerButton)
	{
		InvitePlayerButton->OnClicked().RemoveAll(this);
		InvitePlayerButton->OnClicked().AddUObject(this, &ThisClass::ClickInvitePlayer);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked().RemoveAll(this);
		LeaveButton->OnClicked().AddUObject(this, &ThisClass::ClickLeaveLobby);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked().RemoveAll(this);
		SettingsButton->OnClicked().AddWeakLambda(this, [this]() { OpenMatchOptions(); });
	}
	if (StartButton)
	{
		StartButton->OnClicked().RemoveAll(this);
		StartButton->OnClicked().AddUObject(this, &ThisClass::ClickStartMatch);
	}

	for (int32 i = 0; i < BlueSlotButtons.Num(); ++i)
	{
		if (BlueSlotButtons[i])
		{
			const int32 SlotIdx = i;
			BlueSlotButtons[i]->OnClicked().RemoveAll(this);
			BlueSlotButtons[i]->OnClicked().AddWeakLambda(this, [this, SlotIdx]() { SelectTeamSlot(EBwayLobbyTeam::BlueTeam, SlotIdx); });
		}
	}
	for (int32 i = 0; i < RedSlotButtons.Num(); ++i)
	{
		if (RedSlotButtons[i])
		{
			const int32 SlotIdx = i;
			RedSlotButtons[i]->OnClicked().RemoveAll(this);
			RedSlotButtons[i]->OnClicked().AddWeakLambda(this, [this, SlotIdx]() { SelectTeamSlot(EBwayLobbyTeam::RedTeam, SlotIdx); });
		}
	}
	for (int32 i = 0; i < SpectatorSlotButtons.Num(); ++i)
	{
		if (SpectatorSlotButtons[i])
		{
			const int32 SlotIdx = i;
			SpectatorSlotButtons[i]->OnClicked().RemoveAll(this);
			SpectatorSlotButtons[i]->OnClicked().AddWeakLambda(this, [this, SlotIdx]() { SelectTeamSlot(EBwayLobbyTeam::Spectators, SlotIdx); });
		}
	}
}

void UBwayMatchLobbyWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (UBwayFrontendStateSubsystem* Subsystem = GetFrontendSubsystem())
	{
		Subsystem->OnLobbySettingsChanged.AddUniqueDynamic(this, &ThisClass::RefreshLobbyDisplay);
		if (Subsystem->GetLobbySettings().BlueTeamSlots.IsEmpty())
		{
			Subsystem->InitializeDefaultLobbyRoster();
		}
	}
	RefreshLobbyDisplay();
	RequestRefreshFocus();
}

void UBwayMatchLobbyWidget::NativeOnDeactivated()
{
	if (UBwayFrontendStateSubsystem* Subsystem = GetFrontendSubsystem())
	{
		Subsystem->OnLobbySettingsChanged.RemoveDynamic(this, &ThisClass::RefreshLobbyDisplay);
	}
	Super::NativeOnDeactivated();
}

UWidget* UBwayMatchLobbyWidget::NativeGetDesiredFocusTarget() const
{
	return StartButton ? static_cast<UWidget*>(StartButton) : Super::NativeGetDesiredFocusTarget();
}

void UBwayMatchLobbyWidget::RefreshLobbyDisplay()
{
	const UBwayFrontendStateSubsystem* Subsystem = GetFrontendSubsystem();
	if (!Subsystem)
	{
		return;
	}

	const FBwayMapOptionEntry MapEntry = Subsystem->GetSelectedMapEntry();
	const FBwayMatchLobbySettings& Settings = Subsystem->GetLobbySettings();

	if (LobbyModeBadgeText)
	{
		LobbyModeBadgeText->SetText(Settings.bIsLanMatch
			? NSLOCTEXT("BwayFE", "HostLanMatchBadge", "HOST LAN MATCH  •  STAGING LOBBY -> DORADO (8 MAX)")
			: NSLOCTEXT("BwayFE", "DevPracticeBadge", "DEV PRACTICE / BOT MATCH  •  OFFLINE STAGING -> DORADO"));
	}
	if (MapHeaderText)
	{
		MapHeaderText->SetText(FText::FromString(FString::Printf(TEXT("%s (DORADO)"), *MapEntry.DisplayName.ToString().ToUpper())));
	}
	if (StartButton)
	{
		StartButton->SetLabelText(Settings.bIsLanMatch
			? NSLOCTEXT("BwayFE", "Btn_StartLan", "HOST LAN SESSION")
			: NSLOCTEXT("BwayFE", "Btn_StartPractice", "START BOT PRACTICE"));
	}
	if (SummaryOnlineModeText)
	{
		SummaryOnlineModeText->SetText(Settings.bIsLanMatch
			? NSLOCTEXT("BwayFE", "Sum_ModeLan", "Session Type: LAN (8 MAX)")
			: NSLOCTEXT("BwayFE", "Sum_ModeOffline", "Session Type: OFFLINE PRACTICE"));
	}

	const FText DiffText = UBwayFrontendStateSubsystem::GetBotDifficultyDisplayText(Settings.BotDifficulty);
	const FText InviteText = UBwayFrontendStateSubsystem::GetInviteModeDisplayText(Settings.InviteMode);

	if (InvitesSelectorButton)
	{
		InvitesSelectorButton->SetLabelText(FText::Format(NSLOCTEXT("BwayFE", "InvitesBtnFmt", "< INVITES ({0}) >"), InviteText));
	}
	if (SummaryBotsEnabledText)
	{
		SummaryBotsEnabledText->SetText(FText::FromString(
			FString::Printf(TEXT("Bots Enabled: %s"), Settings.bBotsEnabled ? TEXT("TRUE") : TEXT("FALSE"))));
	}
	if (SummaryBotDifficultyText)
	{
		SummaryBotDifficultyText->SetText(FText::Format(
			NSLOCTEXT("BwayFE", "SummaryBotDiffFmt", "Bot Difficulty: {0}"), DiffText));
	}
	if (SummaryInvitesText)
	{
		SummaryInvitesText->SetText(FText::Format(
			NSLOCTEXT("BwayFE", "SummaryInvitesFmt", "Invites: {0}"), InviteText));
	}

	auto UpdateColumnButtons = [](const TArray<FString>& SlotsData, TArray<TObjectPtr<UBwayCommonButton>>& Buttons)
	{
		for (int32 i = 0; i < Buttons.Num(); ++i)
		{
			if (!Buttons[i]) continue;
			const bool bOccupied = SlotsData.IsValidIndex(i) && !SlotsData[i].IsEmpty();
			Buttons[i]->SetLabelText(bOccupied
				? FText::FromString(SlotsData[i])
				: NSLOCTEXT("BwayFE", "ClickToJoin", "click to join"));
			Buttons[i]->SetHighlightedState(bOccupied);
		}
	};

	UpdateColumnButtons(Settings.BlueTeamSlots, BlueSlotButtons);
	UpdateColumnButtons(Settings.RedTeamSlots, RedSlotButtons);
	UpdateColumnButtons(Settings.SpectatorSlots, SpectatorSlotButtons);
}

UCommonActivatableWidget* UBwayMatchLobbyWidget::OpenMatchOptions()
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}

	const TSubclassOf<UCommonActivatableWidget> ModalClass = BwayFrontEndUI::ResolveScreenClass(
		MatchSettingsModalClass,
		TEXT("/BreakawayCore/UI/FrontEnd/W_MatchSettingsModal.W_MatchSettingsModal_C"),
		UBwayMatchSettingsModalWidget::StaticClass());

	const FGameplayTag ModalLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Modal"));
	return UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, ModalLayer, ModalClass);
}

void UBwayMatchLobbyWidget::ClickCycleInviteMode()
{
	if (UBwayFrontendStateSubsystem* Subsystem = GetFrontendSubsystem())
	{
		Subsystem->CycleInviteMode(1);
	}
}

void UBwayMatchLobbyWidget::ClickInvitePlayer()
{
}

bool UBwayMatchLobbyWidget::SelectTeamSlot(EBwayLobbyTeam Team, int32 SlotIndex)
{
	if (UBwayFrontendStateSubsystem* Subsystem = GetFrontendSubsystem())
	{
		return Subsystem->MoveLocalHostToSlot(Team, SlotIndex);
	}
	return false;
}

void UBwayMatchLobbyWidget::ClickLeaveLobby()
{
	APlayerController* OwningPC = GetOwningPlayer();
	DeactivateWidget();

	if (OwningPC)
	{
		for (TObjectIterator<UBwayLocalMatchBrowserWidget> It; It; ++It)
		{
			if (!It->IsTemplate() && It->GetOwningPlayer() == OwningPC)
			{
				It->DeactivateWidget();
			}
		}
		for (TObjectIterator<UBwayModeSelectionWidget> It; It; ++It)
		{
			if (!It->IsTemplate() && It->GetOwningPlayer() == OwningPC)
			{
				It->DeactivateWidget();
			}
		}
	}
}

bool UBwayMatchLobbyWidget::HostConfiguredMatch(const TMap<FString, FString>& AdditionalExtraArgs, FString& OutAdvertisedPlaylist)
{
	OutAdvertisedPlaylist.Reset();
	UWorld* World = GetWorld();
	APlayerController* OwningPC = GetOwningPlayer();
	if (!World || !OwningPC)
	{
		return false;
	}

	if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		AssetManager->ScanPathsForPrimaryAssets(
			UAssetManager::MapType,
			{TEXT("/BreakawayCore/Maps")},
			UWorld::StaticClass(),
			false,
			false,
			true);
	}
	UBwayExperienceLibrary::RegisterPackagedBreakawayExperiences();

	UBwayFrontendStateSubsystem* Subsystem = GetFrontendSubsystem();
	bool bLanMode = true;
	if (Subsystem)
	{
		Subsystem->SetSelectedHeroId(FPrimaryAssetId());
		bLanMode = Subsystem->GetLobbySettings().bIsLanMatch;
	}

	UGameInstance* GI = World->GetGameInstance();
	UCommonSessionSubsystem* SessionSubsystem = GI ? GI->GetSubsystem<UCommonSessionSubsystem>() : nullptr;
	const UBwayUserFacingExperienceDefinition* Playlist = LoadObject<UBwayUserFacingExperienceDefinition>(
		nullptr,
		TEXT("/Game/System/DA_Playlist_CTR_LAN.DA_Playlist_CTR_LAN"));
	UCommonSession_HostSessionRequest* Request = Playlist ? Playlist->CreateHostingRequest(World) : nullptr;

	if (SessionSubsystem && Request)
	{
		Request->OnlineMode = bLanMode ? ECommonSessionOnlineMode::LAN : ECommonSessionOnlineMode::Offline;
		Request->bUseLobbies = false;
		Request->bUsePresence = false;
		Request->MaxPlayerCount = 8;

		if (Subsystem)
		{
			TMap<FString, FString> LobbyExtraArgs;
			Subsystem->BuildMatchExtraArgs(LobbyExtraArgs);
			for (const TPair<FString, FString>& Pair : LobbyExtraArgs)
			{
				Request->ExtraArgs.Add(Pair.Key, Pair.Value);
			}
		}
		for (const TPair<FString, FString>& Pair : AdditionalExtraArgs)
		{
			Request->ExtraArgs.Add(Pair.Key, Pair.Value);
		}

		FText ValidationError;
		if (Request->ValidateAndLogErrors(ValidationError))
		{
			OutAdvertisedPlaylist = Request->ModeNameForAdvertisement;
			UE_LOG(LogTemp, Log, TEXT("BwayMatchLobbyWidget: Hosting %s session via %s (MaxPlayers=%d, URL=%s)"),
				bLanMode ? TEXT("LAN") : TEXT("Offline"),
				*OutAdvertisedPlaylist,
				Request->MaxPlayerCount,
				*Request->ConstructTravelURL());
			SessionSubsystem->HostSession(OwningPC, Request);
			return true;
		}
	}

	// Fallback direct travel through staging if CommonSessionSubsystem/Playlist could not resolve
	const FBwayMapOptionEntry MapEntry = Subsystem ? Subsystem->GetSelectedMapEntry() : FBwayMapOptionEntry();
	const FString TargetExpName = MapEntry.ExperienceAssetId.IsValid()
		? MapEntry.ExperienceAssetId.PrimaryAssetName.ToString()
		: TEXT("B_BW_Experience_CaptureTheRelic");

	FString Options = FString::Printf(
		TEXT("Experience=B_BW_Experience_HeroSelectStaging?HeroSelectStaging=1?HeroSelectTargetMap=%s?HeroSelectTargetExperience=%s"),
		*MapEntry.MapPackagePath,
		*TargetExpName);
	if (bLanMode)
	{
		Options += TEXT("?bIsLanMatch?listen");
	}
	if (Subsystem)
	{
		TMap<FString, FString> ExtraArgs;
		Subsystem->BuildMatchExtraArgs(ExtraArgs);
		for (const TPair<FString, FString>& Pair : ExtraArgs)
		{
			Options += FString::Printf(TEXT("?%s=%s"), *Pair.Key, *Pair.Value);
		}
	}
	for (const TPair<FString, FString>& Pair : AdditionalExtraArgs)
	{
		Options += FString::Printf(TEXT("?%s=%s"), *Pair.Key, *Pair.Value);
	}

	OutAdvertisedPlaylist = TEXT("DA_Playlist_CTR_LAN");
	UGameplayStatics::OpenLevel(World, FName(TEXT("/BreakawayCore/Maps/L_BW_HeroSelect_Staging")), true, Options);
	return true;
}

void UBwayMatchLobbyWidget::ClickStartMatch()
{
	FString AdvertisedPlaylist;
	HostConfiguredMatch(TMap<FString, FString>(), AdvertisedPlaylist);
}

#if !UE_BUILD_SHIPPING
namespace BwayFrontEndFlowHarness
{
	bool bFlowProbeRunning = false;

	struct FFrontEndToMatchFlowProbe
	{
		bool bExitOnComplete = false;
		int32 Step = 0;
		double StartedAt = FPlatformTime::Seconds();
		double StepChangedAt = StartedAt;
		bool bObservedArenaIntro = false;
		FPrimaryAssetId LockedHeroId;
		float BaseAttackStrength = 0.0f;
		float UpgradedAttackStrength = 0.0f;

		void Advance(int32 NextStep)
		{
			Step = NextStep;
			StepChangedAt = FPlatformTime::Seconds();
		}

		bool Fail(const TCHAR* Reason)
		{
			UE_LOG(LogTemp, Error, TEXT("[BWAY_FE_FLOW] FAIL step=%d reason=%s"), Step, Reason);
			bFlowProbeRunning = false;
			return false;
		}

		bool Tick(float)
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - StartedAt > 180.0 || Now - StepChangedAt > 60.0)
			{
				return Fail(TEXT("timeout"));
			}

			UWorld* World = BwayFrontEndUI::FindActiveRuntimeWorld();
			if (!World)
			{
				return true;
			}

			APlayerController* RawPC = World->GetGameInstance()->GetFirstLocalPlayerController();
			if (!RawPC || !RawPC->GetLocalPlayer())
			{
				return true;
			}

			// Step 0: Main Menu (W_MainMenu) -> verify clean Breakaway flows -> round-trip Find LAN Match -> Host LAN Match
			if (Step == 0)
			{
				UBwayMainMenuWidget* MainMenu = BwayFrontEndUI::FindActiveWidgetForPlayer<UBwayMainMenuWidget>(RawPC);
				if (!MainMenu || !MainMenu->IsActivated())
				{
					return true;
				}

				for (TObjectIterator<UCommonActivatableWidget> It; It; ++It)
				{
					if (!It->IsTemplate() && It->GetOwningPlayer() == RawPC && It->IsActivated()
						&& It->GetClass()->GetName() == TEXT("W_LyraFrontEnd_C"))
					{
						return Fail(TEXT("leftover-lyra-frontend-active"));
					}
				}

				UE_LOG(LogTemp, Display, TEXT("[BWAY_FE_FLOW] Step0_MainMenu: class=%s buttons=%d active=1"),
					*MainMenu->GetClass()->GetPathName(), MainMenu->GetVisibleMenuButtonLabels().Num());

				UBwayLocalMatchBrowserWidget* Browser = Cast<UBwayLocalMatchBrowserWidget>(MainMenu->ClickFindLanMatch());
				if (!Browser)
				{
					return Fail(TEXT("main-menu-open-browser-failed"));
				}
				Advance(1);
				return true;
			}

			// Step 1: Server Browser -> Back -> MainMenu -> ClickHostLanMatch -> W_MatchLobby
			if (Step == 1)
			{
				UBwayLocalMatchBrowserWidget* Browser = BwayFrontEndUI::FindActiveWidgetForPlayer<UBwayLocalMatchBrowserWidget>(RawPC);
				if (!Browser)
				{
					return true;
				}

				Browser->ClickBack();
				UBwayMainMenuWidget* MainMenuAfterBack = BwayFrontEndUI::FindActiveWidgetForPlayer<UBwayMainMenuWidget>(RawPC);
				if (!MainMenuAfterBack || !MainMenuAfterBack->IsActivated())
				{
					return Fail(TEXT("browser-back-did-not-restore-main-menu"));
				}

				UCommonActivatableWidget* Lobby = MainMenuAfterBack->ClickHostLanMatch();
				if (!Lobby)
				{
					return Fail(TEXT("main-menu-host-lan-match-failed"));
				}
				Advance(3);
				return true;
			}

			// Step 3: Match Lobby (W_MatchLobby) & Match Settings Modal (W_MatchSettingsModal) -> Host LAN Session
			if (Step == 3)
			{
				UBwayMatchLobbyWidget* Lobby = BwayFrontEndUI::FindActiveWidgetForPlayer<UBwayMatchLobbyWidget>(RawPC);
				if (!Lobby)
				{
					return true;
				}

				UBwayFrontendStateSubsystem* Subsystem = RawPC->GetLocalPlayer()->GetSubsystem<UBwayFrontendStateSubsystem>();
				if (!Subsystem)
				{
					return Fail(TEXT("missing-frontend-state-subsystem"));
				}

				const FBwayMatchLobbySettings& InitialSettings = Subsystem->GetLobbySettings();
				if (InitialSettings.BlueTeamSlots.IsEmpty() || !InitialSettings.BlueTeamSlots[0].Contains(TEXT("(Host)")))
				{
					return Fail(TEXT("lobby-initial-blue-slot-0-missing-host"));
				}

				UBwayMatchSettingsModalWidget* Modal = Cast<UBwayMatchSettingsModalWidget>(Lobby->OpenMatchOptions());
				if (!Modal || !Modal->IsActivated())
				{
					return Fail(TEXT("lobby-open-match-settings-modal-failed"));
				}

				Modal->CycleMap(1);
				Modal->CycleMap(-1);
				Modal->SetWorkingEnableBots(true);
				Modal->SetWorkingBotDifficulty(EBwayBotDifficulty::Elite);
				const FString ModalClassPath = Modal->GetClass()->GetPathName();
				Modal->AcceptSettings();

				const FBwayMatchLobbySettings& UpdatedSettings = Subsystem->GetLobbySettings();
				const FBwayMapOptionEntry SelectedMap = Subsystem->GetSelectedMapEntry();
				if (!UpdatedSettings.bBotsEnabled || UpdatedSettings.BotDifficulty != EBwayBotDifficulty::Elite
					|| !SelectedMap.DisplayName.ToString().Contains(TEXT("Shrine of Auros")))
				{
					return Fail(TEXT("modal-accept-did-not-commit-settings"));
				}

				Lobby->SelectTeamSlot(EBwayLobbyTeam::RedTeam, 1);
				if (Subsystem->GetLobbySettings().LocalHostTeam != EBwayLobbyTeam::RedTeam)
				{
					return Fail(TEXT("lobby-team-slot-switch-failed"));
				}
				Lobby->SelectTeamSlot(EBwayLobbyTeam::BlueTeam, 0);

				UE_LOG(LogTemp, Display,
					TEXT("[BWAY_FE_FLOW] Step3_MatchLobbyAndModal: lobbyClass=%s modalClass=%s map='%s' botsEnabled=%s botDifficulty=%s hostSlot='%s'"),
					*Lobby->GetClass()->GetPathName(),
					*ModalClassPath,
					*SelectedMap.DisplayName.ToString(),
					UpdatedSettings.bBotsEnabled ? TEXT("TRUE") : TEXT("FALSE"),
					*UBwayFrontendStateSubsystem::GetBotDifficultyDisplayText(UpdatedSettings.BotDifficulty).ToString(),
					*Subsystem->GetLobbySettings().BlueTeamSlots[0]);

				Lobby->ClickStartMatch();
				Advance(4);
				return true;
			}

			// Step 4: Hero Selection Screen (W_HeroSelection) on L_BW_HeroSelect_Staging (or L_BW_Dorado)
			if (Step == 4)
			{
				const bool bOnSelectionMap = World->GetMapName().Contains(TEXT("L_BW_HeroSelect_Staging"))
					|| World->GetMapName().Contains(TEXT("L_BW_Dorado"));
				if (!bOnSelectionMap)
				{
					return true;
				}

				ABwayPlayerController* PC = Cast<ABwayPlayerController>(RawPC);
				ABwayGameState* GS = World->GetGameState<ABwayGameState>();
				ABwayPlayerState* PS = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
				if (!PC || !GS || !PS || !GS->HeroSelectionManager || !GS->HeroSelectionManager->IsSelectionActive())
				{
					return true;
				}

				UBwayHeroSelectWidget* HeroSelect = BwayFrontEndUI::FindActiveWidgetForPlayer<UBwayHeroSelectWidget>(PC);
				if (!HeroSelect || Now - StepChangedAt < 0.5)
				{
					return true;
				}

				const TArray<FHeroDisplayInfo> Heroes = HeroSelect->GetAvailableHeroes();
				if (Heroes.IsEmpty())
				{
					return Fail(TEXT("hero-select-no-heroes-in-registry"));
				}

				FPrimaryAssetId ChosenHeroId;
				for (const FHeroDisplayInfo& Info : Heroes)
				{
					if (Info.bIsAvailable && Info.DisplayName.ToString().Contains(TEXT("Rawlins")))
					{
						ChosenHeroId = Info.HeroId;
						break;
					}
				}
				if (!ChosenHeroId.IsValid())
				{
					for (const FHeroDisplayInfo& Info : Heroes)
					{
						if (Info.bIsAvailable)
						{
							ChosenHeroId = Info.HeroId;
							break;
						}
					}
				}
				if (!ChosenHeroId.IsValid())
				{
					return Fail(TEXT("hero-select-no-available-hero-for-local-player"));
				}

				if (!HeroSelect->SelectHero(ChosenHeroId))
				{
					return Fail(TEXT("hero-select-select-hero-failed"));
				}

				ABwayHeroPreviewActor* PreviewActor = HeroSelect->GetPreviewActor();
				if (!PreviewActor || !PreviewActor->GetPreviewMeshComponent())
				{
					return Fail(TEXT("hero-select-3d-preview-actor-missing"));
				}

				const float CountdownRemaining = GS->HeroSelectionManager->GetSelectionTimeRemaining();
				const FString HeroSelectClassPath = HeroSelect->GetClass()->GetPathName();
				const FString PreviewActorName = GetNameSafe(PreviewActor);

				if (!HeroSelect->LockSelection())
				{
					return Fail(TEXT("hero-select-lock-selection-failed"));
				}

				LockedHeroId = PS->GetSelectedHeroId();
				if (!PS->IsHeroLocked() || LockedHeroId != ChosenHeroId)
				{
					return Fail(TEXT("hero-select-player-state-not-locked"));
				}

				UE_LOG(LogTemp, Display,
					TEXT("[BWAY_FE_FLOW] Step4_HeroSelection: class=%s map=%s hero=%s previewActor=%s timerRemaining=%.1f locked=1"),
					*HeroSelectClassPath,
					*World->GetMapName(),
					*LockedHeroId.ToString(),
					*PreviewActorName,
					CountdownRemaining);

				bObservedArenaIntro = false;
				Advance(5);
				return true;
			}

			// Step 5: Arena Intro Fly-Through Sequence on L_BW_Dorado -> Warmup
			if (Step == 5)
			{
				if (!World->GetMapName().Contains(TEXT("L_BW_Dorado")))
				{
					return true;
				}
				ABwayPlayerController* PC = Cast<ABwayPlayerController>(RawPC);
				ABwayGameState* GS = World->GetGameState<ABwayGameState>();
				UBwayRoundManagementComponent* RoundComp = GS ? GS->GetRoundManagement() : nullptr;
				if (!PC || !RoundComp)
				{
					return true;
				}

				if (RoundComp->IsArenaIntroActive() && !bObservedArenaIntro)
				{
					bObservedArenaIntro = true;
					AActor* ViewTarget = PC->GetViewTarget();
					UE_LOG(LogTemp, Display,
						TEXT("[BWAY_FE_FLOW] Step5_ArenaIntro: active=1 viewTarget=%s loc=%s"),
						*GetNameSafe(ViewTarget),
						ViewTarget ? *ViewTarget->GetActorLocation().ToCompactString() : TEXT("None"));
				}

				if (RoundComp->GetCurrentMatchPhase() == EBwayMatchPhase::Warmup)
				{
					if (!RoundComp->HasArenaIntroPlayed())
					{
						return Fail(TEXT("arena-intro-did-not-play-before-warmup"));
					}
					Advance(6);
				}
				return true;
			}

			// Step 6: Pre-Round Shop Overlay (W_MatchShop) during Warmup
			if (Step == 6)
			{
				ABwayPlayerController* PC = Cast<ABwayPlayerController>(RawPC);
				ABwayGameState* GS = World->GetGameState<ABwayGameState>();
				UBwayRoundManagementComponent* RoundComp = GS ? GS->GetRoundManagement() : nullptr;
				ABwayPlayerState* PS = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
				ABwayCharacterWithAbilities* HeroPawn = PC ? Cast<ABwayCharacterWithAbilities>(PC->GetPawn()) : nullptr;
				if (!PC || !RoundComp || !PS || !HeroPawn)
				{
					return true;
				}

				UBwayUpgradeShopWidget* ShopWidget = BwayFrontEndUI::FindActiveWidgetForPlayer<UBwayUpgradeShopWidget>(PC);
				if (!ShopWidget)
				{
					return Fail(TEXT("pre-round-shop-did-not-auto-open-in-warmup"));
				}

				const ULyraAbilitySystemComponent* PSASC = PS->GetLyraAbilitySystemComponent();
				const UBwayGoldAttributeSet* GoldSet = PSASC ? PSASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
				if (!GoldSet)
				{
					return Fail(TEXT("missing-gold-attribute-set"));
				}

				const int32 StartingGold = FMath::RoundToInt(GoldSet->GetCurrentGold());
				if (StartingGold != 1000)
				{
					return Fail(TEXT("pre-round-shop-starting-gold-not-1000"));
				}

				UBwayUpgradeComponent* UpgradeComp = PS->UpgradeComponent.Get();
				const UBwayHeroAttributeSet* HeroStats = HeroPawn->GetLyraAbilitySystemComponent()
					? HeroPawn->GetLyraAbilitySystemComponent()->GetSet<UBwayHeroAttributeSet>()
					: nullptr;
				if (!UpgradeComp || !HeroStats)
				{
					return Fail(TEXT("missing-upgrade-component-or-hero-attribute-set"));
				}

				BaseAttackStrength = HeroStats->GetAttackStrength();

				const UBwayUpgradeCatalog* Catalog = UpgradeComp->GetCatalog();
				const FBwayUpgradeDefinition* AttackDef = Catalog ? Catalog->FindUpgrade(FName("AttackStrength")) : nullptr;
				if (!AttackDef || AttackDef->Ranks.IsEmpty())
				{
					return Fail(TEXT("missing-attack-strength-catalog-definition"));
				}
				const int32 Tier1Cost = AttackDef->Ranks[0].Cost;
				const int32 ExpectedGoldAfterBuy = StartingGold - Tier1Cost;

				if (!UpgradeComp->TryPurchaseUpgrade(FName("AttackStrength")))
				{
					return Fail(TEXT("shop-buy-attack-strength-failed"));
				}
				if (FMath::RoundToInt(GoldSet->GetCurrentGold()) != ExpectedGoldAfterBuy || UpgradeComp->GetOwnedRank(FName("AttackStrength")) != 1)
				{
					return Fail(TEXT("shop-post-buy-state-mismatch"));
				}

				if (!UpgradeComp->TryUndoLastTransaction())
				{
					return Fail(TEXT("shop-undo-transaction-failed"));
				}
				if (FMath::RoundToInt(GoldSet->GetCurrentGold()) != StartingGold || UpgradeComp->GetOwnedRank(FName("AttackStrength")) != 0)
				{
					return Fail(TEXT("shop-post-undo-state-mismatch"));
				}

				if (!UpgradeComp->TryPurchaseUpgrade(FName("AttackStrength")))
				{
					return Fail(TEXT("shop-rebuy-attack-strength-failed"));
				}

				UpgradedAttackStrength = HeroStats->GetAttackStrength();
				if (UpgradedAttackStrength <= BaseAttackStrength)
				{
					return Fail(TEXT("shop-upgrade-did-not-increase-hero-attack-strength"));
				}

				UE_LOG(LogTemp, Display,
					TEXT("[BWAY_FE_FLOW] Step6_PreRoundShop: class=%s startGold=%d finalGold=%d tier=%d attackBefore=%.1f attackAfter=%.1f warmupRemaining=%d"),
					*ShopWidget->GetClass()->GetPathName(),
					StartingGold,
					FMath::RoundToInt(GoldSet->GetCurrentGold()),
					UpgradeComp->GetOwnedRank(FName("AttackStrength")),
					BaseAttackStrength,
					UpgradedAttackStrength,
					RoundComp->GetWarmupTimeRemaining());

				Advance(7);
				return true;
			}

			// Step 7: Round 1 Active Transition
			if (Step == 7)
			{
				ABwayPlayerController* PC = Cast<ABwayPlayerController>(RawPC);
				ABwayGameState* GS = World->GetGameState<ABwayGameState>();
				UBwayRoundManagementComponent* RoundComp = GS ? GS->GetRoundManagement() : nullptr;
				ABwayPlayerState* PS = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
				if (!PC || !RoundComp || !PS)
				{
					return true;
				}

				if (RoundComp->GetCurrentMatchPhase() != EBwayMatchPhase::Playing)
				{
					return true;
				}

				if (BwayFrontEndUI::FindActiveWidgetForPlayer<UBwayUpgradeShopWidget>(PC) != nullptr)
				{
					return Fail(TEXT("shop-did-not-auto-close-on-round-start"));
				}

				ABwayCharacterWithAbilities* HeroPawn = Cast<ABwayCharacterWithAbilities>(PC->GetPawn());
				if (!HeroPawn)
				{
					return Fail(TEXT("local-hero-pawn-missing-in-playing-phase"));
				}

				const FString PawnClassName = HeroPawn->GetClass()->GetName();
				if (!PawnClassName.Contains(TEXT("B_BW_Character_Humanoid")) || PawnClassName.Contains(TEXT("B_SimpleHeroPawn")))
				{
					return Fail(*FString::Printf(TEXT("wrong-pawn-class-spawned:%s"), *PawnClassName));
				}

				const ULyraExperienceManagerComponent* ExpComp = GS->FindComponentByClass<ULyraExperienceManagerComponent>();
				const ULyraExperienceDefinition* CurrentExp = (ExpComp && ExpComp->IsExperienceLoaded())
					? ExpComp->GetCurrentExperienceChecked()
					: nullptr;
				const FString ExpClassName = GetNameSafe(CurrentExp ? CurrentExp->GetClass() : nullptr);
				if (!ExpClassName.Contains(TEXT("B_BW_Experience_CaptureTheRelic")))
				{
					return Fail(*FString::Printf(TEXT("wrong-experience-loaded:%s"), *ExpClassName));
				}

				UCommonActivatableWidget* ActiveMatchHud = nullptr;
				for (TObjectIterator<UCommonActivatableWidget> It; It; ++It)
				{
					if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated()
						&& It->GetClass()->GetName().Contains(TEXT("WBP_BW_MatchHUDLayout")))
					{
						ActiveMatchHud = *It;
						break;
					}
				}
				if (!ActiveMatchHud)
				{
					return Fail(TEXT("match-hud-layout-not-active-on-game-layer"));
				}

				const UBwayHeroAttributeSet* HeroStats = HeroPawn->GetLyraAbilitySystemComponent()
					? HeroPawn->GetLyraAbilitySystemComponent()->GetSet<UBwayHeroAttributeSet>()
					: nullptr;
				const UBwayUpgradeComponent* UpgradeComp = PS->UpgradeComponent.Get();
				const ULyraAbilitySystemComponent* PSASC = PS->GetLyraAbilitySystemComponent();
				const UBwayGoldAttributeSet* GoldSet = PSASC ? PSASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
				if (!HeroStats || !UpgradeComp || !GoldSet)
				{
					return Fail(TEXT("playing-phase-missing-stats-or-upgrade-comp"));
				}

				const float ActiveAttackStrength = HeroStats->GetAttackStrength();
				if (UpgradeComp->GetOwnedRank(FName("AttackStrength")) != 1 || ActiveAttackStrength <= BaseAttackStrength)
				{
					return Fail(TEXT("playing-phase-upgrade-stat-not-preserved"));
				}

				int32 EliteBotCount = 0;
				for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
				{
					if (const ABwayRelicBotController* BotCtrl = Cast<ABwayRelicBotController>(It->Get()))
					{
						if (BotCtrl->GetBotDifficultyTier() == FName(TEXT("Elite")))
						{
							++EliteBotCount;
						}
					}
				}

				UE_LOG(LogTemp, Display,
					TEXT("[BWAY_FE_FLOW] Step7_RoundActive: phase=Playing exp=%s hud=%s shopClosed=1 pawn=%s pawnClass=%s hero=%s team=%d gold=%d upgradeTier=%d attackStrength=%.1f eliteBots=%d"),
					*ExpClassName,
					*GetNameSafe(ActiveMatchHud),
					*GetNameSafe(HeroPawn),
					*PawnClassName,
					*PS->GetSelectedHeroId().ToString(),
					GS->GetPlayerTeam(PS),
					FMath::RoundToInt(GoldSet->GetCurrentGold()),
					UpgradeComp->GetOwnedRank(FName("AttackStrength")),
					ActiveAttackStrength,
					EliteBotCount);

				UE_LOG(LogTemp, Display,
					TEXT("[BWAY_FE_FLOW] PASS: Complete Front-End to In-Match UI & State Flow verified!"));

				bFlowProbeRunning = false;
				if (bExitOnComplete && World->WorldType == EWorldType::Game)
				{
					FPlatformMisc::RequestExit(false);
				}
				return false;
			}

			return true;
		}
	};

	void StartFlowProbe(bool bExitOnComplete)
	{
		if (bFlowProbeRunning)
		{
			UE_LOG(LogTemp, Warning, TEXT("[BWAY_FE_FLOW] Already running"));
			return;
		}
		bFlowProbeRunning = true;
		TSharedRef<FFrontEndToMatchFlowProbe> Probe = MakeShared<FFrontEndToMatchFlowProbe>();
		Probe->bExitOnComplete = bExitOnComplete;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Delta)
		{
			return Probe->Tick(Delta);
		}), 0.1f);
	}

	FAutoConsoleCommand OpenMainMenuCmd(
		TEXT("bway.UI.OpenMainMenu"),
		TEXT("Pushes Breakaway W_MainMenu onto UI.Layer.Menu for the local player."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UWorld* World = BwayFrontEndUI::FindActiveRuntimeWorld())
			{
				if (APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController())
				{
					BwayFrontEndUI::PushBreakawayMainMenu(PC);
				}
			}
		}));

	FAutoConsoleCommand RunFrontEndFlowCmd(
		TEXT("bway.Test.FrontEndToMatchFlow"),
		TEXT("Executes and verifies the complete Breakaway Front-End to In-Match playable loop."),
		FConsoleCommandDelegate::CreateLambda([]() { StartFlowProbe(false); }));

	FAutoConsoleCommand RunFrontEndFlowExitCmd(
		TEXT("bway.Test.FrontEndToMatchFlowExit"),
		TEXT("Executes and verifies the complete Breakaway Front-End to In-Match playable loop and exits in -game mode."),
		FConsoleCommandDelegate::CreateLambda([]() { StartFlowProbe(true); }));
} // namespace BwayFrontEndFlowHarness
#endif
