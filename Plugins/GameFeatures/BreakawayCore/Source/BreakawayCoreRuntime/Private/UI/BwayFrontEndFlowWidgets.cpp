// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayFrontEndFlowWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "CommonSessionSubsystem.h"
#include "CommonUIExtensions.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/BwayExperienceLibrary.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AI/BwayRelicBotController.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Containers/Ticker.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
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
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "Misc/CommandLine.h"
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
	const FLinearColor DarkBackdrop(0.02f, 0.03f, 0.06f, 0.92f);
	const FLinearColor PanelBg(0.05f, 0.08f, 0.13f, 0.94f);
	const FLinearColor CardBg(0.07f, 0.11f, 0.18f, 0.92f);
	const FLinearColor SlotBg(0.04f, 0.07f, 0.12f, 0.88f);
	const FLinearColor MutedText(0.62f, 0.68f, 0.76f, 1.0f);

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

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 FontSize = 18, const FLinearColor& Color = FLinearColor::White)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetText(Text);
		Block->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = FontSize;
		Block->SetFont(Font);
		return Block;
	}

	UButton* MakeStyledButton(
		UWidgetTree* Tree,
		const FText& LabelText,
		int32 FontSize,
		const FLinearColor& BgColor,
		const FLinearColor& TextColor,
		UTextBlock** OutLabel = nullptr)
	{
		UButton* Btn = Tree->ConstructWidget<UButton>();
		Btn->SetBackgroundColor(BgColor);

		UBorder* PadBorder = Tree->ConstructWidget<UBorder>();
		PadBorder->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		PadBorder->SetPadding(FMargin(18.f, 10.f));

		UTextBlock* Label = MakeText(Tree, LabelText, FontSize, TextColor);
		PadBorder->SetContent(Label);
		Btn->AddChild(PadBorder);

		if (OutLabel)
		{
			*OutLabel = Label;
		}
		return Btn;
	}

	UButton* MakeStyledButton(
		UWidgetTree* Tree,
		const FText& LabelText,
		int32 FontSize,
		const FLinearColor& BgColor,
		const FLinearColor& TextColor,
		TObjectPtr<UTextBlock>* OutLabel)
	{
		UTextBlock* RawLabel = nullptr;
		UButton* Btn = MakeStyledButton(Tree, LabelText, FontSize, BgColor, TextColor, &RawLabel);
		if (OutLabel)
		{
			*OutLabel = RawLabel;
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

	UHorizontalBox* BuildPersistentPartyHeaderBar(UWidgetTree* Tree, const FString& PlayerName, UTextBlock** OutPlayerNameBlock = nullptr)
	{
		UHorizontalBox* HeaderRow = Tree->ConstructWidget<UHorizontalBox>();

		// Gold / Currency readout ("0")
		{
			UBorder* CurrencyBox = Tree->ConstructWidget<UBorder>();
			CurrencyBox->SetBrushColor(FLinearColor(0.04f, 0.06f, 0.10f, 0.90f));
			CurrencyBox->SetPadding(FMargin(14.f, 6.f));
			UTextBlock* CurrencyText = MakeText(Tree, NSLOCTEXT("BwayFE", "HeaderCurrency", "GOLD: 0"), 14, GoldBright);
			CurrencyBox->SetContent(CurrencyText);
			HeaderRow->AddChildToHorizontalBox(CurrencyBox)->SetPadding(FMargin(4.f, 0.f));
		}

		// Player Profile & Level ("1 <PlayerName>")
		{
			UBorder* ProfileBox = Tree->ConstructWidget<UBorder>();
			ProfileBox->SetBrushColor(FLinearColor(0.08f, 0.14f, 0.24f, 0.95f));
			ProfileBox->SetPadding(FMargin(16.f, 6.f));
			UTextBlock* ProfileText = MakeText(
				Tree,
				FText::FromString(FString::Printf(TEXT("LVL 1   %s"), *PlayerName)),
				14,
				FLinearColor::White);
			ProfileBox->SetContent(ProfileText);
			if (OutPlayerNameBlock)
			{
				*OutPlayerNameBlock = ProfileText;
			}
			HeaderRow->AddChildToHorizontalBox(ProfileBox)->SetPadding(FMargin(4.f, 0.f));
		}

		// 3 Party '+' slots matching Breakaway top-right bar
		for (int32 SlotIdx = 0; SlotIdx < 3; ++SlotIdx)
		{
			UBorder* PartySlot = Tree->ConstructWidget<UBorder>();
			PartySlot->SetBrushColor(FLinearColor(0.05f, 0.08f, 0.14f, 0.88f));
			PartySlot->SetPadding(FMargin(12.f, 6.f));
			UTextBlock* PlusText = MakeText(Tree, FText::FromString(TEXT("+")), 14, CyanAccent);
			PartySlot->SetContent(PlusText);
			HeaderRow->AddChildToHorizontalBox(PartySlot)->SetPadding(FMargin(2.f, 0.f));
		}

		return HeaderRow;
	}

	UHorizontalBox* BuildPersistentPartyHeaderBar(UWidgetTree* Tree, const FString& PlayerName, TObjectPtr<UTextBlock>* OutPlayerNameBlock)
	{
		UTextBlock* RawBlock = nullptr;
		UHorizontalBox* HeaderRow = BuildPersistentPartyHeaderBar(Tree, PlayerName, &RawBlock);
		if (OutPlayerNameBlock)
		{
			*OutPlayerNameBlock = RawBlock;
		}
		return HeaderRow;
	}
}

// ============================================================================
// UBwayMainMenuWidget (W_MainMenu)
// ============================================================================

UBwayMainMenuWidget::UBwayMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = false;
}

TOptional<FUIInputConfig> UBwayMainMenuWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

TSharedRef<SWidget> UBwayMainMenuWidget::RebuildWidget()
{
	if (UOverlay* Root = BwayFrontEndUI::EnsureRootOverlay(WidgetTree))
	{
		// Atmospheric dark-blue/gold backdrop
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(FLinearColor(0.02f, 0.04f, 0.08f, 0.78f));
		UOverlaySlot* BgSlot = Root->AddChildToOverlay(Backdrop);
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);

		// Top-Right Persistent Party/Profile Header Bar
		UHorizontalBox* PartyBar = BwayFrontEndUI::BuildPersistentPartyHeaderBar(WidgetTree, TEXT("Player1"), &ProfileNameText);
		UOverlaySlot* TopRightSlot = Root->AddChildToOverlay(PartyBar);
		TopRightSlot->SetHorizontalAlignment(HAlign_Right);
		TopRightSlot->SetVerticalAlignment(VAlign_Top);
		TopRightSlot->SetPadding(FMargin(0.f, 24.f, 36.f, 0.f));

		// Left-Aligned Vertical Navigation Column (Title + PLAY, WARRIORS, HELP, SETTINGS, QUIT)
		UVerticalBox* LeftNavColumn = WidgetTree->ConstructWidget<UVerticalBox>();
		UOverlaySlot* LeftSlot = Root->AddChildToOverlay(LeftNavColumn);
		LeftSlot->SetHorizontalAlignment(HAlign_Left);
		LeftSlot->SetVerticalAlignment(VAlign_Center);
		LeftSlot->SetPadding(FMargin(72.f, 0.f, 0.f, 0.f));

		UTextBlock* LogoTitle = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "BreakawayLogo", "BREAKAWAY"),
			46,
			BwayFrontEndUI::GoldBright);
		LeftNavColumn->AddChildToVerticalBox(LogoTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 36.f));

		auto AddNavButton = [this, LeftNavColumn](const FText& Label, int32 FontSize, const FLinearColor& Bg, const FLinearColor& Fg) -> UButton*
		{
			USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
			SizeBox->SetWidthOverride(280.f);
			UButton* Btn = BwayFrontEndUI::MakeStyledButton(WidgetTree, Label, FontSize, Bg, Fg);
			SizeBox->AddChild(Btn);
			LeftNavColumn->AddChildToVerticalBox(SizeBox)->SetPadding(FMargin(0.f, 6.f));
			return Btn;
		};

		PlayButton = AddNavButton(NSLOCTEXT("BwayFE", "Nav_Play", "PLAY"), 28, BwayFrontEndUI::GoldAccent, FLinearColor::White);
		WarriorsButton = AddNavButton(NSLOCTEXT("BwayFE", "Nav_Warriors", "WARRIORS"), 22, BwayFrontEndUI::PanelBg, BwayFrontEndUI::GoldBright);
		HelpButton = AddNavButton(NSLOCTEXT("BwayFE", "Nav_Help", "HELP"), 22, BwayFrontEndUI::PanelBg, BwayFrontEndUI::GoldBright);
		SettingsButton = AddNavButton(NSLOCTEXT("BwayFE", "Nav_Settings", "SETTINGS"), 22, BwayFrontEndUI::PanelBg, BwayFrontEndUI::GoldBright);
		QuitButton = AddNavButton(NSLOCTEXT("BwayFE", "Nav_Quit", "QUIT"), 22, BwayFrontEndUI::PanelBg, BwayFrontEndUI::MutedText);

		PlayButton->OnClicked.AddDynamic(this, &ThisClass::OnPlayButtonClicked);
		WarriorsButton->OnClicked.AddDynamic(this, &ThisClass::ClickWarriors);
		HelpButton->OnClicked.AddDynamic(this, &ThisClass::ClickHelp);
		SettingsButton->OnClicked.AddDynamic(this, &ThisClass::ClickSettings);
		QuitButton->OnClicked.AddDynamic(this, &ThisClass::ClickQuit);

		StatusBannerText = BwayFrontEndUI::MakeText(WidgetTree, FText::GetEmpty(), 15, BwayFrontEndUI::MutedText);
		LeftNavColumn->AddChildToVerticalBox(StatusBannerText)->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
	}
	return Super::RebuildWidget();
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
				ProfileNameText->SetText(FText::FromString(FString::Printf(TEXT("LVL 1   %s"), *FrontendState->GetResolvedLocalPlayerName())));
			}
		}
	}
}

UWidget* UBwayMainMenuWidget::NativeGetDesiredFocusTarget() const
{
	return PlayButton ? static_cast<UWidget*>(PlayButton) : Super::NativeGetDesiredFocusTarget();
}

void UBwayMainMenuWidget::OnPlayButtonClicked()
{
	ClickPlay();
}

UCommonActivatableWidget* UBwayMainMenuWidget::ClickPlay()
{
	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}
	const TSubclassOf<UCommonActivatableWidget> TargetClass = BwayFrontEndUI::ResolveScreenClass(
		ModeSelectionWidgetClass,
		TEXT("/BreakawayCore/UI/FrontEnd/W_ModeSelection.W_ModeSelection_C"),
		UBwayModeSelectionWidget::StaticClass());

	const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
	return UCommonUIExtensions::PushContentToLayer_ForPlayer(LP, MenuLayer, TargetClass);
}

void UBwayMainMenuWidget::ClickWarriors()
{
	if (StatusBannerText)
	{
		StatusBannerText->SetText(NSLOCTEXT("BwayFE", "WarriorsHint", "Select PLAY to choose your Warrior for battle."));
	}
}

void UBwayMainMenuWidget::ClickHelp()
{
	if (StatusBannerText)
	{
		StatusBannerText->SetText(NSLOCTEXT("BwayFE", "HelpHint", "Capture the Relic or eliminate the enemy team to win 3 rounds!"));
	}
}

void UBwayMainMenuWidget::ClickSettings()
{
	if (StatusBannerText)
	{
		StatusBannerText->SetText(NSLOCTEXT("BwayFE", "SettingsHint", "Match & Bot settings are available in the Custom Match Lobby."));
	}
}

void UBwayMainMenuWidget::ClickQuit()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}

// ============================================================================
// UBwayModeSelectionWidget (W_ModeSelection)
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

		// Top-Right Persistent Party/Profile Header Bar
		UHorizontalBox* PartyBar = BwayFrontEndUI::BuildPersistentPartyHeaderBar(WidgetTree, TEXT("Player1"));
		UOverlaySlot* TopRightSlot = Root->AddChildToOverlay(PartyBar);
		TopRightSlot->SetHorizontalAlignment(HAlign_Right);
		TopRightSlot->SetVerticalAlignment(VAlign_Top);
		TopRightSlot->SetPadding(FMargin(0.f, 24.f, 36.f, 0.f));

		// Center content: 5 horizontal angled/parallelogram mode cards + description + CANCEL
		UVerticalBox* CenterBox = WidgetTree->ConstructWidget<UVerticalBox>();
		UOverlaySlot* CenterSlot = Root->AddChildToOverlay(CenterBox);
		CenterSlot->SetHorizontalAlignment(HAlign_Center);
		CenterSlot->SetVerticalAlignment(VAlign_Center);

		UHorizontalBox* CardsRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		CenterBox->AddChildToVerticalBox(CardsRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));

		auto CreateModeCard = [this, CardsRow](const FText& Title, const FText& Subtitle, bool bHighlightGold) -> UButton*
		{
			USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
			CardSize->SetWidthOverride(240.f);
			CardSize->SetHeightOverride(410.f);

			// Slight parallelogram/angled shear matching Breakaway's mode cards
			FWidgetTransform ShearTransform;
			ShearTransform.Shear = FVector2D(-6.0f, 0.0f);
			CardSize->SetRenderTransform(ShearTransform);

			UButton* CardBtn = WidgetTree->ConstructWidget<UButton>();
			CardBtn->SetBackgroundColor(bHighlightGold ? BwayFrontEndUI::GoldAccent : BwayFrontEndUI::CardBg);

			UBorder* InnerBorder = WidgetTree->ConstructWidget<UBorder>();
			InnerBorder->SetBrushColor(FLinearColor(0.03f, 0.06f, 0.11f, 0.94f));
			InnerBorder->SetPadding(FMargin(16.f, 24.f));

			UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>();
			USpacer* TopSpacer = WidgetTree->ConstructWidget<USpacer>();
			TopSpacer->SetSize(FVector2D(1.f, 240.f));
			CardVBox->AddChildToVerticalBox(TopSpacer);

			UTextBlock* TitleBlock = BwayFrontEndUI::MakeText(WidgetTree, Title, 22, BwayFrontEndUI::GoldBright);
			TitleBlock->SetJustification(ETextJustify::Center);
			CardVBox->AddChildToVerticalBox(TitleBlock)->SetPadding(FMargin(0.f, 8.f, 0.f, 6.f));

			UTextBlock* SubBlock = BwayFrontEndUI::MakeText(WidgetTree, Subtitle, 12, BwayFrontEndUI::MutedText);
			SubBlock->SetJustification(ETextJustify::Center);
			SubBlock->SetAutoWrapText(true);
			CardVBox->AddChildToVerticalBox(SubBlock);

			InnerBorder->SetContent(CardVBox);
			CardBtn->AddChild(InnerBorder);
			CardSize->AddChild(CardBtn);

			CardsRow->AddChildToHorizontalBox(CardSize)->SetPadding(FMargin(10.f, 0.f));
			return CardBtn;
		};

		TutorialCardButton = CreateModeCard(
			NSLOCTEXT("BwayFE", "Mode_Tutorial", "TUTORIAL"),
			NSLOCTEXT("BwayFE", "Mode_Tutorial_Sub", "Learn the basics of Breakaway."),
			false);
		ArenaCardButton = CreateModeCard(
			NSLOCTEXT("BwayFE", "Mode_Arena", "ARENA"),
			NSLOCTEXT("BwayFE", "Mode_Arena_Sub", "Compete in 4v4 battles."),
			false);
		VersusAICardButton = CreateModeCard(
			NSLOCTEXT("BwayFE", "Mode_Coop", "VERSUS AI"),
			NSLOCTEXT("BwayFE", "Mode_Coop_Sub", "Battle against AI opponents."),
			false);
		CustomCardButton = CreateModeCard(
			NSLOCTEXT("BwayFE", "Mode_Custom", "CUSTOM"),
			NSLOCTEXT("BwayFE", "Mode_Custom_Sub", "Host or join custom games."),
			false);
		LocalCardButton = CreateModeCard(
			NSLOCTEXT("BwayFE", "Mode_Local", "LOCAL"),
			NSLOCTEXT("BwayFE", "Mode_Local_Sub", "Host a local server. For dev testing only."),
			true);

		TutorialCardButton->OnClicked.AddDynamic(this, &ThisClass::OnTutorialClicked);
		ArenaCardButton->OnClicked.AddDynamic(this, &ThisClass::OnArenaClicked);
		VersusAICardButton->OnClicked.AddDynamic(this, &ThisClass::OnVersusAIClicked);
		CustomCardButton->OnClicked.AddDynamic(this, &ThisClass::OnCustomClicked);
		LocalCardButton->OnClicked.AddDynamic(this, &ThisClass::OnLocalClicked);

		TutorialCardButton->OnHovered.AddDynamic(this, &ThisClass::OnTutorialHovered);
		ArenaCardButton->OnHovered.AddDynamic(this, &ThisClass::OnArenaHovered);
		VersusAICardButton->OnHovered.AddDynamic(this, &ThisClass::OnVersusAIHovered);
		CustomCardButton->OnHovered.AddDynamic(this, &ThisClass::OnCustomHovered);
		LocalCardButton->OnHovered.AddDynamic(this, &ThisClass::OnLocalHovered);

		// Hover description readout
		ModeDescriptionText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "LocalHoverDesc", "Host a local server. For dev testing only."),
			16,
			FLinearColor::White);
		ModeDescriptionText->SetJustification(ETextJustify::Center);
		CenterBox->AddChildToVerticalBox(ModeDescriptionText)->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));

		// Bottom CANCEL button
		USizeBox* CancelSize = WidgetTree->ConstructWidget<USizeBox>();
		CancelSize->SetWidthOverride(220.f);
		CancelButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree,
			NSLOCTEXT("BwayFE", "Btn_Cancel", "CANCEL"),
			18,
			BwayFrontEndUI::PanelBg,
			BwayFrontEndUI::GoldBright);
		CancelButton->OnClicked.AddDynamic(this, &ThisClass::ClickCancel);
		CancelSize->AddChild(CancelButton);

		UVerticalBoxSlot* CancelSlot = CenterBox->AddChildToVerticalBox(CancelSize);
		CancelSlot->SetHorizontalAlignment(HAlign_Center);
	}
	return Super::RebuildWidget();
}

void UBwayModeSelectionWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	SetHoveredMode(TEXT("LOCAL"));
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
	if (ActiveHoveredModeId == TEXT("TUTORIAL"))
	{
		return NSLOCTEXT("BwayFE", "Desc_Tutorial", "Learn the core mechanics of Relic handling, combat, and buildables.");
	}
	if (ActiveHoveredModeId == TEXT("ARENA"))
	{
		return NSLOCTEXT("BwayFE", "Desc_Arena", "Standard 4v4 matchmade Arena battle.");
	}
	if (ActiveHoveredModeId == TEXT("VERSUS_AI"))
	{
		return NSLOCTEXT("BwayFE", "Desc_VersusAI", "Co-op match against AI-controlled Warriors.");
	}
	if (ActiveHoveredModeId == TEXT("CUSTOM"))
	{
		return NSLOCTEXT("BwayFE", "Desc_Custom", "Create or join a custom match lobby.");
	}
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

void UBwayModeSelectionWidget::OnTutorialClicked() { SelectMode(TEXT("TUTORIAL")); }
void UBwayModeSelectionWidget::OnArenaClicked()    { SelectMode(TEXT("ARENA")); }
void UBwayModeSelectionWidget::OnVersusAIClicked() { SelectMode(TEXT("VERSUS_AI")); }
void UBwayModeSelectionWidget::OnCustomClicked()   { SelectMode(TEXT("CUSTOM")); }
void UBwayModeSelectionWidget::OnLocalClicked()    { SelectMode(TEXT("LOCAL")); }

void UBwayModeSelectionWidget::OnTutorialHovered() { SetHoveredMode(TEXT("TUTORIAL")); }
void UBwayModeSelectionWidget::OnArenaHovered()    { SetHoveredMode(TEXT("ARENA")); }
void UBwayModeSelectionWidget::OnVersusAIHovered() { SetHoveredMode(TEXT("VERSUS_AI")); }
void UBwayModeSelectionWidget::OnCustomHovered()   { SetHoveredMode(TEXT("CUSTOM")); }
void UBwayModeSelectionWidget::OnLocalHovered()    { SetHoveredMode(TEXT("LOCAL")); }

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
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(BwayFrontEndUI::DarkBackdrop);
		UOverlaySlot* BgSlot = Root->AddChildToOverlay(Backdrop);
		BgSlot->SetHorizontalAlignment(HAlign_Fill);
		BgSlot->SetVerticalAlignment(VAlign_Fill);

		// Top-Right Persistent Party/Profile Header Bar
		UHorizontalBox* PartyBar = BwayFrontEndUI::BuildPersistentPartyHeaderBar(WidgetTree, TEXT("Player1"));
		UOverlaySlot* TopRightSlot = Root->AddChildToOverlay(PartyBar);
		TopRightSlot->SetHorizontalAlignment(HAlign_Right);
		TopRightSlot->SetVerticalAlignment(VAlign_Top);
		TopRightSlot->SetPadding(FMargin(0.f, 24.f, 36.f, 0.f));

		// Central Server Browser Panel
		USizeBox* BrowserBoxSize = WidgetTree->ConstructWidget<USizeBox>();
		BrowserBoxSize->SetWidthOverride(920.f);
		BrowserBoxSize->SetHeightOverride(540.f);
		UOverlaySlot* CenterSlot = Root->AddChildToOverlay(BrowserBoxSize);
		CenterSlot->SetHorizontalAlignment(HAlign_Center);
		CenterSlot->SetVerticalAlignment(VAlign_Center);

		UBorder* BrowserPanel = WidgetTree->ConstructWidget<UBorder>();
		BrowserPanel->SetBrushColor(BwayFrontEndUI::PanelBg);
		BrowserPanel->SetPadding(FMargin(28.f, 24.f));
		BrowserBoxSize->AddChild(BrowserPanel);

		UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>();
		BrowserPanel->SetContent(MainVBox);

		// Header Row: Title + Manual REFRESH button
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		MainVBox->AddChildToVerticalBox(HeaderRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));

		UTextBlock* TitleText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "LocalBrowserTitle", "LOCAL SERVER BROWSER"),
			24,
			BwayFrontEndUI::GoldBright);
		UHorizontalBoxSlot* TitleSlot = HeaderRow->AddChildToHorizontalBox(TitleText);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);

		RefreshButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree,
			NSLOCTEXT("BwayFE", "Btn_Refresh", "REFRESH"),
			15,
			BwayFrontEndUI::CardBg,
			BwayFrontEndUI::CyanAccent);
		RefreshButton->OnClicked.AddDynamic(this, &ThisClass::RefreshServerList);
		HeaderRow->AddChildToHorizontalBox(RefreshButton);

		// Central Server List Area
		UBorder* ServerListArea = WidgetTree->ConstructWidget<UBorder>();
		ServerListArea->SetBrushColor(FLinearColor(0.02f, 0.04f, 0.07f, 0.90f));
		ServerListArea->SetPadding(FMargin(24.f, 32.f));
		UVerticalBoxSlot* ListSlot = MainVBox->AddChildToVerticalBox(ServerListArea);
		ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ListSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));

		UVerticalBox* ListContentVBox = WidgetTree->ConstructWidget<UVerticalBox>();
		ServerListArea->SetContent(ListContentVBox);

		ServerListStatusText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "RefreshingServers", "REFRESHING SERVER LIST..."),
			20,
			BwayFrontEndUI::MutedText);
		ServerListStatusText->SetJustification(ETextJustify::Center);
		ListContentVBox->AddChildToVerticalBox(ServerListStatusText)->SetPadding(FMargin(0.f, 80.f, 0.f, 16.f));

		ServerRowsBox = WidgetTree->ConstructWidget<UVerticalBox>();
		ListContentVBox->AddChildToVerticalBox(ServerRowsBox);

		// Bottom Action Bar: BACK and HOST MATCH
		UHorizontalBox* BottomRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		MainVBox->AddChildToVerticalBox(BottomRow);

		BackButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree,
			NSLOCTEXT("BwayFE", "Btn_Back", "BACK"),
			16,
			BwayFrontEndUI::CardBg,
			BwayFrontEndUI::MutedText);
		BackButton->OnClicked.AddDynamic(this, &ThisClass::ClickBack);
		BottomRow->AddChildToHorizontalBox(BackButton);

		USpacer* BottomFlex = WidgetTree->ConstructWidget<USpacer>();
		BottomRow->AddChildToHorizontalBox(BottomFlex)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		USizeBox* HostBtnSize = WidgetTree->ConstructWidget<USizeBox>();
		HostBtnSize->SetWidthOverride(260.f);
		HostMatchButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree,
			NSLOCTEXT("BwayFE", "Btn_HostMatch", "HOST MATCH"),
			20,
			BwayFrontEndUI::GoldAccent,
			FLinearColor::White);
		HostMatchButton->OnClicked.AddDynamic(this, &ThisClass::ClickHostMatch);
		HostBtnSize->AddChild(HostMatchButton);
		BottomRow->AddChildToHorizontalBox(HostBtnSize);

		// Fullscreen CREATING SERVER... Overlay (03_CreatingServer.png)
		CreatingServerOverlay = WidgetTree->ConstructWidget<UBorder>();
		CreatingServerOverlay->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.05f, 0.96f));
		CreatingServerOverlay->SetVisibility(ESlateVisibility::Collapsed);
		UOverlaySlot* CreatingSlot = Root->AddChildToOverlay(CreatingServerOverlay);
		CreatingSlot->SetHorizontalAlignment(HAlign_Fill);
		CreatingSlot->SetVerticalAlignment(VAlign_Fill);

		UVerticalBox* CreatingBox = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* CreatingText = BwayFrontEndUI::MakeText(
			WidgetTree,
			NSLOCTEXT("BwayFE", "CreatingServer", "CREATING SERVER..."),
			28,
			FLinearColor::White);
		CreatingText->SetJustification(ETextJustify::Center);
		CreatingBox->AddChildToVerticalBox(CreatingText);
		CreatingServerOverlay->SetContent(CreatingBox);
		CreatingServerOverlay->SetHorizontalAlignment(HAlign_Center);
		CreatingServerOverlay->SetVerticalAlignment(VAlign_Center);
	}
	return Super::RebuildWidget();
}

void UBwayLocalMatchBrowserWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	bIsCreatingServer = false;
	if (CreatingServerOverlay)
	{
		CreatingServerOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshServerList();
}

void UBwayLocalMatchBrowserWidget::NativeOnDeactivated()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
		World->GetTimerManager().ClearTimer(CreatingServerTimerHandle);
	}
	Super::NativeOnDeactivated();
}

UWidget* UBwayLocalMatchBrowserWidget::NativeGetDesiredFocusTarget() const
{
	return HostMatchButton ? static_cast<UWidget*>(HostMatchButton) : Super::NativeGetDesiredFocusTarget();
}

void UBwayLocalMatchBrowserWidget::RefreshServerList()
{
	DiscoveredSessions.Reset();
	if (ServerRowsBox)
	{
		ServerRowsBox->ClearChildren();
	}
	if (ServerListStatusText)
	{
		ServerListStatusText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ServerListStatusText->SetText(NSLOCTEXT("BwayFE", "RefreshingServers", "REFRESHING SERVER LIST..."));
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);

		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UCommonSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UCommonSessionSubsystem>())
			{
				ActiveSearchRequest = SessionSubsystem->CreateOnlineSearchSessionRequest();
				if (ActiveSearchRequest)
				{
					ActiveSearchRequest->bUseLobbies = false;
					ActiveSearchRequest->OnSearchFinished.AddUObject(this, &ThisClass::HandleSessionSearchFinished);
					SessionSubsystem->FindSessions(GetOwningPlayer(), ActiveSearchRequest);
				}
			}
		}

		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&ThisClass::FinishSimulatedRefresh,
			0.45f,
			false);
	}
}

void UBwayLocalMatchBrowserWidget::FinishSimulatedRefresh()
{
	if (DiscoveredSessions.IsEmpty() && ServerListStatusText)
	{
		ServerListStatusText->SetText(NSLOCTEXT("BwayFE", "NoGamesInProgress", "NO GAMES IN PROGRESS"));
	}
}

void UBwayLocalMatchBrowserWidget::HandleSessionSearchFinished(bool bSucceeded, const FText& ErrorMessage)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	DiscoveredSessions.Reset();
	if (bSucceeded && ActiveSearchRequest && !ActiveSearchRequest->Results.IsEmpty())
	{
		DiscoveredSessions = ActiveSearchRequest->Results;
		if (ServerListStatusText)
		{
			ServerListStatusText->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ServerRowsBox)
		{
			ServerRowsBox->ClearChildren();
			UCommonSession_SearchResult* FirstResult = DiscoveredSessions[0];
			FString Desc = FirstResult ? FirstResult->GetDescription() : TEXT("Local Breakaway Match");
			UButton* RowBtn = BwayFrontEndUI::MakeStyledButton(
				WidgetTree,
				FText::FromString(FString::Printf(TEXT("JOIN: %s"), *Desc)),
				16,
				BwayFrontEndUI::CardBg,
				BwayFrontEndUI::GoldBright);
			RowBtn->OnClicked.AddDynamic(this, &ThisClass::OnSessionRow0Clicked);
			ServerRowsBox->AddChildToVerticalBox(RowBtn)->SetPadding(FMargin(0.f, 4.f));
		}
	}
	else if (ServerListStatusText)
	{
		ServerListStatusText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ServerListStatusText->SetText(NSLOCTEXT("BwayFE", "NoGamesInProgress", "NO GAMES IN PROGRESS"));
	}
}

void UBwayLocalMatchBrowserWidget::OnSessionRow0Clicked()
{
	JoinDiscoveredSession(0);
}

void UBwayLocalMatchBrowserWidget::JoinDiscoveredSession(int32 SessionIndex)
{
	if (!DiscoveredSessions.IsValidIndex(SessionIndex))
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UCommonSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UCommonSessionSubsystem>())
			{
				SessionSubsystem->JoinSession(GetOwningPlayer(), DiscoveredSessions[SessionIndex]);
			}
		}
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
			0.40f,
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

		// Horizontal Map Carousel (< Map Card >)
		UHorizontalBox* CarouselRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* CarouselSlot = VBox->AddChildToVerticalBox(CarouselRow);
		CarouselSlot->SetHorizontalAlignment(HAlign_Center);
		CarouselSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

		PrevMapButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree, FText::FromString(TEXT("<")), 24, BwayFrontEndUI::CardBg, BwayFrontEndUI::CyanAccent);
		PrevMapButton->OnClicked.AddDynamic(this, &ThisClass::OnPrevMapClicked);
		CarouselRow->AddChildToHorizontalBox(PrevMapButton)->SetVerticalAlignment(VAlign_Center);

		USizeBox* MapCardSize = WidgetTree->ConstructWidget<USizeBox>();
		MapCardSize->SetWidthOverride(420.f);
		MapCardSize->SetHeightOverride(190.f);
		UBorder* MapCardBorder = WidgetTree->ConstructWidget<UBorder>();
		MapCardBorder->SetBrushColor(FLinearColor(0.10f, 0.16f, 0.24f, 0.96f));
		MapCardBorder->SetPadding(FMargin(20.f));

		UVerticalBox* MapCardVBox = WidgetTree->ConstructWidget<UVerticalBox>();
		MapNameText = BwayFrontEndUI::MakeText(
			WidgetTree, NSLOCTEXT("BwayFE", "DefaultMap", "The Shrine of Auros"), 24, BwayFrontEndUI::GoldBright);
		MapNameText->SetJustification(ETextJustify::Center);
		MapCardVBox->AddChildToVerticalBox(MapNameText)->SetPadding(FMargin(0.f, 36.f, 0.f, 10.f));

		MapDescText = BwayFrontEndUI::MakeText(
			WidgetTree, FText::GetEmpty(), 13, BwayFrontEndUI::MutedText);
		MapDescText->SetJustification(ETextJustify::Center);
		MapDescText->SetAutoWrapText(true);
		MapCardVBox->AddChildToVerticalBox(MapDescText);

		MapCardBorder->SetContent(MapCardVBox);
		MapCardSize->AddChild(MapCardBorder);
		CarouselRow->AddChildToHorizontalBox(MapCardSize)->SetPadding(FMargin(16.f, 0.f));

		NextMapButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree, FText::FromString(TEXT(">")), 24, BwayFrontEndUI::CardBg, BwayFrontEndUI::CyanAccent);
		NextMapButton->OnClicked.AddDynamic(this, &ThisClass::OnNextMapClicked);
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

			EnableBotsButton = BwayFrontEndUI::MakeStyledButton(
				WidgetTree,
				FText::FromString(TEXT("< TRUE >")),
				18,
				BwayFrontEndUI::CardBg,
				FLinearColor::White,
				&EnableBotsValueText);
			EnableBotsButton->OnClicked.AddDynamic(this, &ThisClass::OnToggleBotsClicked);
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

			BotDifficultyButton = BwayFrontEndUI::MakeStyledButton(
				WidgetTree,
				FText::FromString(TEXT("< NORMAL >")),
				18,
				BwayFrontEndUI::CardBg,
				FLinearColor::White,
				&BotDifficultyValueText);
			BotDifficultyButton->OnClicked.AddDynamic(this, &ThisClass::OnCycleDifficultyClicked);
			DiffRow->AddChildToHorizontalBox(BotDifficultyButton);
		}

		// ACCEPT and CANCEL buttons
		UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* ActionSlot = VBox->AddChildToVerticalBox(ActionRow);
		ActionSlot->SetHorizontalAlignment(HAlign_Center);

		AcceptButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree, NSLOCTEXT("BwayFE", "Btn_Accept", "ACCEPT"), 18, BwayFrontEndUI::GoldAccent, FLinearColor::White);
		AcceptButton->OnClicked.AddDynamic(this, &ThisClass::AcceptSettings);
		ActionRow->AddChildToHorizontalBox(AcceptButton)->SetPadding(FMargin(12.f, 0.f));

		CancelButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree, NSLOCTEXT("BwayFE", "Btn_Cancel", "CANCEL"), 18, BwayFrontEndUI::CardBg, BwayFrontEndUI::MutedText);
		CancelButton->OnClicked.AddDynamic(this, &ThisClass::CancelSettings);
		ActionRow->AddChildToHorizontalBox(CancelButton)->SetPadding(FMargin(12.f, 0.f));
	}
	return Super::RebuildWidget();
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
	if (EnableBotsValueText)
	{
		EnableBotsValueText->SetText(FText::FromString(bWorkingBotsEnabled ? TEXT("<  TRUE  >") : TEXT("<  FALSE  >")));
	}
	if (BotDifficultyValueText)
	{
		const FText DiffText = UBwayFrontendStateSubsystem::GetBotDifficultyDisplayText(WorkingBotDifficulty);
		BotDifficultyValueText->SetText(FText::Format(NSLOCTEXT("BwayFE", "DiffFmt", "<  {0}  >"), DiffText));
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

void UBwayMatchSettingsModalWidget::OnPrevMapClicked()         { CycleMap(-1); }
void UBwayMatchSettingsModalWidget::OnNextMapClicked()         { CycleMap(1); }
void UBwayMatchSettingsModalWidget::OnToggleBotsClicked()      { ToggleEnableBots(); }
void UBwayMatchSettingsModalWidget::OnCycleDifficultyClicked() { CycleBotDifficulty(1); }

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

		// Top-Right Persistent Party/Profile Header Bar
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

		// Top Section: Host Controls + Map Banner
		UHorizontalBox* TopBannerRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		MainLayout->AddChildToVerticalBox(TopBannerRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));

		// Left Host Controls Box
		UBorder* HostControlsPanel = WidgetTree->ConstructWidget<UBorder>();
		HostControlsPanel->SetBrushColor(BwayFrontEndUI::PanelBg);
		HostControlsPanel->SetPadding(FMargin(18.f, 16.f));
		TopBannerRow->AddChildToHorizontalBox(HostControlsPanel)->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));

		UVerticalBox* HostControlsVBox = WidgetTree->ConstructWidget<UVerticalBox>();
		HostControlsPanel->SetContent(HostControlsVBox);

		MatchOptionsButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree, NSLOCTEXT("BwayFE", "Btn_MatchOptions", "MATCH OPTIONS"), 15, BwayFrontEndUI::CardBg, BwayFrontEndUI::CyanAccent);
		MatchOptionsButton->OnClicked.AddDynamic(this, &ThisClass::OnMatchOptionsClicked);
		HostControlsVBox->AddChildToVerticalBox(MatchOptionsButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

		InvitesSelectorButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree,
			NSLOCTEXT("BwayFE", "Btn_InvitesHostOnly", "< INVITES (HOST ONLY) >"),
			14,
			BwayFrontEndUI::CardBg,
			BwayFrontEndUI::CyanAccent,
			&InvitesButtonLabel);
		InvitesSelectorButton->OnClicked.AddDynamic(this, &ThisClass::ClickCycleInviteMode);
		HostControlsVBox->AddChildToVerticalBox(InvitesSelectorButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

		InvitePlayerButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree, NSLOCTEXT("BwayFE", "Btn_InvitePlayer", "INVITE PLAYER"), 14, BwayFrontEndUI::CardBg, BwayFrontEndUI::MutedText);
		InvitePlayerButton->OnClicked.AddDynamic(this, &ThisClass::ClickInvitePlayer);
		HostControlsVBox->AddChildToVerticalBox(InvitePlayerButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

		// Read-only summary block
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

		UTextBlock* CustomMatchSub = BwayFrontEndUI::MakeText(
			WidgetTree, NSLOCTEXT("BwayFE", "CustomMatchSub", "CUSTOM MATCH:"), 16, BwayFrontEndUI::GoldAccent);
		BannerVBox->AddChildToVerticalBox(CustomMatchSub)->SetPadding(FMargin(0.f, 12.f, 0.f, 6.f));

		MapHeaderText = BwayFrontEndUI::MakeText(
			WidgetTree, NSLOCTEXT("BwayFE", "ShrineHeader", "THE SHRINE OF AUROS"), 38, FLinearColor::White);
		BannerVBox->AddChildToVerticalBox(MapHeaderText);

		// Middle Section: 3-Column Team Roster Grid (BLUE TEAM, RED TEAM, SPECTATORS)
		UHorizontalBox* RosterColumnsRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* RosterSlot = MainLayout->AddChildToVerticalBox(RosterColumnsRow);
		RosterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RosterSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));

		BlueSlotButtons.Reset();
		BlueSlotLabels.Reset();
		RedSlotButtons.Reset();
		RedSlotLabels.Reset();
		SpectatorSlotButtons.Reset();
		SpectatorSlotLabels.Reset();

		auto BuildTeamColumn = [this, RosterColumnsRow](
			const FText& HeaderTitle,
			const FLinearColor& HeaderColor,
			TArray<TObjectPtr<UButton>>& OutButtons,
			TArray<TObjectPtr<UTextBlock>>& OutLabels)
		{
			UVerticalBox* ColBox = WidgetTree->ConstructWidget<UVerticalBox>();
			UHorizontalBoxSlot* ColSlot = RosterColumnsRow->AddChildToHorizontalBox(ColBox);
			ColSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ColSlot->SetPadding(FMargin(8.f, 0.f));

			UBorder* HeaderBar = WidgetTree->ConstructWidget<UBorder>();
			HeaderBar->SetBrushColor(HeaderColor);
			HeaderBar->SetPadding(FMargin(16.f, 10.f));
			UTextBlock* HeaderText = BwayFrontEndUI::MakeText(WidgetTree, HeaderTitle, 18, FLinearColor::White);
			HeaderBar->SetContent(HeaderText);
			ColBox->AddChildToVerticalBox(HeaderBar)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

			for (int32 i = 0; i < 4; ++i)
			{
				UTextBlock* SlotLabel = nullptr;
				UButton* SlotBtn = BwayFrontEndUI::MakeStyledButton(
					WidgetTree,
					NSLOCTEXT("BwayFE", "ClickToJoin", "click to join"),
					15,
					BwayFrontEndUI::SlotBg,
					BwayFrontEndUI::MutedText,
					&SlotLabel);
				ColBox->AddChildToVerticalBox(SlotBtn)->SetPadding(FMargin(0.f, 4.f));
				OutButtons.Add(SlotBtn);
				OutLabels.Add(SlotLabel);
			}
		};

		BuildTeamColumn(NSLOCTEXT("BwayFE", "Col_BlueTeam", "BLUE TEAM"), BwayFrontEndUI::BlueTeamHeader, BlueSlotButtons, BlueSlotLabels);
		BuildTeamColumn(NSLOCTEXT("BwayFE", "Col_RedTeam", "RED TEAM"), BwayFrontEndUI::RedTeamHeader, RedSlotButtons, RedSlotLabels);
		BuildTeamColumn(NSLOCTEXT("BwayFE", "Col_Spectators", "SPECTATORS"), BwayFrontEndUI::SpecTeamHeader, SpectatorSlotButtons, SpectatorSlotLabels);

		if (BlueSlotButtons.Num() == 4)
		{
			BlueSlotButtons[0]->OnClicked.AddDynamic(this, &ThisClass::OnBlueSlot0Clicked);
			BlueSlotButtons[1]->OnClicked.AddDynamic(this, &ThisClass::OnBlueSlot1Clicked);
			BlueSlotButtons[2]->OnClicked.AddDynamic(this, &ThisClass::OnBlueSlot2Clicked);
			BlueSlotButtons[3]->OnClicked.AddDynamic(this, &ThisClass::OnBlueSlot3Clicked);
		}
		if (RedSlotButtons.Num() == 4)
		{
			RedSlotButtons[0]->OnClicked.AddDynamic(this, &ThisClass::OnRedSlot0Clicked);
			RedSlotButtons[1]->OnClicked.AddDynamic(this, &ThisClass::OnRedSlot1Clicked);
			RedSlotButtons[2]->OnClicked.AddDynamic(this, &ThisClass::OnRedSlot2Clicked);
			RedSlotButtons[3]->OnClicked.AddDynamic(this, &ThisClass::OnRedSlot3Clicked);
		}
		if (SpectatorSlotButtons.Num() == 4)
		{
			SpectatorSlotButtons[0]->OnClicked.AddDynamic(this, &ThisClass::OnSpecSlot0Clicked);
			SpectatorSlotButtons[1]->OnClicked.AddDynamic(this, &ThisClass::OnSpecSlot1Clicked);
			SpectatorSlotButtons[2]->OnClicked.AddDynamic(this, &ThisClass::OnSpecSlot2Clicked);
			SpectatorSlotButtons[3]->OnClicked.AddDynamic(this, &ThisClass::OnSpecSlot3Clicked);
		}

		// Bottom Bar: Leave, Settings, and bottom-right START
		UHorizontalBox* BottomBar = WidgetTree->ConstructWidget<UHorizontalBox>();
		MainLayout->AddChildToVerticalBox(BottomBar);

		LeaveButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree, NSLOCTEXT("BwayFE", "Btn_Leave", "Leave"), 16, BwayFrontEndUI::CardBg, FLinearColor::White);
		LeaveButton->OnClicked.AddDynamic(this, &ThisClass::ClickLeaveLobby);
		BottomBar->AddChildToHorizontalBox(LeaveButton)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));

		SettingsButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree, NSLOCTEXT("BwayFE", "Btn_Settings", "Settings"), 16, BwayFrontEndUI::CardBg, FLinearColor::White);
		SettingsButton->OnClicked.AddDynamic(this, &ThisClass::OnMatchOptionsClicked);
		BottomBar->AddChildToHorizontalBox(SettingsButton);

		USpacer* FlexSpacer = WidgetTree->ConstructWidget<USpacer>();
		BottomBar->AddChildToHorizontalBox(FlexSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		USizeBox* StartSize = WidgetTree->ConstructWidget<USizeBox>();
		StartSize->SetWidthOverride(280.f);
		StartButton = BwayFrontEndUI::MakeStyledButton(
			WidgetTree, NSLOCTEXT("BwayFE", "Btn_Start", "START"), 24, BwayFrontEndUI::GoldAccent, FLinearColor::White);
		StartButton->OnClicked.AddDynamic(this, &ThisClass::ClickStartMatch);
		StartSize->AddChild(StartButton);
		BottomBar->AddChildToHorizontalBox(StartSize);
	}
	return Super::RebuildWidget();
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

	if (MapHeaderText)
	{
		MapHeaderText->SetText(FText::FromString(MapEntry.DisplayName.ToString().ToUpper()));
	}

	const FText DiffText = UBwayFrontendStateSubsystem::GetBotDifficultyDisplayText(Settings.BotDifficulty);
	const FText InviteText = UBwayFrontendStateSubsystem::GetInviteModeDisplayText(Settings.InviteMode);

	if (InvitesButtonLabel)
	{
		InvitesButtonLabel->SetText(FText::Format(NSLOCTEXT("BwayFE", "InvitesBtnFmt", "< INVITES ({0}) >"), InviteText));
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

	auto UpdateColumnLabels = [](const TArray<FString>& SlotsData, TArray<TObjectPtr<UTextBlock>>& Labels)
	{
		for (int32 i = 0; i < Labels.Num(); ++i)
		{
			if (!Labels[i]) continue;
			const bool bOccupied = SlotsData.IsValidIndex(i) && !SlotsData[i].IsEmpty();
			Labels[i]->SetText(bOccupied
				? FText::FromString(SlotsData[i])
				: NSLOCTEXT("BwayFE", "ClickToJoin", "click to join"));
			Labels[i]->SetColorAndOpacity(FSlateColor(bOccupied ? FLinearColor::White : BwayFrontEndUI::MutedText));
		}
	};

	UpdateColumnLabels(Settings.BlueTeamSlots, BlueSlotLabels);
	UpdateColumnLabels(Settings.RedTeamSlots, RedSlotLabels);
	UpdateColumnLabels(Settings.SpectatorSlots, SpectatorSlotLabels);
}

void UBwayMatchLobbyWidget::OnMatchOptionsClicked()
{
	OpenMatchOptions();
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
	// Local session placeholder hook
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

void UBwayMatchLobbyWidget::ClickStartMatch()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
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
	if (Subsystem)
	{
		// Clear any stale hero lock before entering the new match's Hero Selection screen
		Subsystem->SetSelectedHeroId(FPrimaryAssetId());
	}

	const FBwayMapOptionEntry MapEntry = Subsystem ? Subsystem->GetSelectedMapEntry() : FBwayMapOptionEntry();
	const FName MapPackageName(*MapEntry.MapPackagePath);
	const FString ExpName = MapEntry.ExperienceAssetId.IsValid()
		? MapEntry.ExperienceAssetId.PrimaryAssetName.ToString()
		: TEXT("B_BW_Experience_CaptureTheRelic");

	FString Options = FString::Printf(TEXT("Experience=%s?listen"), *ExpName);
	if (Subsystem)
	{
		TMap<FString, FString> ExtraArgs;
		Subsystem->BuildMatchExtraArgs(ExtraArgs);
		for (const TPair<FString, FString>& Pair : ExtraArgs)
		{
			Options += FString::Printf(TEXT("?%s=%s"), *Pair.Key, *Pair.Value);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BwayMatchLobbyWidget: Starting match on %s with Options='%s'"),
		*MapPackageName.ToString(), *Options);

	UGameplayStatics::OpenLevel(World, MapPackageName, true, Options);
}

void UBwayMatchLobbyWidget::OnBlueSlot0Clicked() { SelectTeamSlot(EBwayLobbyTeam::BlueTeam, 0); }
void UBwayMatchLobbyWidget::OnBlueSlot1Clicked() { SelectTeamSlot(EBwayLobbyTeam::BlueTeam, 1); }
void UBwayMatchLobbyWidget::OnBlueSlot2Clicked() { SelectTeamSlot(EBwayLobbyTeam::BlueTeam, 2); }
void UBwayMatchLobbyWidget::OnBlueSlot3Clicked() { SelectTeamSlot(EBwayLobbyTeam::BlueTeam, 3); }

void UBwayMatchLobbyWidget::OnRedSlot0Clicked()  { SelectTeamSlot(EBwayLobbyTeam::RedTeam, 0); }
void UBwayMatchLobbyWidget::OnRedSlot1Clicked()  { SelectTeamSlot(EBwayLobbyTeam::RedTeam, 1); }
void UBwayMatchLobbyWidget::OnRedSlot2Clicked()  { SelectTeamSlot(EBwayLobbyTeam::RedTeam, 2); }
void UBwayMatchLobbyWidget::OnRedSlot3Clicked()  { SelectTeamSlot(EBwayLobbyTeam::RedTeam, 3); }

void UBwayMatchLobbyWidget::OnSpecSlot0Clicked() { SelectTeamSlot(EBwayLobbyTeam::Spectators, 0); }
void UBwayMatchLobbyWidget::OnSpecSlot1Clicked() { SelectTeamSlot(EBwayLobbyTeam::Spectators, 1); }
void UBwayMatchLobbyWidget::OnSpecSlot2Clicked() { SelectTeamSlot(EBwayLobbyTeam::Spectators, 2); }
void UBwayMatchLobbyWidget::OnSpecSlot3Clicked() { SelectTeamSlot(EBwayLobbyTeam::Spectators, 3); }

#if !UE_BUILD_SHIPPING
namespace BwayFrontEndFlowHarness
{
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
		const TSubclassOf<UCommonActivatableWidget> MainMenuClass = BwayFrontEndUI::ResolveScreenClass(
			TSoftClassPtr<UCommonActivatableWidget>(),
			TEXT("/BreakawayCore/UI/FrontEnd/W_MainMenu.W_MainMenu_C"),
			UBwayMainMenuWidget::StaticClass());
		const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
		return UCommonUIExtensions::PushContentToLayer_ForPlayer(PC->GetLocalPlayer(), MenuLayer, MainMenuClass);
	}

	// Auto-pushes W_MainMenu onto UI.Layer.Menu when L_LyraFrontEnd finishes showing W_LyraFrontEnd_C,
	// unless a legacy LANBrowser / SelectionEdges / Goal4 probe command is active on the command line.
	struct FAutoFrontEndMenuHook
	{
		TWeakObjectPtr<UCommonActivatableWidget> LastWrappedLyraFrontEnd;

		FAutoFrontEndMenuHook()
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FAutoFrontEndMenuHook::Tick), 0.1f);
		}

		bool Tick(float)
		{
			const FString CmdLine = FCommandLine::Get();
			if (CmdLine.Contains(TEXT("bway.Test.LANBrowser"))
				|| CmdLine.Contains(TEXT("bway.Test.SelectionEdges"))
				|| CmdLine.Contains(TEXT("bway.Test.Goal4")))
			{
				return true;
			}

			UWorld* World = FindActiveRuntimeWorld();
			if (!World || !World->GetMapName().Contains(TEXT("L_LyraFrontEnd")))
			{
				return true;
			}

			APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();
			if (!PC || !PC->GetLocalPlayer())
			{
				return true;
			}

			for (TObjectIterator<UCommonActivatableWidget> It; It; ++It)
			{
				if (!It->IsTemplate() && It->GetOwningPlayer() == PC && It->IsActivated()
					&& It->GetClass()->GetName() == TEXT("W_LyraFrontEnd_C"))
				{
					if (LastWrappedLyraFrontEnd.Get() != *It)
					{
						LastWrappedLyraFrontEnd = *It;
						UCommonActivatableWidget* Pushed = PushBreakawayMainMenu(PC);
						UE_LOG(LogTemp, Display, TEXT("[BWAY_FE_FLOW] AutoPushedMainMenu map=%s widget=%s"),
							*World->GetMapName(), *GetNameSafe(Pushed));
					}
					break;
				}
			}
			return true;
		}
	};

	static FAutoFrontEndMenuHook GAutoFrontEndMenuHook;

	bool bFlowProbeRunning = false;

	struct FFrontEndToMatchFlowProbe
	{
		bool bExitOnComplete = false;
		int32 Step = 0;
		double StartedAt = FPlatformTime::Seconds();
		double StepChangedAt = StartedAt;
		bool bTriggeredHostMatch = false;
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

			UWorld* World = FindActiveRuntimeWorld();
			if (!World)
			{
				return true;
			}

			APlayerController* RawPC = World->GetGameInstance()->GetFirstLocalPlayerController();
			if (!RawPC || !RawPC->GetLocalPlayer())
			{
				return true;
			}

			// Step 0: Main Menu (W_MainMenu) -> ClickPlay -> W_ModeSelection
			if (Step == 0)
			{
				UBwayMainMenuWidget* MainMenu = FindActiveWidgetForPlayer<UBwayMainMenuWidget>(RawPC);
				if (!MainMenu || !MainMenu->IsActivated() || !GAutoFrontEndMenuHook.LastWrappedLyraFrontEnd.IsValid())
				{
					return true;
				}

				UE_LOG(LogTemp, Display, TEXT("[BWAY_FE_FLOW] Step0_MainMenu: class=%s active=1"),
					*MainMenu->GetClass()->GetPathName());

				UCommonActivatableWidget* ModeSelect = MainMenu->ClickPlay();
				if (!ModeSelect)
				{
					return Fail(TEXT("main-menu-click-play-failed"));
				}
				Advance(1);
				return true;
			}

			// Step 1: Mode Selection (W_ModeSelection) -> Hover LOCAL, Test Cancel/Play, Click LOCAL -> W_LocalMatchBrowser
			if (Step == 1)
			{
				UBwayModeSelectionWidget* ModeSelect = FindActiveWidgetForPlayer<UBwayModeSelectionWidget>(RawPC);
				if (!ModeSelect)
				{
					return true;
				}

				ModeSelect->SetHoveredMode(TEXT("LOCAL"));
				const FString HoverDesc = ModeSelect->GetCurrentModeDescription().ToString();
				if (!HoverDesc.Contains(TEXT("Host a local server. For dev testing only.")))
				{
					return Fail(TEXT("mode-select-local-hover-desc-mismatch"));
				}

				// Test CANCEL button popping back to W_MainMenu, then re-open ModeSelection
				ModeSelect->ClickCancel();
				UBwayMainMenuWidget* MainMenuAfterCancel = FindActiveWidgetForPlayer<UBwayMainMenuWidget>(RawPC);
				if (!MainMenuAfterCancel || !MainMenuAfterCancel->IsActivated())
				{
					return Fail(TEXT("mode-select-cancel-did-not-restore-main-menu"));
				}

				ModeSelect = Cast<UBwayModeSelectionWidget>(MainMenuAfterCancel->ClickPlay());
				if (!ModeSelect)
				{
					return Fail(TEXT("main-menu-reopen-mode-select-failed"));
				}

				UE_LOG(LogTemp, Display, TEXT("[BWAY_FE_FLOW] Step1_ModeSelection: class=%s localHoverDesc='%s' cancelRoundTrip=1"),
					*ModeSelect->GetClass()->GetPathName(), *HoverDesc);

				UCommonActivatableWidget* Browser = ModeSelect->ClickLocalMode();
				if (!Browser)
				{
					return Fail(TEXT("mode-select-click-local-failed"));
				}
				bTriggeredHostMatch = false;
				Advance(2);
				return true;
			}

			// Step 2: Local Match Browser (W_LocalMatchBrowser) -> Refresh -> ClickHostMatch -> CREATING SERVER... -> W_MatchLobby
			if (Step == 2)
			{
				if (UBwayMatchLobbyWidget* ActiveLobby = FindActiveWidgetForPlayer<UBwayMatchLobbyWidget>(RawPC))
				{
					Advance(3);
					return true;
				}

				UBwayLocalMatchBrowserWidget* Browser = FindActiveWidgetForPlayer<UBwayLocalMatchBrowserWidget>(RawPC);
				if (!Browser)
				{
					return true;
				}

				if (!bTriggeredHostMatch)
				{
					if (Now - StepChangedAt < 0.45)
					{
						return true;
					}
					Browser->ClickHostMatch();
					if (!Browser->IsCreatingServer())
					{
						return Fail(TEXT("browser-creating-server-overlay-not-active"));
					}
					bTriggeredHostMatch = true;
					UE_LOG(LogTemp, Display, TEXT("[BWAY_FE_FLOW] Step2_LocalMatchBrowser: class=%s creatingServerOverlay=1"),
						*Browser->GetClass()->GetPathName());
				}
				return true;
			}

			// Step 3: Custom Match Lobby (W_MatchLobby) & Match Settings Modal (W_MatchSettingsModal) -> Start Match
			if (Step == 3)
			{
				UBwayMatchLobbyWidget* Lobby = FindActiveWidgetForPlayer<UBwayMatchLobbyWidget>(RawPC);
				if (!Lobby)
				{
					return true;
				}

				UBwayFrontendStateSubsystem* Subsystem = RawPC->GetLocalPlayer()->GetSubsystem<UBwayFrontendStateSubsystem>();
				if (!Subsystem)
				{
					return Fail(TEXT("missing-frontend-state-subsystem"));
				}

				// Verify default host placement in Blue Team Slot 1
				const FBwayMatchLobbySettings& InitialSettings = Subsystem->GetLobbySettings();
				if (InitialSettings.BlueTeamSlots.IsEmpty() || !InitialSettings.BlueTeamSlots[0].Contains(TEXT("(Host)")))
				{
					return Fail(TEXT("lobby-initial-blue-slot-0-missing-host"));
				}

				// Open Match Settings Modal (W_MatchSettingsModal) on UI.Layer.Modal
				UBwayMatchSettingsModalWidget* Modal = Cast<UBwayMatchSettingsModalWidget>(Lobby->OpenMatchOptions());
				if (!Modal || !Modal->IsActivated())
				{
					return Fail(TEXT("lobby-open-match-settings-modal-failed"));
				}

				// Exercise map carousel arrows and return to The Shrine of Auros (index 0)
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

				// Exercise team/slot switching ("click to join") and return to Blue Slot 1
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

			// Step 4: Hero Selection Screen (W_HeroSelection) on L_BW_Dorado
			if (Step == 4)
			{
				if (!World->GetMapName().Contains(TEXT("L_BW_Dorado")))
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

				UBwayHeroSelectWidget* HeroSelect = FindActiveWidgetForPlayer<UBwayHeroSelectWidget>(PC);
				if (!HeroSelect || Now - StepChangedAt < 0.5)
				{
					return true;
				}

				const TArray<FHeroDisplayInfo> Heroes = HeroSelect->GetAvailableHeroes();
				if (Heroes.IsEmpty())
				{
					return Fail(TEXT("hero-select-no-heroes-in-registry"));
				}

				// Prefer Rawlins (matching 06_HeroSelect.png reference) or first available hero
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

				HeroSelect->ToggleAbilitiesPanel();
				const bool bAbilitiesShown = true;

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

				int32 BotsLockedCount = 0;
				for (APlayerState* MemberPS : GS->PlayerArray)
				{
					const ABwayPlayerState* BwayMember = Cast<ABwayPlayerState>(MemberPS);
					if (BwayMember && BwayMember != PS && BwayMember->IsHeroLocked() && BwayMember->GetSelectedHeroId().IsValid())
					{
						++BotsLockedCount;
					}
				}

				UE_LOG(LogTemp, Display,
					TEXT("[BWAY_FE_FLOW] Step4_HeroSelection: class=%s hero=%s previewActor=%s abilitiesToggled=%d timerRemaining=%.1f locked=1 botsLocked=%d"),
					*HeroSelectClassPath,
					*LockedHeroId.ToString(),
					*PreviewActorName,
					bAbilitiesShown ? 1 : 0,
					CountdownRemaining,
					BotsLockedCount);

				bObservedArenaIntro = false;
				Advance(5);
				return true;
			}

			// Step 5: Arena Intro Fly-Through Sequence (5.5s camera pan -> Warmup)
			if (Step == 5)
			{
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

			// Step 6: Pre-Round Shop Overlay (W_MatchShop) during 15s Warmup
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

				UBwayUpgradeShopWidget* ShopWidget = FindActiveWidgetForPlayer<UBwayUpgradeShopWidget>(PC);
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

				// Buy AttackStrength Tier I
				if (!UpgradeComp->TryPurchaseUpgrade(FName("AttackStrength")))
				{
					return Fail(TEXT("shop-buy-attack-strength-failed"));
				}
				if (FMath::RoundToInt(GoldSet->GetCurrentGold()) != ExpectedGoldAfterBuy || UpgradeComp->GetOwnedRank(FName("AttackStrength")) != 1)
				{
					return Fail(TEXT("shop-post-buy-state-mismatch"));
				}

				// Test UNDO transaction -> restores 1000 gold and Tier 0
				if (!UpgradeComp->TryUndoLastTransaction())
				{
					return Fail(TEXT("shop-undo-transaction-failed"));
				}
				if (FMath::RoundToInt(GoldSet->GetCurrentGold()) != StartingGold || UpgradeComp->GetOwnedRank(FName("AttackStrength")) != 0)
				{
					return Fail(TEXT("shop-post-undo-state-mismatch"));
				}

				// Re-purchase AttackStrength Tier I so it remains active for Round 1
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

			// Step 7: Round 1 Active Transition (Shop auto-closes at 0s -> Hero spawned with upgrade & HUD active)
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

				if (FindActiveWidgetForPlayer<UBwayUpgradeShopWidget>(PC) != nullptr)
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
			if (UWorld* World = FindActiveRuntimeWorld())
			{
				if (APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController())
				{
					PushBreakawayMainMenu(PC);
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
