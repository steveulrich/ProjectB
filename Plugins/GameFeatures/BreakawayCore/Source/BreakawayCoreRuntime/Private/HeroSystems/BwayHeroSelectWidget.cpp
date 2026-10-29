// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeroSystems/BwayHeroSelectWidget.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "CommonInputModeTypes.h"
#include "CommonInputSubsystem.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayFrontendStateSubsystem.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HeroSystems/BwayHeroAbilityUILibrary.h"
#include "HeroSystems/BwayHeroPreviewActor.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UI/BwayAbilityPreviewWidget.h"
#include "UI/BwayCommonButton.h"
#include "UI/BwayHeroSlotWidget.h"
#include "UI/Foundation/LyraButtonBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroSelectWidget)

namespace
{
	int32 GetCanonicalGoal5HeroSortOrder(const FPrimaryAssetId& HeroId, EHeroClass HeroClass)
	{
		const FString AssetName = HeroId.PrimaryAssetName.ToString();
		if (AssetName.Contains(TEXT("Argus"), ESearchCase::IgnoreCase))
		{
			return 0;
		}
		if (AssetName.Contains(TEXT("Alona"), ESearchCase::IgnoreCase))
		{
			return 1;
		}
		if (AssetName.Contains(TEXT("Hexweaver"), ESearchCase::IgnoreCase) || AssetName.Contains(TEXT("Korryn"), ESearchCase::IgnoreCase))
		{
			return 2;
		}
		if (AssetName.Contains(TEXT("Gunslinger"), ESearchCase::IgnoreCase) || AssetName.Contains(TEXT("Rawlins"), ESearchCase::IgnoreCase))
		{
			return 3;
		}
		return 10 + static_cast<int32>(HeroClass);
	}

	FLinearColor GetClassBadgeColor(const FText& ClassNameText, EHeroClass HeroClass)
	{
		const FString Upper = ClassNameText.ToString().ToUpper();
		if (Upper.Contains(TEXT("DEBUFF")))
		{
			return FLinearColor(0.68f, 0.36f, 0.96f, 1.0f);
		}
		if (Upper.Contains(TEXT("SUPPORT")) || HeroClass == EHeroClass::Support)
		{
			return FLinearColor(0.18f, 0.84f, 0.68f, 1.0f);
		}
		if (Upper.Contains(TEXT("SHOOTER")) || HeroClass == EHeroClass::Shooter)
		{
			return FLinearColor(0.96f, 0.64f, 0.22f, 1.0f);
		}
		return FLinearColor(0.92f, 0.34f, 0.28f, 1.0f);
	}

	void SafelyDisplaceExistingNamedWidget(UWidgetTree* InWidgetTree, FName ReservedName)
	{
		if (!InWidgetTree)
		{
			return;
		}
		if (UWidget* Existing = InWidgetTree->FindWidget(ReservedName))
		{
			Existing->SetVisibility(ESlateVisibility::Collapsed);
			Existing->Rename(nullptr, InWidgetTree, REN_DontCreateRedirectors | REN_NonTransactional);
		}
	}
}

UBwayHeroSelectWidget::UBwayHeroSelectWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TOptional<FUIInputConfig> UBwayHeroSelectWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

TSharedRef<SWidget> UBwayHeroSelectWidget::RebuildWidget()
{
	if (WidgetTree && !bUsingGoal5Layout)
	{
		// Displace any legacy serialized widgets sharing reserved contract names (e.g. WBP_BW_HeroSelect)
		// so both W_HeroSelection and WBP_BW_HeroSelect construct the unified Goal 5 layout identically.
		SafelyDisplaceExistingNamedWidget(WidgetTree, TEXT("HeroSelectRootOverlay"));
		SafelyDisplaceExistingNamedWidget(WidgetTree, TEXT("Button_Lock"));
		SafelyDisplaceExistingNamedWidget(WidgetTree, TEXT("Text_HeroDetails"));
		SafelyDisplaceExistingNamedWidget(WidgetTree, TEXT("Widget_Timer"));

		bUsingGoal5Layout = true;
		HeroSlotWidgets.Reset();
		AbilityPreviewWidgets.Reset();

		const FLinearColor GoldColor(0.96f, 0.80f, 0.30f, 1.0f);
		const FLinearColor CyanColor(0.24f, 0.80f, 0.96f, 1.0f);
		const FLinearColor BlueTeamAccent(0.20f, 0.62f, 0.98f, 1.0f);
		const FLinearColor RedTeamAccent(0.94f, 0.32f, 0.30f, 1.0f);
		const FLinearColor DarkPanel(0.03f, 0.06f, 0.12f, 0.93f);
		const FLinearColor SubPanel(0.05f, 0.10f, 0.18f, 0.94f);

		auto MakeCommonText = [this](FName Name, const FText& Txt, int32 Size, const FLinearColor& Col, ETextJustify::Type Justify = ETextJustify::Left) -> UCommonTextBlock*
		{
			UCommonTextBlock* Block = Name.IsNone()
				? WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass())
				: WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), Name);
			Block->SetText(Txt);
			Block->SetColorAndOpacity(FSlateColor(Col));
			Block->SetJustification(Justify);
			FSlateFontInfo Font = Block->GetFont();
			Font.Size = Size;
			Block->SetFont(Font);
			return Block;
		};

		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HeroSelectRootOverlay"));
		WidgetTree->RootWidget = Root;

		// =========================================================================
		// 1. TOP HEADER BAR: Phase Title + Local Team Pill + Ready (X/Y) + Countdown
		// =========================================================================
		UBorder* TopBarBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopHeaderBar"));
		TopBarBorder->SetBrushColor(DarkPanel);
		TopBarBorder->SetPadding(FMargin(18.f, 10.f));
		{
			UOverlaySlot* TopSlot = Root->AddChildToOverlay(TopBarBorder);
			TopSlot->SetHorizontalAlignment(HAlign_Fill);
			TopSlot->SetVerticalAlignment(VAlign_Top);
			TopSlot->SetPadding(FMargin(24.f, 14.f, 24.f, 0.f));
		}

		UHorizontalBox* TopBarHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopHeaderHBox"));
		TopBarBorder->SetContent(TopBarHBox);

		PhaseTitleText = MakeCommonText(
			TEXT("Text_PhaseTitle"),
			NSLOCTEXT("BwayHeroSelect", "StagingHeaderTitle", "HERO SELECT — DORADO"),
			16,
			GoldColor);
		TopBarHBox->AddChildToHorizontalBox(PhaseTitleText)->SetVerticalAlignment(VAlign_Center);

		LocalTeamBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LocalTeamBadgeBorder"));
		LocalTeamBadgeBorder->SetPadding(FMargin(10.f, 3.f));
		LocalTeamBadgeBorder->SetBrushColor(FLinearColor(0.08f, 0.24f, 0.44f, 0.95f));
		LocalTeamBadgeText = MakeCommonText(
			TEXT("Text_LocalTeamBadge"),
			NSLOCTEXT("BwayHeroSelect", "BlueTeamBadge", "BLUE TEAM"),
			11,
			FLinearColor::White,
			ETextJustify::Center);
		LocalTeamBadgeBorder->SetContent(LocalTeamBadgeText);
		{
			UHorizontalBoxSlot* TeamBadgeSlot = TopBarHBox->AddChildToHorizontalBox(LocalTeamBadgeBorder);
			TeamBadgeSlot->SetVerticalAlignment(VAlign_Center);
			TeamBadgeSlot->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
		}

		TopBarHBox->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// Ready (X/Y) Counter
		UBorder* ReadyPillBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ReadyPillBorder"));
		ReadyPillBorder->SetBrushColor(SubPanel);
		ReadyPillBorder->SetPadding(FMargin(16.f, 5.f));
		ReadyCountText = MakeCommonText(
			TEXT("Text_ReadyCount"),
			NSLOCTEXT("BwayHeroSelect", "ReadyDefault", "Ready (0/0)"),
			14,
			CyanColor,
			ETextJustify::Center);
		ReadyPillBorder->SetContent(ReadyCountText);
		TopBarHBox->AddChildToHorizontalBox(ReadyPillBorder)->SetVerticalAlignment(VAlign_Center);

		TopBarHBox->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// Shared Countdown Timer ("Widget_Timer" preserved for NativeConstruct contract)
		UBorder* TimerFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Widget_Timer"));
		TimerFrame->SetBrushColor(SubPanel);
		TimerFrame->SetPadding(FMargin(16.f, 5.f));
		CountdownTimerText = MakeCommonText(
			TEXT("Text_CountdownTimer"),
			NSLOCTEXT("BwayHeroSelect", "AssembleTeamDefault", "ASSEMBLE TEAM   99"),
			16,
			GoldColor,
			ETextJustify::Right);
		TimerFrame->SetContent(CountdownTimerText);
		SelectionTimerWidget = TimerFrame;
		TopBarHBox->AddChildToHorizontalBox(TimerFrame)->SetVerticalAlignment(VAlign_Center);

		// =========================================================================
		// 2. LIVE 4v4 TEAM ROSTER SIDEBARS (Left: BLUE TEAM 4 slots, Right: RED TEAM 4 slots)
		// =========================================================================
		auto BuildTeamSidebar = [&](bool bIsBlueTeam, FTeamSidebarSlotRow OutSlots[4], TObjectPtr<UCommonTextBlock>& OutHeaderText)
		{
			const FLinearColor TeamAccent = bIsBlueTeam ? BlueTeamAccent : RedTeamAccent;
			USizeBox* SidebarWidth = WidgetTree->ConstructWidget<USizeBox>();
			SidebarWidth->SetWidthOverride(244.f);

			UOverlaySlot* OverlaySlot = Root->AddChildToOverlay(SidebarWidth);
			OverlaySlot->SetHorizontalAlignment(bIsBlueTeam ? HAlign_Left : HAlign_Right);
			OverlaySlot->SetVerticalAlignment(VAlign_Top);
			OverlaySlot->SetPadding(bIsBlueTeam ? FMargin(24.f, 74.f, 0.f, 0.f) : FMargin(0.f, 74.f, 24.f, 0.f));

			UBorder* SidebarFrame = WidgetTree->ConstructWidget<UBorder>();
			SidebarFrame->SetBrushColor(DarkPanel);
			SidebarFrame->SetPadding(FMargin(10.f, 10.f));
			SidebarWidth->AddChild(SidebarFrame);

			UVerticalBox* SidebarVBox = WidgetTree->ConstructWidget<UVerticalBox>();
			SidebarFrame->SetContent(SidebarVBox);

			UBorder* HeaderBar = WidgetTree->ConstructWidget<UBorder>();
			HeaderBar->SetBrushColor(FLinearColor(TeamAccent.R * 0.35f, TeamAccent.G * 0.35f, TeamAccent.B * 0.35f, 0.96f));
			HeaderBar->SetPadding(FMargin(10.f, 5.f));
			OutHeaderText = MakeCommonText(
				NAME_None,
				bIsBlueTeam
					? NSLOCTEXT("BwayHeroSelect", "BlueTeamSidebarHeader", "BLUE TEAM (4)")
					: NSLOCTEXT("BwayHeroSelect", "RedTeamSidebarHeader", "RED TEAM (4)"),
				12,
				FLinearColor::White);
			HeaderBar->SetContent(OutHeaderText);
			SidebarVBox->AddChildToVerticalBox(HeaderBar)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

			for (int32 SlotIdx = 0; SlotIdx < 4; ++SlotIdx)
			{
				FTeamSidebarSlotRow& Row = OutSlots[SlotIdx];
				Row.OuterBorder = WidgetTree->ConstructWidget<UBorder>();
				Row.OuterBorder->SetBrushColor(SubPanel);
				Row.OuterBorder->SetPadding(FMargin(6.f, 5.f));
				SidebarVBox->AddChildToVerticalBox(Row.OuterBorder)->SetPadding(FMargin(0.f, 3.f));

				UHorizontalBox* RowHBox = WidgetTree->ConstructWidget<UHorizontalBox>();
				Row.OuterBorder->SetContent(RowHBox);

				USizeBox* PortraitSize = WidgetTree->ConstructWidget<USizeBox>();
				PortraitSize->SetWidthOverride(38.f);
				PortraitSize->SetHeightOverride(38.f);
				UHorizontalBoxSlot* ImgSlot = RowHBox->AddChildToHorizontalBox(PortraitSize);
				ImgSlot->SetVerticalAlignment(VAlign_Center);
				ImgSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

				Row.PortraitBorder = WidgetTree->ConstructWidget<UBorder>();
				Row.PortraitBorder->SetPadding(FMargin(1.5f));
				Row.PortraitBorder->SetBrushColor(FLinearColor(TeamAccent.R * 0.45f, TeamAccent.G * 0.45f, TeamAccent.B * 0.45f, 0.90f));
				PortraitSize->AddChild(Row.PortraitBorder);

				Row.PortraitImage = WidgetTree->ConstructWidget<UImage>();
				Row.PortraitImage->SetDesiredSizeOverride(FVector2D(35.f, 35.f));
				Row.PortraitBorder->SetContent(Row.PortraitImage);

				UVerticalBox* InfoVBox = WidgetTree->ConstructWidget<UVerticalBox>();
				UHorizontalBoxSlot* InfoSlot = RowHBox->AddChildToHorizontalBox(InfoVBox);
				InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				InfoSlot->SetVerticalAlignment(VAlign_Center);

				Row.PlayerNameText = MakeCommonText(
					NAME_None,
					FText::FromString(FString::Printf(TEXT("Slot %d"), SlotIdx + 1)),
					10,
					FLinearColor::White);
				InfoVBox->AddChildToVerticalBox(Row.PlayerNameText);

				Row.HeroStatusText = MakeCommonText(
					NAME_None,
					NSLOCTEXT("BwayHeroSelect", "SlotOpen", "OPEN"),
					9,
					FLinearColor(0.68f, 0.76f, 0.88f, 0.95f));
				InfoVBox->AddChildToVerticalBox(Row.HeroStatusText);
			}
		};

		BuildTeamSidebar(true, BlueTeamSlots, BlueTeamHeaderText);
		BuildTeamSidebar(false, RedTeamSlots, RedTeamHeaderText);

		// =========================================================================
		// 3. HERO KIT & BUILDABLE INSPECTION PANEL (Center-Left, framing 3D Hero on Center-Right)
		// =========================================================================
		USizeBox* InspectionSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InspectionPanelSize"));
		InspectionSize->SetWidthOverride(556.f);
		InspectionSize->SetMaxDesiredHeight(720.f);
		{
			UOverlaySlot* InspectSlot = Root->AddChildToOverlay(InspectionSize);
			InspectSlot->SetHorizontalAlignment(HAlign_Left);
			InspectSlot->SetVerticalAlignment(VAlign_Top);
			InspectSlot->SetPadding(FMargin(282.f, 74.f, 0.f, 0.f));
		}

		InspectionPanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InspectionPanelBorder"));
		InspectionPanelBorder->SetBrushColor(DarkPanel);
		InspectionPanelBorder->SetPadding(FMargin(14.f, 12.f));
		InspectionSize->AddChild(InspectionPanelBorder);

		UVerticalBox* InspectVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InspectionVBox"));
		InspectionPanelBorder->SetContent(InspectVBox);

		// Hero Name + Class Badge + Base Stats Row
		UHorizontalBox* HeroTitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeroTitleRow"));
		InspectVBox->AddChildToVerticalBox(HeroTitleRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

		InspectedHeroNameText = MakeCommonText(
			TEXT("Text_InspectedHeroName"),
			NSLOCTEXT("BwayHeroSelect", "DefaultHeroTitle", "ARGUS"),
			22,
			FLinearColor::White);
		HeroTitleRow->AddChildToHorizontalBox(InspectedHeroNameText)->SetVerticalAlignment(VAlign_Center);

		InspectedHeroClassBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InspectedHeroClassBorder"));
		InspectedHeroClassBorder->SetPadding(FMargin(8.f, 3.f));
		InspectedHeroClassBorder->SetBrushColor(FLinearColor(0.45f, 0.16f, 0.14f, 0.96f));
		InspectedHeroClassText = MakeCommonText(
			TEXT("Text_InspectedHeroClass"),
			NSLOCTEXT("BwayHeroSelect", "DefaultClassTitle", "FIGHTER"),
			11,
			FLinearColor::White,
			ETextJustify::Center);
		InspectedHeroClassBorder->SetContent(InspectedHeroClassText);
		{
			UHorizontalBoxSlot* ClassSlot = HeroTitleRow->AddChildToHorizontalBox(InspectedHeroClassBorder);
			ClassSlot->SetVerticalAlignment(VAlign_Center);
			ClassSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		}

		HeroTitleRow->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		InspectedStatsText = MakeCommonText(
			TEXT("Text_InspectedStats"),
			NSLOCTEXT("BwayHeroSelect", "DefaultStatsReadout", "HP 0 | ATK 0 | ARM 0 | SPD 0"),
			10,
			GoldColor,
			ETextJustify::Right);
		HeroTitleRow->AddChildToHorizontalBox(InspectedStatsText)->SetVerticalAlignment(VAlign_Center);

		// Summary & Travel Failure Recovery Banner (exact contract name: "Text_HeroDetails")
		HeroDetailsBannerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HeroDetailsBannerBorder"));
		HeroDetailsBannerBorder->SetBrushColor(SubPanel);
		HeroDetailsBannerBorder->SetPadding(FMargin(10.f, 6.f));
		Text_HeroDetails = MakeCommonText(
			TEXT("Text_HeroDetails"),
			NSLOCTEXT("BwayHeroSelect", "ChooseHeroPrompt", "Hover or select a warrior to inspect base stats, kit actions, and signature buildable."),
			10,
			FLinearColor(0.84f, 0.90f, 0.98f, 1.0f));
		Text_HeroDetails->SetAutoWrapText(true);
		HeroDetailsBannerBorder->SetContent(Text_HeroDetails);
		InspectVBox->AddChildToVerticalBox(HeroDetailsBannerBorder)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

		// Scrollable Kit & Signature Buildable Rows (6 persistent UBwayAbilityPreviewWidget entries)
		USizeBox* KitScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("KitScrollSize"));
		KitScrollSize->SetMaxDesiredHeight(510.f);
		UVerticalBoxSlot* ScrollVSlot = InspectVBox->AddChildToVerticalBox(KitScrollSize);
		ScrollVSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ScrollVSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

		KitPreviewScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("KitPreviewScrollBox"));
		KitScrollSize->AddChild(KitPreviewScrollBox);

		for (int32 AbilIdx = 0; AbilIdx < 8; ++AbilIdx)
		{
			UBwayAbilityPreviewWidget* PreviewRow = WidgetTree->ConstructWidget<UBwayAbilityPreviewWidget>(
				UBwayAbilityPreviewWidget::StaticClass(),
				*FString::Printf(TEXT("AbilityPreviewRow_%d"), AbilIdx));
			PreviewRow->ClearPreview();
			AbilityPreviewWidgets.Add(PreviewRow);
			if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(KitPreviewScrollBox->AddChild(PreviewRow)))
			{
				RowSlot->SetPadding(FMargin(0.f, 2.f));
			}
		}

		// Non-duplicated Shared Movement Bar
		UBorder* SharedMoveBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SharedMovementBorder"));
		SharedMoveBorder->SetBrushColor(FLinearColor(0.06f, 0.13f, 0.22f, 0.94f));
		SharedMoveBorder->SetPadding(FMargin(10.f, 5.f));
		SharedMovementText = MakeCommonText(
			TEXT("Text_SharedMovement"),
			UBwayHeroAbilityUILibrary::ResolveSharedMovementSummaryText(nullptr),
			10,
			CyanColor);
		SharedMovementText->SetAutoWrapText(true);
		SharedMoveBorder->SetContent(SharedMovementText);
		InspectVBox->AddChildToVerticalBox(SharedMoveBorder);

		// =========================================================================
		// 4. BOTTOM BAR: 4-HERO ROSTER GRID (UBwayHeroSlotWidget) + LOCK-IN / RETRY BUTTON ("Button_Lock")
		// =========================================================================
		UBorder* BottomDockBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BottomDockBorder"));
		BottomDockBorder->SetBrushColor(DarkPanel);
		BottomDockBorder->SetPadding(FMargin(14.f, 10.f));
		{
			UOverlaySlot* BottomSlot = Root->AddChildToOverlay(BottomDockBorder);
			BottomSlot->SetHorizontalAlignment(HAlign_Center);
			BottomSlot->SetVerticalAlignment(VAlign_Bottom);
			BottomSlot->SetPadding(FMargin(24.f, 0.f, 24.f, 16.f));
		}

		UHorizontalBox* BottomDockHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BottomDockHBox"));
		BottomDockBorder->SetContent(BottomDockHBox);

		HeroSlotsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeroRosterSlotsRow"));
		BottomDockHBox->AddChildToHorizontalBox(HeroSlotsRow)->SetVerticalAlignment(VAlign_Center);

		for (int32 SlotIdx = 0; SlotIdx < 4; ++SlotIdx)
		{
			UBwayHeroSlotWidget* SlotWidget = WidgetTree->ConstructWidget<UBwayHeroSlotWidget>(
				UBwayHeroSlotWidget::StaticClass(),
				*FString::Printf(TEXT("HeroSlotCard_%d"), SlotIdx));
			HeroSlotWidgets.Add(SlotWidget);
			UHorizontalBoxSlot* CardHSlot = HeroSlotsRow->AddChildToHorizontalBox(SlotWidget);
			CardHSlot->SetPadding(FMargin(5.f, 0.f));
			CardHSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Lock-In & Travel Failure Recovery Column (exact button name: "Button_Lock")
		UVerticalBox* LockColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LockActionColumn"));
		{
			UHorizontalBoxSlot* LockColSlot = BottomDockHBox->AddChildToHorizontalBox(LockColumn);
			LockColSlot->SetVerticalAlignment(VAlign_Center);
			LockColSlot->SetPadding(FMargin(16.f, 0.f, 4.f, 0.f));
		}

		USizeBox* LockBtnSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LockButtonSize"));
		LockBtnSize->SetWidthOverride(228.f);
		LockBtnSize->SetHeightOverride(56.f);
		LockColumn->AddChildToVerticalBox(LockBtnSize);

		LockInButton = WidgetTree->ConstructWidget<UBwayCommonButton>(UBwayCommonButton::StaticClass(), TEXT("Button_Lock"));
		LockInButton->SetLabelText(NSLOCTEXT("BwayHeroSelect", "SelectHeroFirst", "SELECT A HERO"));
		LockInButton->SetFontSize(17);
		LockInButton->SetContentPadding(FMargin(18.f, 12.f));
		LockInButton->SetButtonColors(
			FLinearColor(0.62f, 0.46f, 0.12f, 0.97f),
			FLinearColor(0.84f, 0.64f, 0.16f, 1.00f),
			GoldColor,
			FLinearColor::White);
		LockBtnSize->AddChild(LockInButton);
		TravelConfirmButton = LockInButton;

		LockHintText = MakeCommonText(
			TEXT("Text_LockHint"),
			NSLOCTEXT("BwayHeroSelect", "HintChooseWarrior", "Unique heroes per team"),
			10,
			FLinearColor(0.68f, 0.76f, 0.88f, 0.95f),
			ETextJustify::Center);
		LockColumn->AddChildToVerticalBox(LockHintText)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	return Super::RebuildWidget();
}

void UBwayHeroSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!SelectionTimerWidget)
	{
		SelectionTimerWidget = GetWidgetFromName(TEXT("Widget_Timer"));
	}
	if (!TravelConfirmButton)
	{
		TravelConfirmButton = Cast<UCommonButtonBase>(GetWidgetFromName(TEXT("Button_Lock")));
	}

	if (LockInButton)
	{
		LockInButton->OnClicked().RemoveAll(this);
		LockInButton->OnClicked().AddUObject(this, &ThisClass::HandleLockInButtonClicked);
	}

	for (UBwayHeroSlotWidget* SlotWidget : HeroSlotWidgets)
	{
		if (!SlotWidget)
		{
			continue;
		}
		SlotWidget->OnHeroSlotClicked.RemoveDynamic(this, &ThisClass::HandleHeroSlotClicked);
		SlotWidget->OnHeroSlotClicked.AddDynamic(this, &ThisClass::HandleHeroSlotClicked);
		SlotWidget->OnHeroSlotFocusedOrHovered.RemoveDynamic(this, &ThisClass::HandleHeroSlotFocusedOrHovered);
		SlotWidget->OnHeroSlotFocusedOrHovered.AddDynamic(this, &ThisClass::HandleHeroSlotFocusedOrHovered);
	}

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

	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		FrontendSubsystem = LP->GetSubsystem<UBwayFrontendStateSubsystem>();
	}

	RefreshGoal5HeroSelectLayout();
}

void UBwayHeroSelectWidget::NativeDestruct()
{
	CleanupPreviewActor();
	UnbindInputMethodListener();
	UnbindFromPlayerState();
	UnbindFromSelectionManager();
	UnbindFromSelectionPhase();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionTimerHandle);
	}

	Super::NativeDestruct();
}

void UBwayHeroSelectWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UWorld* World = GetWorld(); World && !HeroRegistry)
	{
		HeroRegistry = UBwayHeroRegistry::Get(World);
	}

	RefreshSelectionManager();
	RefreshLocalPlayerState();
	BindToPlayerState();
	BindToSelectionManager();
	BindInputMethodListener();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SelectionTimerHandle,
			this,
			&UBwayHeroSelectWidget::UpdateSelectionTimer,
			0.1f,
			true);
	}

	// IMPORTANT: Never call SelectHero() automatically on activation — players start with an
	// empty authoritative selection until they explicitly pick, preserving EmptyLockRejected
	// and duplicate-selection invariants. Instead, locally inspect the selected or first hero.
	const FPrimaryAssetId ExistingSelection = GetSelectedHeroId();
	if (ExistingSelection.IsValid())
	{
		InspectedHeroId = ExistingSelection;
		OnLocalSelectionChanged(ExistingSelection);
		EnsureAndUpdatePreviewActor(ExistingSelection);
	}
	else
	{
		const TArray<FHeroDisplayInfo> Available = GetAvailableHeroes();
		for (const FHeroDisplayInfo& Info : Available)
		{
			if (Info.HeroId.IsValid())
			{
				InspectedHeroId = Info.HeroId;
				EnsureAndUpdatePreviewActor(Info.HeroId);
				break;
			}
		}
	}

	bCachedLockState = IsSelectionLocked();
	if (bCachedLockState)
	{
		OnLocalSelectionLocked(ExistingSelection);
	}
	else
	{
		OnLocalSelectionUnlocked();
	}

	OnHeroListChanged();
	RefreshHeroDetails();
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();

	bIsInitialized = true;
	RequestRefreshFocus();
}

void UBwayHeroSelectWidget::NativeOnDeactivated()
{
	CleanupPreviewActor();
	UnbindInputMethodListener();
	UnbindFromPlayerState();
	UnbindFromSelectionManager();
	UnbindFromSelectionPhase();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SelectionTimerHandle);
	}

	bIsInitialized = false;
	Super::NativeOnDeactivated();
}

UWidget* UBwayHeroSelectWidget::NativeGetDesiredFocusTarget() const
{
	if (bTravelRetryAvailable && TravelConfirmButton && TravelConfirmButton->GetIsEnabled())
	{
		return TravelConfirmButton;
	}

	if (!IsSelectionLocked())
	{
		const FPrimaryAssetId TargetId = GetSelectedHeroId().IsValid() ? GetSelectedHeroId() : InspectedHeroId;
		for (UBwayHeroSlotWidget* SlotWidget : HeroSlotWidgets)
		{
			if (SlotWidget && SlotWidget->GetVisibility() == ESlateVisibility::Visible && SlotWidget->GetIsEnabled()
				&& SlotWidget->GetHeroId() == TargetId)
			{
				return SlotWidget;
			}
		}
		for (UBwayHeroSlotWidget* SlotWidget : HeroSlotWidgets)
		{
			if (SlotWidget && SlotWidget->GetVisibility() == ESlateVisibility::Visible && SlotWidget->GetIsEnabled())
			{
				return SlotWidget;
			}
		}
	}

	if (TravelConfirmButton && TravelConfirmButton->GetIsEnabled())
	{
		return TravelConfirmButton;
	}

	return Super::NativeGetDesiredFocusTarget();
}

// ========== PUBLIC API ==========

TArray<FHeroDisplayInfo> UBwayHeroSelectWidget::GetAvailableHeroes()
{
	TArray<FHeroDisplayInfo> DisplayInfos;

	if (!HeroRegistry && GetWorld())
	{
		HeroRegistry = UBwayHeroRegistry::Get(GetWorld());
	}
	if (!HeroRegistry)
	{
		return DisplayInfos;
	}

	TArray<TSoftObjectPtr<UBwayHeroDataAsset>> HeroSoftRefs = HeroRegistry->GetAllHeroSoftObjects();

	const int32 LocalTeam = GetLocalPlayerTeam();
	const FPrimaryAssetId CurrentSelection = GetSelectedHeroId();
	const ULocalPlayer* OwningLP = GetOwningLocalPlayer();
	const TArray<FPlayerHeroSelectionState> PlayerSelections =
		SelectionManager ? SelectionManager->GetAllPlayerSelections() : TArray<FPlayerHeroSelectionState>();

	for (const TSoftObjectPtr<UBwayHeroDataAsset>& SoftRef : HeroSoftRefs)
	{
		UBwayHeroDataAsset* HeroData = HeroRegistry->LoadHeroSync(SoftRef);
		if (!HeroData)
		{
			continue;
		}

		FHeroDisplayInfo DisplayInfo;
		DisplayInfo.HeroId = HeroData->GetPrimaryAssetId();
		DisplayInfo.DisplayName = HeroData->DisplayName;
		DisplayInfo.Portrait = HeroData->Portrait;
		DisplayInfo.Stats = HeroData->HeroStats;
		DisplayInfo.HeroClass = HeroData->HeroClass;
		DisplayInfo.ClassName = HeroData->GetClassDisplayName();
		DisplayInfo.Abilities = UBwayHeroAbilityUILibrary::ResolveAbilityBarForHero(HeroData);

		if (OwningLP)
		{
			for (FAbilityDisplayInfo& AbilityInfo : DisplayInfo.Abilities)
			{
				if (AbilityInfo.InputActionTag.IsValid())
				{
					AbilityInfo.InputKeyLabel = UBwayHeroAbilityUILibrary::ResolveKeyLabelForInputTag(AbilityInfo.InputActionTag, OwningLP);
				}
			}
		}

		if (SelectionManager)
		{
			DisplayInfo.bIsAvailable = (DisplayInfo.HeroId == CurrentSelection) ||
				SelectionManager->IsHeroAvailableForTeam(DisplayInfo.HeroId, LocalTeam);
		}
		else
		{
			DisplayInfo.bIsAvailable = true;
		}

		DisplayInfo.bIsSelected = (DisplayInfo.HeroId == CurrentSelection);
		DisplayInfo.bIsLocked = IsSelectionLocked() && DisplayInfo.bIsSelected;

		DisplayInfo.SelectedByPlayerIndex = -1;
		for (int32 SelectionIndex = 0; SelectionIndex < PlayerSelections.Num(); ++SelectionIndex)
		{
			const FPlayerHeroSelectionState& Selection = PlayerSelections[SelectionIndex];
			if (Selection.SelectedHeroId == DisplayInfo.HeroId && (LocalTeam < 0 || Selection.TeamIndex == LocalTeam))
			{
				if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
				{
					DisplayInfo.SelectedByPlayerIndex = FMath::Max(0, BwayPS->GetPlayerNum());
				}
				else
				{
					DisplayInfo.SelectedByPlayerIndex = SelectionIndex;
				}
				break;
			}
		}

		DisplayInfos.Add(MoveTemp(DisplayInfo));
	}

	DisplayInfos.Sort([](const FHeroDisplayInfo& A, const FHeroDisplayInfo& B)
	{
		const int32 OrderA = GetCanonicalGoal5HeroSortOrder(A.HeroId, A.HeroClass);
		const int32 OrderB = GetCanonicalGoal5HeroSortOrder(B.HeroId, B.HeroClass);
		if (OrderA != OrderB)
		{
			return OrderA < OrderB;
		}
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});

	TMap<uint8, int32> ClassCounts;
	for (FHeroDisplayInfo& Info : DisplayInfos)
	{
		int32& Count = ClassCounts.FindOrAdd(static_cast<uint8>(Info.HeroClass));
		Info.HeroClassIndex = Count++;
	}

	return DisplayInfos;
}

FPrimaryAssetId UBwayHeroSelectWidget::GetSelectedHeroId() const
{
	if (LocalPlayerState)
	{
		return LocalPlayerState->GetSelectedHeroId();
	}
	if (FrontendSubsystem)
	{
		return FrontendSubsystem->GetSelectedHeroId();
	}
	return FPrimaryAssetId();
}

FPrimaryAssetId UBwayHeroSelectWidget::GetInspectedHeroId() const
{
	if (InspectedHeroId.IsValid())
	{
		return InspectedHeroId;
	}
	return GetSelectedHeroId();
}

void UBwayHeroSelectWidget::SetInspectedHero(FPrimaryAssetId HeroId)
{
	if (!HeroId.IsValid())
	{
		return;
	}
	const bool bChanged = (InspectedHeroId != HeroId);
	InspectedHeroId = HeroId;
	if (bChanged)
	{
		EnsureAndUpdatePreviewActor(HeroId);
	}
	RefreshHeroDetails();
	RefreshHeroRosterGrid();
	RefreshTeamRosterSidebars();
	if (bShowingTravelFailure)
	{
		RefreshTravelFailurePresentation();
	}
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
	RefreshSelectionManager();
	RefreshLocalPlayerState();
	if ((!LocalPlayerState || !SelectionManager) && GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
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

	InspectedHeroId = HeroId;

	if (LocalPlayerState)
	{
		if (FrontendSubsystem)
		{
			FrontendSubsystem->SetSelectedHeroId(HeroId);
		}

		LocalPlayerState->ServerSetSelectedHeroId(HeroId);
		EnsureAndUpdatePreviewActor(HeroId);
		RefreshHeroDetails();
		RefreshGoal5HeroSelectLayout();
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Selected hero %s (Server Route)"), *HeroId.ToString());
	}
	else if (FrontendSubsystem)
	{
		FrontendSubsystem->SetSelectedHeroId(HeroId);
		OnLocalSelectionChanged(HeroId);
		EnsureAndUpdatePreviewActor(HeroId);
		RefreshHeroDetails();
		OnHeroListChanged();
		RefreshGoal5HeroSelectLayout();
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Selected hero %s (Frontend Route)"), *HeroId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectWidget: Cannot select hero - neither player state nor frontend subsystem found"));
		return false;
	}

	return true;
}

bool UBwayHeroSelectWidget::LockSelection()
{
	RefreshSelectionManager();
	RefreshLocalPlayerState();
	if (SelectionPhase && SelectionPhase->HasPostSelectionTravelFailed() && IsSelectionLocked())
	{
		if (auto* Controller = Cast<ABwayPlayerController>(GetOwningPlayer()))
		{
			Controller->RetryHeroSelectionTravel();
		}
		RefreshTravelFailurePresentation();
		return false;
	}
	if ((!LocalPlayerState || !SelectionManager) && GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		return false;
	}
	if (!LocalPlayerState)
	{
		if (FrontendSubsystem)
		{
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Locked selection (Frontend Route)"));
			return true;
		}
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectWidget: Cannot lock selection - no player state"));
		return false;
	}

	if (IsSelectionLocked())
	{
		return true;
	}

	const FPrimaryAssetId SelectedHero = GetSelectedHeroId();
	if (!SelectedHero.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: Cannot lock - no hero selected"));
		return false;
	}

	LocalPlayerState->ServerLockHeroSelection();

	if (UBwayHeroDataAsset* HeroData = GetHeroDataAsset(SelectedHero))
	{
		if (HeroData->VoiceBank)
		{
			UGameplayStatics::PlaySound2D(this, HeroData->VoiceBank);
		}
	}
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectWidget: Locked selection"));
	return true;
}

void UBwayHeroSelectWidget::ConfirmSelection()
{
	RefreshSelectionManager();
	RefreshLocalPlayerState();
	if (SelectionPhase && SelectionPhase->HasPostSelectionTravelFailed() && IsSelectionLocked())
	{
		if (auto* Controller = Cast<ABwayPlayerController>(GetOwningPlayer()))
		{
			Controller->RetryHeroSelectionTravel();
		}
		RefreshTravelFailurePresentation();
		return;
	}

	const FPrimaryAssetId SelectedHero = GetSelectedHeroId();
	if (!SelectedHero.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectWidget: Cannot confirm - no hero selected"));
		return;
	}

	const bool bAwaitPhaseAdvance = SelectionManager && (SelectionManager->IsSelectionActive()
		|| (LocalPlayerState && GetOwningPlayer() && !GetOwningPlayer()->GetPawn()));

	if (!IsSelectionLocked())
	{
		if (!LockSelection())
		{
			return;
		}
	}

	if (bAwaitPhaseAdvance)
	{
		return;
	}

	OnHeroConfirmed.Broadcast();
	DeactivateWidget();
}

void UBwayHeroSelectWidget::RefreshSelectionLockPresentation()
{
	const bool bNowLocked = IsSelectionLocked();
	if (bNowLocked != bCachedLockState)
	{
		bCachedLockState = bNowLocked;
		if (bNowLocked)
		{
			OnLocalSelectionLocked(GetSelectedHeroId());
		}
		else
		{
			OnLocalSelectionUnlocked();
		}
		RefreshGoal5HeroSelectLayout();
		RefreshTravelFailurePresentation();
		RequestRefreshFocus();
	}
}

int32 UBwayHeroSelectWidget::GetLocalPlayerTeam() const
{
	if (LocalPlayerState)
	{
		if (const ABwayGameState* BwayGS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
		{
			return BwayGS->GetPlayerTeam(LocalPlayerState);
		}
	}
	return -1;
}

bool UBwayHeroSelectWidget::IsHeroAvailable(FPrimaryAssetId HeroId) const
{
	if (!HeroId.IsValid())
	{
		return false;
	}
	if (HeroId == GetSelectedHeroId())
	{
		return true;
	}
	if (!SelectionManager)
	{
		return true;
	}
	return SelectionManager->IsHeroAvailableForTeam(HeroId, GetLocalPlayerTeam());
}

float UBwayHeroSelectWidget::GetRemainingSelectionTime() const
{
	if (!SelectionManager || !SelectionManager->IsSelectionActive() || SelectionManager->SelectionTimeLimit <= 0.0f)
	{
		return -1.0f;
	}
	return SelectionManager->GetSelectionTimeRemaining();
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

UBwayHeroDataAsset* UBwayHeroSelectWidget::GetHeroDataAsset(FPrimaryAssetId HeroId) const
{
	if (!HeroId.IsValid())
	{
		return nullptr;
	}
	return UBwayHeroRegistry::GetHeroDataById(HeroId);
}

bool UBwayHeroSelectWidget::GetHeroDisplayInfo(FPrimaryAssetId HeroId, FHeroDisplayInfo& OutDisplayInfo) const
{
	UBwayHeroDataAsset* HeroData = GetHeroDataAsset(HeroId);
	if (!HeroData)
	{
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

	if (const ULocalPlayer* OwningLP = GetOwningLocalPlayer())
	{
		for (FAbilityDisplayInfo& AbilityInfo : OutDisplayInfo.Abilities)
		{
			if (AbilityInfo.InputActionTag.IsValid())
			{
				AbilityInfo.InputKeyLabel = UBwayHeroAbilityUILibrary::ResolveKeyLabelForInputTag(AbilityInfo.InputActionTag, OwningLP);
			}
		}
	}

	const int32 LocalTeam = GetLocalPlayerTeam();
	const FPrimaryAssetId CurrentSelection = GetSelectedHeroId();

	if (SelectionManager)
	{
		OutDisplayInfo.bIsAvailable = (OutDisplayInfo.HeroId == CurrentSelection) ||
			SelectionManager->IsHeroAvailableForTeam(OutDisplayInfo.HeroId, LocalTeam);
	}
	else
	{
		OutDisplayInfo.bIsAvailable = true;
	}

	OutDisplayInfo.bIsSelected = (OutDisplayInfo.HeroId == CurrentSelection);
	OutDisplayInfo.bIsLocked = IsSelectionLocked() && OutDisplayInfo.bIsSelected;
	OutDisplayInfo.SelectedByPlayerIndex = -1;

	if (SelectionManager)
	{
		const TArray<FPlayerHeroSelectionState> PlayerSelections = SelectionManager->GetAllPlayerSelections();
		for (int32 SelectionIndex = 0; SelectionIndex < PlayerSelections.Num(); ++SelectionIndex)
		{
			const FPlayerHeroSelectionState& Selection = PlayerSelections[SelectionIndex];
			if (Selection.SelectedHeroId == OutDisplayInfo.HeroId && (LocalTeam < 0 || Selection.TeamIndex == LocalTeam))
			{
				if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Selection.PlayerState))
				{
					OutDisplayInfo.SelectedByPlayerIndex = FMath::Max(0, BwayPS->GetPlayerNum());
				}
				else
				{
					OutDisplayInfo.SelectedByPlayerIndex = SelectionIndex;
				}
				break;
			}
		}
	}

	return true;
}

bool UBwayHeroSelectWidget::GetSelectedHeroDisplayInfo(FHeroDisplayInfo& OutDisplayInfo) const
{
	const FPrimaryAssetId SelectedId = GetSelectedHeroId();
	return SelectedId.IsValid() && GetHeroDisplayInfo(SelectedId, OutDisplayInfo);
}

bool UBwayHeroSelectWidget::GetInspectedHeroDisplayInfo(FHeroDisplayInfo& OutDisplayInfo) const
{
	const FPrimaryAssetId TargetId = GetInspectedHeroId();
	return TargetId.IsValid() && GetHeroDisplayInfo(TargetId, OutDisplayInfo);
}

TArray<FBwayTeamRosterSlotViewModel> UBwayHeroSelectWidget::GetTeamRosterSlotViewModels(int32 TeamIndex) const
{
	TArray<FBwayTeamRosterSlotViewModel> Slots;
	Slots.Reserve(4);

	const ABwayGameState* BwayGS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;

	// 1. Collect human & active PlayerState entries assigned to TeamIndex
	if (SelectionManager)
	{
		const TArray<FPlayerHeroSelectionState> AllPlayers = SelectionManager->GetAllPlayerSelections();
		for (const FPlayerHeroSelectionState& Entry : AllPlayers)
		{
			if (!Entry.PlayerState || Entry.PlayerState->IsInactive() || Entry.PlayerState->IsOnlyASpectator())
			{
				continue;
			}

			int32 ResolvedTeam = Entry.TeamIndex;
			if (ResolvedTeam < 0 && BwayGS)
			{
				ResolvedTeam = BwayGS->GetPlayerTeam(Entry.PlayerState);
			}
			if (ResolvedTeam != TeamIndex || Slots.Num() >= 4)
			{
				continue;
			}

			FBwayTeamRosterSlotViewModel VM;
			VM.TeamIndex = TeamIndex;
			VM.SlotIndex = Slots.Num();
			VM.bIsOccupied = true;
			VM.bIsBot = Entry.PlayerState->IsABot();
			VM.bIsLocalPlayer = (LocalPlayerState && Entry.PlayerState == LocalPlayerState);
			VM.bIsLocked = Entry.bIsLocked;

			FString BaseName = Entry.PlayerState->GetPlayerName();
			if (BaseName.TrimStartAndEnd().IsEmpty())
			{
				BaseName = FString::Printf(TEXT("Player %d"), Entry.PlayerState->GetPlayerId());
			}
			VM.PlayerName = VM.bIsLocalPlayer ? FString::Printf(TEXT("%s (YOU)"), *BaseName) : BaseName;

			FPrimaryAssetId EffectiveHero = Entry.SelectedHeroId;
			bool bHoverOnly = false;
			if (!EffectiveHero.IsValid() && VM.bIsLocalPlayer && InspectedHeroId.IsValid())
			{
				EffectiveHero = InspectedHeroId;
				bHoverOnly = true;
			}

			VM.HeroId = EffectiveHero;
			if (UBwayHeroDataAsset* HeroData = GetHeroDataAsset(EffectiveHero))
			{
				VM.HeroDisplayName = HeroData->DisplayName;
				VM.Portrait = HeroData->Portrait;
				if (Entry.bIsLocked)
				{
					VM.StatusText = FText::FromString(FString::Printf(TEXT("%s • LOCKED"), *HeroData->DisplayName.ToString()));
				}
				else if (bHoverOnly)
				{
					VM.StatusText = FText::FromString(FString::Printf(TEXT("%s • HOVERING"), *HeroData->DisplayName.ToString()));
				}
				else
				{
					VM.StatusText = FText::FromString(FString::Printf(TEXT("%s • SELECTING"), *HeroData->DisplayName.ToString()));
				}
			}
			else
			{
				VM.StatusText = NSLOCTEXT("BwayHeroSelect", "StatusSelecting", "SELECTING...");
			}

			Slots.Add(MoveTemp(VM));
		}
	}

	// 2. Append pre-assigned staging bot entries for TeamIndex (live bot portraits carrying into Dorado)
	if (SelectionManager && Slots.Num() < 4)
	{
		TArray<FBwayStagingBotSelection> TeamBots;
		for (const FBwayStagingBotSelection& BotSel : SelectionManager->GetStagingBotSelections())
		{
			if (BotSel.TeamIndex == TeamIndex)
			{
				TeamBots.Add(BotSel);
			}
		}
		TeamBots.Sort([](const FBwayStagingBotSelection& A, const FBwayStagingBotSelection& B)
		{
			return A.BotSlotIndex < B.BotSlotIndex;
		});

		for (const FBwayStagingBotSelection& BotSel : TeamBots)
		{
			if (Slots.Num() >= 4)
			{
				break;
			}

			FBwayTeamRosterSlotViewModel VM;
			VM.TeamIndex = TeamIndex;
			VM.SlotIndex = Slots.Num();
			VM.bIsOccupied = true;
			VM.bIsBot = true;
			VM.bIsLocked = BotSel.bIsLocked;
			VM.PlayerName = BotSel.BotDisplayName.IsEmpty()
				? FString::Printf(TEXT("Bot %d"), VM.SlotIndex + 1)
				: BotSel.BotDisplayName;
			VM.HeroId = BotSel.SelectedHeroId;

			if (UBwayHeroDataAsset* HeroData = GetHeroDataAsset(BotSel.SelectedHeroId))
			{
				VM.HeroDisplayName = HeroData->DisplayName;
				VM.Portrait = HeroData->Portrait;
				VM.StatusText = FText::FromString(FString::Printf(TEXT("%s • BOT READY"), *HeroData->DisplayName.ToString()));
			}
			else
			{
				VM.StatusText = NSLOCTEXT("BwayHeroSelect", "BotPending", "BOT READY");
			}

			Slots.Add(MoveTemp(VM));
		}
	}

	// 3. Pad up to 4 slots per team
	while (Slots.Num() < 4)
	{
		FBwayTeamRosterSlotViewModel EmptyVM;
		EmptyVM.TeamIndex = TeamIndex;
		EmptyVM.SlotIndex = Slots.Num();
		EmptyVM.bIsOccupied = false;
		EmptyVM.PlayerName = FString::Printf(TEXT("%s Slot %d"), TeamIndex == 0 ? TEXT("Blue") : TEXT("Red"), EmptyVM.SlotIndex + 1);
		EmptyVM.StatusText = NSLOCTEXT("BwayHeroSelect", "OpenTeamSlot", "OPEN SLOT");
		Slots.Add(MoveTemp(EmptyVM));
	}

	return Slots;
}

void UBwayHeroSelectWidget::ToggleAbilitiesPanel()
{
	bAbilitiesPanelVisible = !bAbilitiesPanelVisible;
	if (InspectionPanelBorder)
	{
		InspectionPanelBorder->SetVisibility(
			bAbilitiesPanelVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UBwayHeroSelectWidget::SelectHeroByIndex(int32 Index)
{
	const TArray<FHeroDisplayInfo> Heroes = GetAvailableHeroes();
	if (!Heroes.IsValidIndex(Index))
	{
		return false;
	}
	return SelectHero(Heroes[Index].HeroId);
}

FText UBwayHeroSelectWidget::GetReadyHeaderText() const
{
	return ReadyCountText ? ReadyCountText->GetText() : FText::GetEmpty();
}

FText UBwayHeroSelectWidget::GetCountdownHeaderText() const
{
	return CountdownTimerText ? CountdownTimerText->GetText() : FText::GetEmpty();
}

FText UBwayHeroSelectWidget::GetSharedMovementDisplayText() const
{
	return SharedMovementText ? SharedMovementText->GetText() : FText::GetEmpty();
}

FText UBwayHeroSelectWidget::GetInspectedStatsDisplayText() const
{
	return InspectedStatsText ? InspectedStatsText->GetText() : FText::GetEmpty();
}

// ========== BINDING & INPUT DEVICE UPDATES ==========

void UBwayHeroSelectWidget::BindInputMethodListener()
{
	UnbindInputMethodListener();
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(LP))
		{
			InputMethodChangedHandle = InputSubsystem->OnInputMethodChangedNative.AddUObject(
				this,
				&ThisClass::HandleInputMethodChanged);
		}
	}
}

void UBwayHeroSelectWidget::UnbindInputMethodListener()
{
	if (InputMethodChangedHandle.IsValid())
	{
		if (ULocalPlayer* LP = GetOwningLocalPlayer())
		{
			if (UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(LP))
			{
				InputSubsystem->OnInputMethodChangedNative.Remove(InputMethodChangedHandle);
			}
		}
		InputMethodChangedHandle.Reset();
	}
}

void UBwayHeroSelectWidget::HandleInputMethodChanged(ECommonInputType NewInputType)
{
	RefreshHeroDetails();
	if (bShowingTravelFailure)
	{
		RefreshTravelFailurePresentation();
	}
	if (NewInputType == ECommonInputType::Gamepad && !HasUserFocusedDescendants(GetOwningPlayer()))
	{
		RequestRefreshFocus();
	}
}

void UBwayHeroSelectWidget::RefreshSelectionManager()
{
	RefreshSelectionPhase();
	AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	UBwayHeroSelectionManager* CurrentManager = GS ? GS->FindComponentByClass<UBwayHeroSelectionManager>() : nullptr;
	if (SelectionManager == CurrentManager)
	{
		return;
	}
	UnbindFromSelectionManager();
	SelectionManager = CurrentManager;
	BindToSelectionManager();
	OnHeroListChanged();
	RefreshGoal5HeroSelectLayout();
	RequestRefreshFocus();
}

void UBwayHeroSelectWidget::RefreshSelectionPhase()
{
	const auto* GS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	auto* CurrentPhase = GS ? GS->HeroSelectionPhaseComponent.Get() : nullptr;
	if (SelectionPhase == CurrentPhase)
	{
		return;
	}
	UnbindFromSelectionPhase();
	SelectionPhase = CurrentPhase;
	if (SelectionPhase)
	{
		SelectionPhase->OnTravelFailureChanged.AddUniqueDynamic(this, &ThisClass::RefreshTravelFailurePresentation);
	}
	RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::UnbindFromSelectionPhase()
{
	if (SelectionPhase)
	{
		SelectionPhase->OnTravelFailureChanged.RemoveDynamic(this, &ThisClass::RefreshTravelFailurePresentation);
	}
	SelectionPhase = nullptr;
}

void UBwayHeroSelectWidget::RefreshTravelFailurePresentation()
{
	const bool bFailed = SelectionPhase && SelectionPhase->HasPostSelectionTravelFailed();
	if (!bFailed)
	{
		if (bShowingTravelFailure)
		{
			bShowingTravelFailure = false;
			bTravelRetryAvailable = false;
			if (HeroDetailsBannerBorder)
			{
				HeroDetailsBannerBorder->SetBrushColor(FLinearColor(0.05f, 0.10f, 0.18f, 0.94f));
			}
			if (Text_HeroDetails)
			{
				Text_HeroDetails->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.90f, 0.98f, 1.0f)));
			}
			if (IsSelectionLocked())
			{
				OnLocalSelectionLocked(GetSelectedHeroId());
			}
			else
			{
				OnLocalSelectionUnlocked();
			}
			RefreshHeroDetails();
			RefreshLockButtonPresentation();
			ConfigureExplicitNavigationRules();
		}
		return;
	}

	const auto* Controller = Cast<ABwayPlayerController>(GetOwningPlayer());
	const bool bHost = Controller && Controller->HasAuthority() && Controller->IsLocalController() && Controller->GetLocalPlayer();
	const bool bCanRetry = Controller && Controller->CanRetryHeroSelectionTravel();

	if (IsSelectionLocked())
	{
		const FText RetryLabel = bHost
			? NSLOCTEXT("BwayHeroSelect", "RetryTravel", "Retry travel")
			: NSLOCTEXT("BwayHeroSelect", "WaitForHostRetry", "Waiting for host");

		if (LockInButton)
		{
			LockInButton->SetLabelText(RetryLabel);
			LockInButton->SetButtonColors(
				FLinearColor(0.68f, 0.28f, 0.12f, 0.98f),
				FLinearColor(0.88f, 0.38f, 0.14f, 1.00f),
				FLinearColor(0.98f, 0.84f, 0.30f, 1.00f),
				FLinearColor::White);
			LockInButton->SetIsEnabled(bCanRetry);
			LockInButton->SetIsInteractionEnabled(bCanRetry);
		}
		else if (TravelConfirmButton)
		{
			if (ULyraButtonBase* LyraBtn = Cast<ULyraButtonBase>(TravelConfirmButton))
			{
				LyraBtn->SetButtonText(RetryLabel);
			}
			TravelConfirmButton->SetIsEnabled(bCanRetry);
			TravelConfirmButton->SetIsInteractionEnabled(bCanRetry);
		}
	}

	if (HeroDetailsBannerBorder)
	{
		HeroDetailsBannerBorder->SetBrushColor(FLinearColor(0.32f, 0.12f, 0.08f, 0.97f));
	}
	if (Text_HeroDetails)
	{
		Text_HeroDetails->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.88f, 0.64f, 1.0f)));
		Text_HeroDetails->SetText(!IsSelectionLocked()
			? NSLOCTEXT("BwayHeroSelect", "TravelFailedSelect", "Match travel failed. Choose and lock your hero; the host can retry when everyone is ready.")
			: bCanRetry ? NSLOCTEXT("BwayHeroSelect", "TravelFailedRetry", "Match travel failed. Your selections are saved. Choose Retry travel to try again.")
			: bHost ? NSLOCTEXT("BwayHeroSelect", "TravelFailedPlayers", "Match travel failed. Waiting for all players to lock before you can retry.")
			: NSLOCTEXT("BwayHeroSelect", "TravelFailedWait", "Match travel failed. Your selection is saved. Waiting for the host to retry."));
	}
	if (LockHintText)
	{
		LockHintText->SetText(bHost
			? NSLOCTEXT("BwayHeroSelect", "TravelRetryHostHint", "Host recovery active")
			: NSLOCTEXT("BwayHeroSelect", "TravelRetryClientHint", "Selections preserved"));
	}

	const bool bFocusRetry = bCanRetry && !bTravelRetryAvailable;
	bShowingTravelFailure = true;
	bTravelRetryAvailable = bCanRetry;
	ConfigureExplicitNavigationRules();
	if (bFocusRetry)
	{
		RequestRefreshFocus();
	}
}

void UBwayHeroSelectWidget::RefreshLocalPlayerState()
{
	APlayerController* PC = GetOwningPlayer();
	ABwayPlayerState* CurrentState = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
	if (CurrentState == LocalPlayerState)
	{
		RefreshSelectionLockPresentation();
		return;
	}

	UnbindFromPlayerState();
	LocalPlayerState = CurrentState;
	BindToPlayerState();

	if (LocalPlayerState)
	{
		const FPrimaryAssetId SelectedId = GetSelectedHeroId();
		if (SelectedId.IsValid())
		{
			InspectedHeroId = SelectedId;
			OnLocalSelectionChanged(SelectedId);
			EnsureAndUpdatePreviewActor(SelectedId);
		}
		bCachedLockState = IsSelectionLocked();
		if (bCachedLockState)
		{
			OnLocalSelectionLocked(SelectedId);
		}
		else
		{
			OnLocalSelectionUnlocked();
		}
		RefreshHeroDetails();
		OnHeroListChanged();
		RefreshGoal5HeroSelectLayout();
		RefreshTravelFailurePresentation();
		RequestRefreshFocus();
	}
}

void UBwayHeroSelectWidget::BindToPlayerState()
{
	if (!LocalPlayerState)
	{
		return;
	}
	LocalPlayerState->OnSelectedHeroChanged.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandlePlayerSelectionChanged);
	LocalPlayerState->OnHeroLocked.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandlePlayerHeroLocked);
}

void UBwayHeroSelectWidget::UnbindFromPlayerState()
{
	if (!LocalPlayerState)
	{
		return;
	}
	LocalPlayerState->OnSelectedHeroChanged.RemoveDynamic(this, &UBwayHeroSelectWidget::HandlePlayerSelectionChanged);
	LocalPlayerState->OnHeroLocked.RemoveDynamic(this, &UBwayHeroSelectWidget::HandlePlayerHeroLocked);
}

void UBwayHeroSelectWidget::BindToSelectionManager()
{
	if (!SelectionManager)
	{
		return;
	}
	SelectionManager->OnPlayerHeroSelectionChanged.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged);
	SelectionManager->OnPlayerHeroLocked.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandleOtherPlayerHeroLocked);
	SelectionManager->OnStagingBotSelectionsChanged.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandleStagingBotSelectionsChanged);
	SelectionManager->OnAllPlayersReady.AddUniqueDynamic(this, &UBwayHeroSelectWidget::HandleAllPlayersReady);
}

void UBwayHeroSelectWidget::UnbindFromSelectionManager()
{
	if (!SelectionManager)
	{
		return;
	}
	SelectionManager->OnPlayerHeroSelectionChanged.RemoveDynamic(this, &UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged);
	SelectionManager->OnPlayerHeroLocked.RemoveDynamic(this, &UBwayHeroSelectWidget::HandleOtherPlayerHeroLocked);
	SelectionManager->OnStagingBotSelectionsChanged.RemoveDynamic(this, &UBwayHeroSelectWidget::HandleStagingBotSelectionsChanged);
	SelectionManager->OnAllPlayersReady.RemoveDynamic(this, &UBwayHeroSelectWidget::HandleAllPlayersReady);
}

// ========== DELEGATE HANDLERS ==========

void UBwayHeroSelectWidget::HandlePlayerSelectionChanged(FPrimaryAssetId NewHeroId)
{
	if (NewHeroId.IsValid())
	{
		InspectedHeroId = NewHeroId;
		EnsureAndUpdatePreviewActor(NewHeroId);
	}
	OnLocalSelectionChanged(NewHeroId);
	RefreshHeroDetails();
	OnHeroListChanged();
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::HandlePlayerHeroLocked(FPrimaryAssetId LockedHeroId)
{
	bCachedLockState = true;
	if (LockedHeroId.IsValid())
	{
		InspectedHeroId = LockedHeroId;
	}
	OnLocalSelectionLocked(LockedHeroId);
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();
	RequestRefreshFocus();
}

void UBwayHeroSelectWidget::HandleOtherPlayerSelectionChanged(APlayerState* PlayerState, FPrimaryAssetId HeroId, int32 TeamIndex)
{
	if (TeamIndex == GetLocalPlayerTeam())
	{
		OnHeroListChanged();
	}
	OnOtherPlayerSelectionChanged(PlayerState, HeroId, TeamIndex);
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::HandleOtherPlayerHeroLocked(APlayerState* PlayerState, FPrimaryAssetId LockedHeroId)
{
	int32 PeerTeam = -1;
	if (const ABwayGameState* BwayGS = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
	{
		PeerTeam = BwayGS->GetPlayerTeam(PlayerState);
	}
	OnOtherPlayerSelectionChanged(PlayerState, LockedHeroId, PeerTeam);
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::HandleStagingBotSelectionsChanged()
{
	RefreshTeamRosterSidebars();
}

void UBwayHeroSelectWidget::HandleAllPlayersReady()
{
	OnAllPlayersReady();
	RefreshGoal5HeroSelectLayout();
	RefreshTravelFailurePresentation();
}

void UBwayHeroSelectWidget::HandleHeroSlotClicked(FPrimaryAssetId HeroId)
{
	if (HeroId.IsValid())
	{
		SelectHero(HeroId);
	}
}

void UBwayHeroSelectWidget::HandleHeroSlotFocusedOrHovered(FPrimaryAssetId HeroId)
{
	if (HeroId.IsValid())
	{
		SetInspectedHero(HeroId);
	}
}

void UBwayHeroSelectWidget::HandleLockInButtonClicked()
{
	ConfirmSelection();
}

void UBwayHeroSelectWidget::UpdateSelectionTimer()
{
	RefreshSelectionManager();
	RefreshLocalPlayerState();

	ABwayGameState* State = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr;
	UBwayRoundManagementComponent* Rounds = State ? State->GetRoundManagement() : nullptr;
	APlayerController* Controller = GetOwningPlayer();
	if (LocalPlayerState && LocalPlayerState->IsHeroLocked() && Controller && Controller->GetPawn() && Rounds
		&& (Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Playing
			|| Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::PostRound))
	{
		if (ABwayPlayerController* BwayController = Cast<ABwayPlayerController>(Controller))
		{
			BwayController->Client_HideHeroSelection();
		}
		else
		{
			DeactivateWidget();
		}
		return;
	}

	const float RemainingTime = GetRemainingSelectionTime();
	OnSelectionTimerUpdated(RemainingTime);

	if (SelectionTimerWidget)
	{
		SelectionTimerWidget->SetVisibility(RemainingTime >= 0.0f
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (CountdownTimerText)
	{
		const int32 DisplaySecs = RemainingTime >= 0.0f ? FMath::CeilToInt(RemainingTime) : 0;
		CountdownTimerText->SetText(FText::Format(
			NSLOCTEXT("BwayHeroSelect", "AssembleTeamFmt", "ASSEMBLE TEAM   {0}"),
			FText::AsNumber(DisplaySecs)));
	}

	int32 ReadyCount = 0;
	int32 TotalCount = 0;
	GetReadyPlayerCount(ReadyCount, TotalCount);
	if (ReadyCountText)
	{
		ReadyCountText->SetText(FText::Format(
			NSLOCTEXT("BwayHeroSelect", "ReadyCounterFmt", "Ready ({0}/{1})"),
			FText::AsNumber(ReadyCount),
			FText::AsNumber(TotalCount)));
	}

	RefreshTeamRosterSidebars();
}

void UBwayHeroSelectWidget::RefreshHeroDetails()
{
	FHeroDisplayInfo Hero;
	const bool bHasHero = GetInspectedHeroDisplayInfo(Hero);

	if (SharedMovementText)
	{
		SharedMovementText->SetText(UBwayHeroAbilityUILibrary::ResolveSharedMovementSummaryText(GetOwningLocalPlayer()));
	}

	if (!bHasHero)
	{
		if (!bShowingTravelFailure && Text_HeroDetails)
		{
			Text_HeroDetails->SetText(NSLOCTEXT("BwayHeroSelect", "ChooseHero", "Choose a hero to view base stats, kit actions, shared movement, and signature buildable."));
		}
		for (UBwayAbilityPreviewWidget* PreviewWidget : AbilityPreviewWidgets)
		{
			if (PreviewWidget)
			{
				PreviewWidget->ClearPreview();
			}
		}
		return;
	}

	const FLinearColor ClassAccent = GetClassBadgeColor(Hero.ClassName, Hero.HeroClass);

	if (InspectedHeroNameText)
	{
		InspectedHeroNameText->SetText(FText::FromString(Hero.DisplayName.ToString().ToUpper()));
	}
	if (InspectedHeroClassText)
	{
		InspectedHeroClassText->SetText(FText::FromString(Hero.ClassName.ToString().ToUpper()));
	}
	if (InspectedHeroClassBorder)
	{
		InspectedHeroClassBorder->SetBrushColor(FLinearColor(ClassAccent.R * 0.45f, ClassAccent.G * 0.45f, ClassAccent.B * 0.45f, 0.96f));
	}
	if (InspectedStatsText)
	{
		InspectedStatsText->SetText(FText::FromString(FString::Printf(
			TEXT("HP %d  |  ATK %d  |  ARMOR %d  |  SPEED %d"),
			FMath::RoundToInt(Hero.Stats.MaxHealth),
			FMath::RoundToInt(Hero.Stats.BaseDamage),
			FMath::RoundToInt(Hero.Stats.Armor),
			FMath::RoundToInt(Hero.Stats.MoveSpeed))));
	}

	FString BuildableName;
	for (const FAbilityDisplayInfo& AbilityInfo : Hero.Abilities)
	{
		if (AbilityInfo.bIsBuildable && !AbilityInfo.AbilityName.IsEmpty())
		{
			BuildableName = AbilityInfo.AbilityName.ToString();
			break;
		}
	}

	if (!bShowingTravelFailure && Text_HeroDetails)
	{
		const FString SummaryLine = BuildableName.IsEmpty()
			? FString::Printf(
				TEXT("%s (%s) — Health: %d  Attack: %d  Armor: %d  Speed: %d"),
				*Hero.DisplayName.ToString(),
				*Hero.ClassName.ToString(),
				FMath::RoundToInt(Hero.Stats.MaxHealth),
				FMath::RoundToInt(Hero.Stats.BaseDamage),
				FMath::RoundToInt(Hero.Stats.Armor),
				FMath::RoundToInt(Hero.Stats.MoveSpeed))
			: FString::Printf(
				TEXT("%s (%s) — Health: %d  Attack: %d  Armor: %d  Speed: %d  |  Signature Buildable: %s"),
				*Hero.DisplayName.ToString(),
				*Hero.ClassName.ToString(),
				FMath::RoundToInt(Hero.Stats.MaxHealth),
				FMath::RoundToInt(Hero.Stats.BaseDamage),
				FMath::RoundToInt(Hero.Stats.Armor),
				FMath::RoundToInt(Hero.Stats.MoveSpeed),
				*BuildableName);
		Text_HeroDetails->SetText(FText::FromString(SummaryLine));
	}

	for (int32 RowIdx = 0; RowIdx < AbilityPreviewWidgets.Num(); ++RowIdx)
	{
		UBwayAbilityPreviewWidget* PreviewWidget = AbilityPreviewWidgets[RowIdx];
		if (!PreviewWidget)
		{
			continue;
		}
		if (Hero.Abilities.IsValidIndex(RowIdx))
		{
			PreviewWidget->SetAbilityPreview(Hero.Abilities[RowIdx]);
		}
		else
		{
			PreviewWidget->ClearPreview();
		}
	}
}

void UBwayHeroSelectWidget::RefreshGoal5HeroSelectLayout()
{
	if (!bUsingGoal5Layout)
	{
		return;
	}

	const int32 LocalTeam = GetLocalPlayerTeam();
	if (LocalTeamBadgeBorder && LocalTeamBadgeText)
	{
		if (LocalTeam == 1)
		{
			LocalTeamBadgeText->SetText(NSLOCTEXT("BwayHeroSelect", "RedTeamBadge", "RED TEAM"));
			LocalTeamBadgeBorder->SetBrushColor(FLinearColor(0.46f, 0.14f, 0.14f, 0.95f));
		}
		else if (LocalTeam == 0)
		{
			LocalTeamBadgeText->SetText(NSLOCTEXT("BwayHeroSelect", "BlueTeamBadge", "BLUE TEAM"));
			LocalTeamBadgeBorder->SetBrushColor(FLinearColor(0.10f, 0.28f, 0.50f, 0.95f));
		}
		else
		{
			LocalTeamBadgeText->SetText(NSLOCTEXT("BwayHeroSelect", "StagingLobbyBadge", "STAGING"));
			LocalTeamBadgeBorder->SetBrushColor(FLinearColor(0.16f, 0.22f, 0.32f, 0.92f));
		}
	}

	int32 ReadyCount = 0;
	int32 TotalCount = 0;
	GetReadyPlayerCount(ReadyCount, TotalCount);
	if (ReadyCountText)
	{
		ReadyCountText->SetText(FText::Format(
			NSLOCTEXT("BwayHeroSelect", "ReadyCounterFmt", "Ready ({0}/{1})"),
			FText::AsNumber(ReadyCount),
			FText::AsNumber(TotalCount)));
	}

	RefreshHeroRosterGrid();
	RefreshTeamRosterSidebars();
	RefreshLockButtonPresentation();
	ConfigureExplicitNavigationRules();
}

void UBwayHeroSelectWidget::RefreshHeroRosterGrid()
{
	if (!HeroSlotsRow)
	{
		return;
	}

	const TArray<FHeroDisplayInfo> Heroes = GetAvailableHeroes();
	const bool bLocalLocked = IsSelectionLocked();
	const FPrimaryAssetId CurrentInspected = GetInspectedHeroId();

	while (HeroSlotWidgets.Num() < Heroes.Num())
	{
		const int32 NewIdx = HeroSlotWidgets.Num();
		UBwayHeroSlotWidget* ExtraSlot = WidgetTree->ConstructWidget<UBwayHeroSlotWidget>(
			UBwayHeroSlotWidget::StaticClass(),
			*FString::Printf(TEXT("HeroSlotCard_%d"), NewIdx));
		ExtraSlot->OnHeroSlotClicked.AddDynamic(this, &ThisClass::HandleHeroSlotClicked);
		ExtraSlot->OnHeroSlotFocusedOrHovered.AddDynamic(this, &ThisClass::HandleHeroSlotFocusedOrHovered);
		HeroSlotWidgets.Add(ExtraSlot);
		UHorizontalBoxSlot* CardHSlot = HeroSlotsRow->AddChildToHorizontalBox(ExtraSlot);
		CardHSlot->SetPadding(FMargin(5.f, 0.f));
		CardHSlot->SetVerticalAlignment(VAlign_Center);
	}

	for (int32 SlotIdx = 0; SlotIdx < HeroSlotWidgets.Num(); ++SlotIdx)
	{
		UBwayHeroSlotWidget* SlotWidget = HeroSlotWidgets[SlotIdx];
		if (!SlotWidget)
		{
			continue;
		}
		if (Heroes.IsValidIndex(SlotIdx))
		{
			SlotWidget->SetVisibility(ESlateVisibility::Visible);
			SlotWidget->SetHeroData(Heroes[SlotIdx]);
			SlotWidget->SetInspectedState(Heroes[SlotIdx].HeroId == CurrentInspected);
			SlotWidget->SetLocalPhaseLockedState(bLocalLocked);
		}
		else
		{
			SlotWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UBwayHeroSelectWidget::RefreshTeamRosterSidebars()
{
	if (!bUsingGoal5Layout)
	{
		return;
	}

	auto UpdateSidebar = [](
		const TArray<FBwayTeamRosterSlotViewModel>& ViewModels,
		FTeamSidebarSlotRow Rows[4],
		UCommonTextBlock* HeaderText,
		bool bIsBlueTeam)
	{
		const FLinearColor TeamAccent = bIsBlueTeam
			? FLinearColor(0.20f, 0.62f, 0.98f, 1.0f)
			: FLinearColor(0.94f, 0.32f, 0.30f, 1.0f);

		int32 OccupiedCount = 0;
		for (int32 Idx = 0; Idx < 4; ++Idx)
		{
			if (!ViewModels.IsValidIndex(Idx))
			{
				continue;
			}
			const FBwayTeamRosterSlotViewModel& VM = ViewModels[Idx];
			if (VM.bIsOccupied)
			{
				++OccupiedCount;
			}

			FTeamSidebarSlotRow& Row = Rows[Idx];
			if (Row.PlayerNameText)
			{
				Row.PlayerNameText->SetText(FText::FromString(VM.PlayerName));
				Row.PlayerNameText->SetColorAndOpacity(FSlateColor(
					VM.bIsLocalPlayer
						? FLinearColor(0.98f, 0.84f, 0.32f, 1.0f)
						: (VM.bIsOccupied ? FLinearColor::White : FLinearColor(0.48f, 0.54f, 0.64f, 0.85f))));
			}
			if (Row.HeroStatusText)
			{
				Row.HeroStatusText->SetText(VM.StatusText);
				Row.HeroStatusText->SetColorAndOpacity(FSlateColor(
					VM.bIsLocked
						? FLinearColor(0.30f, 0.94f, 0.54f, 1.0f)
						: (VM.bIsBot
							? FLinearColor(0.56f, 0.80f, 0.98f, 0.95f)
							: FLinearColor(0.74f, 0.82f, 0.92f, 0.95f))));
			}
			if (Row.OuterBorder)
			{
				if (VM.bIsLocalPlayer)
				{
					Row.OuterBorder->SetBrushColor(FLinearColor(0.11f, 0.20f, 0.34f, 0.97f));
				}
				else if (VM.bIsLocked)
				{
					Row.OuterBorder->SetBrushColor(FLinearColor(0.06f, 0.16f, 0.14f, 0.95f));
				}
				else
				{
					Row.OuterBorder->SetBrushColor(FLinearColor(0.05f, 0.09f, 0.16f, 0.90f));
				}
			}
			if (Row.PortraitBorder)
			{
				Row.PortraitBorder->SetBrushColor(VM.bIsLocked
					? FLinearColor(0.26f, 0.90f, 0.50f, 1.0f)
					: FLinearColor(TeamAccent.R * 0.45f, TeamAccent.G * 0.45f, TeamAccent.B * 0.45f, 0.90f));
			}
			if (Row.PortraitImage)
			{
				if (VM.Portrait)
				{
					Row.PortraitImage->SetBrushFromTexture(VM.Portrait, false);
					const float Opacity = VM.bIsLocked ? 1.0f : 0.78f;
					Row.PortraitImage->SetColorAndOpacity(FLinearColor(Opacity, Opacity, Opacity, 1.0f));
				}
				else
				{
					Row.PortraitImage->SetBrushFromTexture(nullptr, false);
					Row.PortraitImage->SetBrushTintColor(FSlateColor(
						VM.bIsOccupied
							? FLinearColor(TeamAccent.R * 0.35f, TeamAccent.G * 0.35f, TeamAccent.B * 0.35f, 0.90f)
							: FLinearColor(0.08f, 0.11f, 0.17f, 0.70f)));
				}
			}
		}

		if (HeaderText)
		{
			HeaderText->SetText(FText::FromString(FString::Printf(
				TEXT("%s (%d/4)"),
				bIsBlueTeam ? TEXT("BLUE TEAM") : TEXT("RED TEAM"),
				OccupiedCount)));
		}
	};

	UpdateSidebar(GetTeamRosterSlotViewModels(0), BlueTeamSlots, BlueTeamHeaderText, true);
	UpdateSidebar(GetTeamRosterSlotViewModels(1), RedTeamSlots, RedTeamHeaderText, false);
}

void UBwayHeroSelectWidget::RefreshLockButtonPresentation()
{
	if (bShowingTravelFailure)
	{
		return;
	}

	const bool bLocked = IsSelectionLocked();
	const bool bHasSelection = GetSelectedHeroId().IsValid();

	if (LockInButton)
	{
		if (bLocked)
		{
			LockInButton->SetLabelText(NSLOCTEXT("BwayHeroSelect", "LockedInBtn", "LOCKED IN"));
			LockInButton->SetButtonColors(
				FLinearColor(0.14f, 0.48f, 0.26f, 0.96f),
				FLinearColor(0.18f, 0.58f, 0.32f, 1.00f),
				FLinearColor(0.32f, 0.92f, 0.54f, 1.00f),
				FLinearColor::White);
			LockInButton->SetIsEnabled(false);
			LockInButton->SetIsInteractionEnabled(false);
		}
		else if (bHasSelection)
		{
			LockInButton->SetLabelText(NSLOCTEXT("BwayHeroSelect", "LockInBtn", "LOCK IN"));
			LockInButton->SetButtonColors(
				FLinearColor(0.62f, 0.46f, 0.12f, 0.97f),
				FLinearColor(0.84f, 0.64f, 0.16f, 1.00f),
				FLinearColor(0.98f, 0.84f, 0.30f, 1.00f),
				FLinearColor::White);
			LockInButton->SetIsEnabled(true);
			LockInButton->SetIsInteractionEnabled(true);
		}
		else
		{
			LockInButton->SetLabelText(NSLOCTEXT("BwayHeroSelect", "SelectHeroBtn", "SELECT A HERO"));
			LockInButton->SetButtonColors(
				FLinearColor(0.10f, 0.15f, 0.24f, 0.90f),
				FLinearColor(0.14f, 0.20f, 0.32f, 0.95f),
				FLinearColor(0.36f, 0.48f, 0.64f, 0.90f),
				FLinearColor(0.70f, 0.76f, 0.86f, 1.00f));
			LockInButton->SetIsEnabled(false);
			LockInButton->SetIsInteractionEnabled(false);
		}
	}

	if (LockHintText)
	{
		if (bLocked)
		{
			LockHintText->SetText(NSLOCTEXT("BwayHeroSelect", "HintWaitingForPeers", "Waiting for all players"));
		}
		else if (bHasSelection)
		{
			LockHintText->SetText(NSLOCTEXT("BwayHeroSelect", "HintConfirmLock", "Confirm warrior to lock in"));
		}
		else
		{
			LockHintText->SetText(NSLOCTEXT("BwayHeroSelect", "HintChooseWarrior", "Unique heroes per team"));
		}
	}
}

void UBwayHeroSelectWidget::ConfigureExplicitNavigationRules()
{
	TArray<UBwayHeroSlotWidget*> VisibleSlots;
	for (UBwayHeroSlotWidget* SlotWidget : HeroSlotWidgets)
	{
		if (SlotWidget && SlotWidget->GetVisibility() == ESlateVisibility::Visible && SlotWidget->GetIsEnabled())
		{
			VisibleSlots.Add(SlotWidget);
		}
	}

	const bool bLockFocusable = TravelConfirmButton && TravelConfirmButton->GetIsEnabled();

	for (int32 Idx = 0; Idx < VisibleSlots.Num(); ++Idx)
	{
		UBwayHeroSlotWidget* Current = VisibleSlots[Idx];
		if (Idx > 0)
		{
			Current->SetNavigationRuleExplicit(EUINavigation::Left, VisibleSlots[Idx - 1]);
		}
		else
		{
			Current->SetNavigationRuleBase(EUINavigation::Left, EUINavigationRule::Stop);
		}

		if (Idx + 1 < VisibleSlots.Num())
		{
			Current->SetNavigationRuleExplicit(EUINavigation::Right, VisibleSlots[Idx + 1]);
		}
		else if (bLockFocusable)
		{
			Current->SetNavigationRuleExplicit(EUINavigation::Right, TravelConfirmButton);
		}
		else
		{
			Current->SetNavigationRuleBase(EUINavigation::Right, EUINavigationRule::Stop);
		}

		if (bLockFocusable)
		{
			Current->SetNavigationRuleExplicit(EUINavigation::Down, TravelConfirmButton);
		}
		else
		{
			Current->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Stop);
		}
		Current->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Stop);
	}

	if (TravelConfirmButton)
	{
		if (!VisibleSlots.IsEmpty())
		{
			UBwayHeroSlotWidget* ReturnSlot = VisibleSlots.Last();
			const FPrimaryAssetId SelectedId = GetSelectedHeroId();
			for (UBwayHeroSlotWidget* SlotWidget : VisibleSlots)
			{
				if (SlotWidget->GetHeroId() == SelectedId)
				{
					ReturnSlot = SlotWidget;
					break;
				}
			}
			TravelConfirmButton->SetNavigationRuleExplicit(EUINavigation::Left, ReturnSlot);
			TravelConfirmButton->SetNavigationRuleExplicit(EUINavigation::Up, ReturnSlot);
		}
		else
		{
			TravelConfirmButton->SetNavigationRuleBase(EUINavigation::Left, EUINavigationRule::Stop);
			TravelConfirmButton->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Stop);
		}
		TravelConfirmButton->SetNavigationRuleBase(EUINavigation::Right, EUINavigationRule::Stop);
		TravelConfirmButton->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Stop);
	}
}

void UBwayHeroSelectWidget::EnsureAndUpdatePreviewActor(FPrimaryAssetId HeroId)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	UBwayHeroDataAsset* HeroData = GetHeroDataAsset(HeroId);
	if (!PreviewActor || !IsValid(PreviewActor))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		PreviewActor = World->SpawnActor<ABwayHeroPreviewActor>(
			ABwayHeroPreviewActor::StaticClass(),
			FVector(0.0f, 0.0f, 1450.0f),
			FRotator(0.0f, -20.0f, 0.0f),
			SpawnParams);
	}

	if (PreviewActor)
	{
		PreviewActor->UpdatePreviewHero(HeroData);
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (!PC->GetPawn() && PC->GetViewTarget() != PreviewActor)
			{
				PC->SetViewTargetWithBlend(PreviewActor, 0.25f);
			}
		}
	}
}

void UBwayHeroSelectWidget::CleanupPreviewActor()
{
	if (PreviewActor && IsValid(PreviewActor))
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (PC->GetViewTarget() == PreviewActor && PC->GetPawn())
			{
				PC->SetViewTarget(PC->GetPawn());
			}
		}
		PreviewActor->Destroy();
	}
	PreviewActor = nullptr;
}
