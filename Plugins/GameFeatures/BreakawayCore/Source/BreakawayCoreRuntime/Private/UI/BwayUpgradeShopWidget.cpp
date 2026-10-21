#include "UI/BwayUpgradeShopWidget.h"
#include "BwayPlayerState.h"
#include "BwayPlayerController.h"
#include "BwayGameState.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "CommonInputModeTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Input/Reply.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayUpgradeShopWidget)

namespace BwayShopUI
{
	static const FLinearColor BackdropColor(0.015f, 0.025f, 0.045f, 0.94f);
	static const FLinearColor PanelBg(0.03f, 0.055f, 0.095f, 0.96f);
	static const FLinearColor CardBg(0.06f, 0.10f, 0.16f, 0.95f);
	static const FLinearColor GoldAccent(0.85f, 0.65f, 0.18f, 1.0f);
	static const FLinearColor GoldBright(1.0f, 0.84f, 0.32f, 1.0f);
	static const FLinearColor CyanAccent(0.18f, 0.72f, 0.95f, 1.0f);
	static const FLinearColor MutedText(0.65f, 0.72f, 0.80f, 1.0f);

	static UTextBlock* MakeShopText(UWidgetTree* Tree, const FText& Text, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetText(Text);
		Block->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo FontInfo = Block->GetFont();
		FontInfo.Size = FontSize;
		Block->SetFont(FontInfo);
		return Block;
	}

	static FText GetItemTitlePrefix(FName Id, const FText& DisplayName)
	{
		if (Id == TEXT("AttackStrength")) return FText::FromString(TEXT("SWORD OF MARS (+Attack Strength)"));
		if (Id == TEXT("LifeLeech")) return FText::FromString(TEXT("FANG OF ACHILLES (+Life Leech)"));
		if (Id == TEXT("Armor")) return FText::FromString(TEXT("AEGIS SHIELD (+Armor)"));
		if (Id == TEXT("RespawnTimeReduction")) return FText::FromString(TEXT("PHOENIX TALISMAN (-Respawn Time)"));
		if (Id == TEXT("MoveSpeed")) return FText::FromString(TEXT("BOOTS OF HERMES (+Move Speed)"));
		if (Id == TEXT("CooldownReduction")) return FText::FromString(TEXT("CHRONOS HOURGLASS (-Cooldowns)"));
		return DisplayName;
	}
}

UBwayUpgradeShopWidget::UBwayUpgradeShopWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

TSharedRef<SWidget> UBwayUpgradeShopWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
		WidgetTree->RootWidget = Root;

		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(BwayShopUI::BackdropColor);
		UOverlaySlot* BackdropSlot = Root->AddChildToOverlay(Backdrop);
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);

		UVerticalBox* MasterVBox = WidgetTree->ConstructWidget<UVerticalBox>();
		UOverlaySlot* MasterSlot = Root->AddChildToOverlay(MasterVBox);
		MasterSlot->SetHorizontalAlignment(HAlign_Fill);
		MasterSlot->SetVerticalAlignment(VAlign_Fill);
		MasterSlot->SetPadding(FMargin(42.0f, 22.0f, 42.0f, 24.0f));

		// ====================================================================
		// 1. TOP BAR: Pre-Round Countdown Timer (15 -> 0) + Close Button (X)
		// ====================================================================
		UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>();
		MasterVBox->AddChildToVerticalBox(TopBar)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		UTextBlock* ShopHeaderTitle = BwayShopUI::MakeShopText(
			WidgetTree, NSLOCTEXT("BwayShop", "Title", "MATCH UPGRADE SHOP"), 22, BwayShopUI::GoldBright);
		UHorizontalBoxSlot* TitleSlot = TopBar->AddChildToHorizontalBox(ShopHeaderTitle);
		TitleSlot->SetVerticalAlignment(VAlign_Center);

		USpacer* LeftTopSpacer = WidgetTree->ConstructWidget<USpacer>();
		TopBar->AddChildToHorizontalBox(LeftTopSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UBorder* TimerBadge = WidgetTree->ConstructWidget<UBorder>();
		TimerBadge->SetBrushColor(BwayShopUI::PanelBg);
		TimerBadge->SetPadding(FMargin(24.0f, 8.0f));
		PreRoundTimerText = BwayShopUI::MakeShopText(
			WidgetTree, NSLOCTEXT("BwayShop", "WarmupCountdown", "0:15"), 28, FLinearColor::White);
		TimerBadge->SetContent(PreRoundTimerText);
		TopBar->AddChildToHorizontalBox(TimerBadge);

		USpacer* RightTopSpacer = WidgetTree->ConstructWidget<USpacer>();
		TopBar->AddChildToHorizontalBox(RightTopSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		CloseButton = WidgetTree->ConstructWidget<UButton>();
		CloseButton->SetBackgroundColor(BwayShopUI::CardBg);
		UTextBlock* CloseLabel = BwayShopUI::MakeShopText(
			WidgetTree, NSLOCTEXT("BwayShop", "Close", "CLOSE [ESC]"), 14, FLinearColor::White);
		CloseButton->AddChild(CloseLabel);
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::CloseShop);
		TopBar->AddChildToHorizontalBox(CloseButton)->SetVerticalAlignment(VAlign_Center);

		// ====================================================================
		// 2. MAIN SPLIT: Left (3-Col Grid + 4-Slot Inventory) & Right (Inspector)
		// ====================================================================
		UHorizontalBox* MainSplit = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* MainSplitSlot = MasterVBox->AddChildToVerticalBox(MainSplit);
		MainSplitSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// ---------------- LEFT PANE (Categorized Grid + Inventory Bar) ----------------
		UVerticalBox* LeftPane = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBoxSlot* LeftPaneSlot = MainSplit->AddChildToHorizontalBox(LeftPane);
		LeftPaneSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LeftPaneSlot->SetPadding(FMargin(0.0f, 0.0f, 20.0f, 0.0f));

		// 3-Column Categorized Upgrade Grid: ATTACK, DEFENSE, MISC
		UHorizontalBox* CategoryColumnsRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBoxSlot* GridSlot = LeftPane->AddChildToVerticalBox(CategoryColumnsRow);
		GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		GridSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));

		auto CreateCategoryColumn = [this, CategoryColumnsRow](const FText& ColTitle, const FLinearColor& HeaderTint, TObjectPtr<UVerticalBox>& OutItemsBox)
		{
			UBorder* ColBorder = WidgetTree->ConstructWidget<UBorder>();
			ColBorder->SetBrushColor(BwayShopUI::PanelBg);
			ColBorder->SetPadding(FMargin(12.0f));
			UHorizontalBoxSlot* ColSlot = CategoryColumnsRow->AddChildToHorizontalBox(ColBorder);
			ColSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ColSlot->SetPadding(FMargin(4.0f, 0.0f));

			UVerticalBox* ColVBox = WidgetTree->ConstructWidget<UVerticalBox>();
			ColBorder->SetContent(ColVBox);

			UBorder* HeaderBar = WidgetTree->ConstructWidget<UBorder>();
			HeaderBar->SetBrushColor(HeaderTint);
			HeaderBar->SetPadding(FMargin(12.0f, 8.0f));
			UTextBlock* HeaderLabel = BwayShopUI::MakeShopText(WidgetTree, ColTitle, 16, FLinearColor::White);
			HeaderBar->SetContent(HeaderLabel);
			ColVBox->AddChildToVerticalBox(HeaderBar)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

			OutItemsBox = WidgetTree->ConstructWidget<UVerticalBox>();
			ColVBox->AddChildToVerticalBox(OutItemsBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		};

		CreateCategoryColumn(NSLOCTEXT("BwayShop", "Cat_Attack", "ATTACK"), FLinearColor(0.50f, 0.14f, 0.12f, 0.95f), AttackColumnBox);
		CreateCategoryColumn(NSLOCTEXT("BwayShop", "Cat_Defense", "DEFENSE"), FLinearColor(0.12f, 0.32f, 0.55f, 0.95f), DefenseColumnBox);
		CreateCategoryColumn(NSLOCTEXT("BwayShop", "Cat_Misc", "MISC"), FLinearColor(0.28f, 0.22f, 0.48f, 0.95f), MiscColumnBox);

		// Bottom 4-Slot INVENTORY Bar
		UBorder* InventoryPanel = WidgetTree->ConstructWidget<UBorder>();
		InventoryPanel->SetBrushColor(BwayShopUI::PanelBg);
		InventoryPanel->SetPadding(FMargin(16.0f, 12.0f));
		LeftPane->AddChildToVerticalBox(InventoryPanel);

		UVerticalBox* InvVBox = WidgetTree->ConstructWidget<UVerticalBox>();
		InventoryPanel->SetContent(InvVBox);

		UTextBlock* InvHeader = BwayShopUI::MakeShopText(
			WidgetTree, NSLOCTEXT("BwayShop", "InvHeader", "INVENTORY (4 SLOTS)"), 14, BwayShopUI::GoldBright);
		InvVBox->AddChildToVerticalBox(InvHeader)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		UHorizontalBox* InvSlotsRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		InvVBox->AddChildToVerticalBox(InvSlotsRow);

		InventorySlotButtons.Reset();
		InventorySlotLabels.Reset();
		for (int32 SlotIdx = 0; SlotIdx < 4; ++SlotIdx)
		{
			UButton* SlotBtn = WidgetTree->ConstructWidget<UButton>();
			SlotBtn->SetBackgroundColor(BwayShopUI::CardBg);
			UTextBlock* SlotLbl = BwayShopUI::MakeShopText(
				WidgetTree, NSLOCTEXT("BwayShop", "EmptyInvSlot", "EMPTY SLOT"), 13, BwayShopUI::MutedText);
			SlotBtn->AddChild(SlotLbl);
			UHorizontalBoxSlot* HSlot = InvSlotsRow->AddChildToHorizontalBox(SlotBtn);
			HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			HSlot->SetPadding(FMargin(4.0f, 0.0f));
			InventorySlotButtons.Add(SlotBtn);
			InventorySlotLabels.Add(SlotLbl);
		}
		InventorySlotButtons[0]->OnClicked.AddDynamic(this, &ThisClass::OnInventorySlot0Clicked);
		InventorySlotButtons[1]->OnClicked.AddDynamic(this, &ThisClass::OnInventorySlot1Clicked);
		InventorySlotButtons[2]->OnClicked.AddDynamic(this, &ThisClass::OnInventorySlot2Clicked);
		InventorySlotButtons[3]->OnClicked.AddDynamic(this, &ThisClass::OnInventorySlot3Clicked);

		// ---------------- RIGHT PANE (Inspector Panel) ----------------
		USizeBox* RightInspectorSize = WidgetTree->ConstructWidget<USizeBox>();
		RightInspectorSize->SetWidthOverride(480.0f);
		MainSplit->AddChildToHorizontalBox(RightInspectorSize);

		UBorder* InspectorPanel = WidgetTree->ConstructWidget<UBorder>();
		InspectorPanel->SetBrushColor(BwayShopUI::PanelBg);
		InspectorPanel->SetPadding(FMargin(22.0f, 18.0f));
		RightInspectorSize->AddChild(InspectorPanel);

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		InspectorPanel->SetContent(Column);

		InspectorTitleText = BwayShopUI::MakeShopText(
			WidgetTree, NSLOCTEXT("BwayShop", "InspectorDefault", "SWORD OF MARS"), 20, BwayShopUI::GoldBright);
		Column->AddChildToVerticalBox(InspectorTitleText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

		InspectorCategoryText = BwayShopUI::MakeShopText(
			WidgetTree, NSLOCTEXT("BwayShop", "InspectorCatDefault", "ATTACK UPGRADE"), 13, BwayShopUI::CyanAccent);
		Column->AddChildToVerticalBox(InspectorCategoryText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		Balance = BwayShopUI::MakeShopText(WidgetTree, FText::GetEmpty(), 16, BwayShopUI::GoldBright);
		Column->AddChildToVerticalBox(Balance)->SetPadding(FMargin(0.0f, 4.0f));

		// Selector (kept focused and directly above BuyButton for gamepad navigation & BwayShopInputTestCommands)
		Selector = WidgetTree->ConstructWidget<UComboBoxString>();
		Selector->SetEnableGamepadNavigationMode(true);
		Column->AddChildToVerticalBox(Selector)->SetPadding(FMargin(0.0f, 8.0f));
		Selector->OnSelectionChanged.AddDynamic(this, &ThisClass::SelectionChanged);

		Details = BwayShopUI::MakeShopText(WidgetTree, FText::GetEmpty(), 13, BwayShopUI::MutedText);
		Details->SetAutoWrapText(true);
		Column->AddChildToVerticalBox(Details)->SetPadding(FMargin(0.0f, 6.0f));

		// Tier I-IV Progression Rows
		TierRowBorders.Reset();
		TierRowTexts.Reset();
		for (int32 TierIdx = 0; TierIdx < 4; ++TierIdx)
		{
			UBorder* TierRow = WidgetTree->ConstructWidget<UBorder>();
			TierRow->SetBrushColor(BwayShopUI::CardBg);
			TierRow->SetPadding(FMargin(12.0f, 6.0f));
			UTextBlock* TierText = BwayShopUI::MakeShopText(
				WidgetTree,
				FText::FromString(FString::Printf(TEXT("TIER %d"), TierIdx + 1)),
				13,
				BwayShopUI::MutedText);
			TierRow->SetContent(TierText);
			Column->AddChildToVerticalBox(TierRow)->SetPadding(FMargin(0.0f, 3.0f));
			TierRowBorders.Add(TierRow);
			TierRowTexts.Add(TierText);
		}

		// Action Buttons: BUY, SELL, UNDO
		BuyButton = WidgetTree->ConstructWidget<UButton>();
		BuyButton->SetBackgroundColor(BwayShopUI::GoldAccent);
		BuyLabel = BwayShopUI::MakeShopText(WidgetTree, NSLOCTEXT("BwayShop", "BuyBtn", "BUY"), 16, FLinearColor::White);
		BuyButton->AddChild(BuyLabel);
		Column->AddChildToVerticalBox(BuyButton)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 6.0f));
		BuyButton->OnClicked.AddDynamic(this, &ThisClass::BuySelected);

		// Explicit navigation rule so Gamepad Down from Selector always moves focus to BuyButton
		Selector->SetNavigationRuleExplicit(EUINavigation::Down, BuyButton);
		BuyButton->SetNavigationRuleExplicit(EUINavigation::Up, Selector);

		UHorizontalBox* SecondaryActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		Column->AddChildToVerticalBox(SecondaryActionRow)->SetPadding(FMargin(0.0f, 4.0f));

		SellButton = WidgetTree->ConstructWidget<UButton>();
		SellButton->SetBackgroundColor(BwayShopUI::CardBg);
		SellLabel = BwayShopUI::MakeShopText(WidgetTree, NSLOCTEXT("BwayShop", "SellBtn", "SELL"), 14, FLinearColor::White);
		SellButton->AddChild(SellLabel);
		SellButton->OnClicked.AddDynamic(this, &ThisClass::SellSelected);
		UHorizontalBoxSlot* SellSlot = SecondaryActionRow->AddChildToHorizontalBox(SellButton);
		SellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SellSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));

		UndoButton = WidgetTree->ConstructWidget<UButton>();
		UndoButton->SetBackgroundColor(BwayShopUI::CardBg);
		UndoLabel = BwayShopUI::MakeShopText(WidgetTree, NSLOCTEXT("BwayShop", "UndoBtn", "UNDO"), 14, FLinearColor::White);
		UndoButton->AddChild(UndoLabel);
		UndoButton->OnClicked.AddDynamic(this, &ThisClass::UndoLastAction);
		UHorizontalBoxSlot* UndoSlot = SecondaryActionRow->AddChildToHorizontalBox(UndoButton);
		UndoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UndoSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));

		Status = BwayShopUI::MakeShopText(WidgetTree, FText::GetEmpty(), 13, BwayShopUI::CyanAccent);
		Status->SetAutoWrapText(true);
		Column->AddChildToVerticalBox(Status)->SetPadding(FMargin(0.0f, 8.0f));
	}
	return Super::RebuildWidget();
}

TOptional<FUIInputConfig> UBwayUpgradeShopWidget::GetDesiredInputConfig() const
{
	FUIInputConfig Config(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	Config.bIgnoreMoveInput = true;
	Config.bIgnoreLookInput = true;
	return Config;
}

void UBwayUpgradeShopWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	const ABwayPlayerState* PS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<ABwayPlayerState>() : nullptr;
	Shop = PS ? PS->UpgradeComponent : nullptr;
	ASC = PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS) : nullptr;
	if (Shop)
	{
		Shop->OnUpgradesChanged.AddUniqueDynamic(this, &ThisClass::Refresh);
		Shop->OnPurchaseResult.AddUniqueDynamic(this, &ThisClass::PurchaseResult);
	}
	if (ASC)
	{
		GoldHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute())
			.AddWeakLambda(this, [this](const FOnAttributeChangeData&) { Refresh(); });
	}
	bPending = false;
	Refresh();
	// Spatial eligibility and warmup countdown update while open; gold and ownership updates remain delegate-driven.
	GetWorld()->GetTimerManager().SetTimer(AccessTimer, this, &ThisClass::RefreshAccess, 0.25f, true);
}

void UBwayUpgradeShopWidget::NativeOnDeactivated()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(AccessTimer);
	if (Shop)
	{
		Shop->OnUpgradesChanged.RemoveDynamic(this, &ThisClass::Refresh);
		Shop->OnPurchaseResult.RemoveDynamic(this, &ThisClass::PurchaseResult);
	}
	if (ASC) ASC->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute()).Remove(GoldHandle);
	Shop = nullptr;
	ASC = nullptr;
	Super::NativeOnDeactivated();
}

void UBwayUpgradeShopWidget::RebuildCategoryGrid()
{
	const UBwayUpgradeCatalog* Catalog = Shop ? Shop->GetCatalog() : nullptr;
	if (!Catalog || !AttackColumnBox || !DefenseColumnBox || !MiscColumnBox)
	{
		return;
	}

	AttackColumnBox->ClearChildren();
	DefenseColumnBox->ClearChildren();
	MiscColumnBox->ClearChildren();
	CatalogCardButtons.Reset();
	CatalogCardLabels.Reset();

	for (int32 Idx = 0; Idx < Catalog->Upgrades.Num(); ++Idx)
	{
		const FBwayUpgradeDefinition& Entry = Catalog->Upgrades[Idx];
		UVerticalBox* TargetColumn = MiscColumnBox;
		switch (Entry.GetEffectiveCategory())
		{
		case EBwayUpgradeCategory::Attack:
			TargetColumn = AttackColumnBox;
			break;
		case EBwayUpgradeCategory::Defense:
			TargetColumn = DefenseColumnBox;
			break;
		default:
			TargetColumn = MiscColumnBox;
			break;
		}

		UButton* CardBtn = WidgetTree->ConstructWidget<UButton>();
		CardBtn->SetBackgroundColor(BwayShopUI::CardBg);
		UTextBlock* CardText = BwayShopUI::MakeShopText(WidgetTree, Entry.DisplayName, 13, FLinearColor::White);
		CardText->SetAutoWrapText(true);
		CardBtn->AddChild(CardText);

		switch (Idx)
		{
		case 0: CardBtn->OnClicked.AddDynamic(this, &ThisClass::OnCatalogItem0Clicked); break;
		case 1: CardBtn->OnClicked.AddDynamic(this, &ThisClass::OnCatalogItem1Clicked); break;
		case 2: CardBtn->OnClicked.AddDynamic(this, &ThisClass::OnCatalogItem2Clicked); break;
		case 3: CardBtn->OnClicked.AddDynamic(this, &ThisClass::OnCatalogItem3Clicked); break;
		case 4: CardBtn->OnClicked.AddDynamic(this, &ThisClass::OnCatalogItem4Clicked); break;
		case 5: CardBtn->OnClicked.AddDynamic(this, &ThisClass::OnCatalogItem5Clicked); break;
		case 6: CardBtn->OnClicked.AddDynamic(this, &ThisClass::OnCatalogItem6Clicked); break;
		case 7: CardBtn->OnClicked.AddDynamic(this, &ThisClass::OnCatalogItem7Clicked); break;
		default: break;
		}

		TargetColumn->AddChildToVerticalBox(CardBtn)->SetPadding(FMargin(0.0f, 4.0f));
		CatalogCardButtons.Add(CardBtn);
		CatalogCardLabels.Add(CardText);
	}
}

void UBwayUpgradeShopWidget::Refresh()
{
	if (!Selector) return;
	const UBwayUpgradeCatalog* Catalog = Shop ? Shop->GetCatalog() : nullptr;
	if (!Catalog)
	{
		if (Status) Status->SetText(NSLOCTEXT("BwayShop", "Unavailable", "Upgrades are unavailable in this session."));
		if (BuyButton) BuyButton->SetIsEnabled(false);
		if (SellButton) SellButton->SetIsEnabled(false);
		if (UndoButton) UndoButton->SetIsEnabled(false);
		return;
	}
	if (DisplayIds.Num() != Catalog->Upgrades.Num())
	{
		DisplayIds.Reset();
		Selector->ClearOptions();
		for (const FBwayUpgradeDefinition& Entry : Catalog->Upgrades)
		{
			DisplayIds.Add(Entry.Id);
			Selector->AddOption(Entry.DisplayName.ToString());
		}
		RebuildCategoryGrid();
		if (!DisplayIds.IsEmpty()) Selector->SetSelectedIndex(0);
	}

	const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	const int32 CurrentGoldInt = Gold ? FMath::FloorToInt(Gold->GetCurrentGold()) : 0;
	Balance->SetText(FText::Format(NSLOCTEXT("BwayShop", "Balance", "Gold: {0}    Upgrades: {1}/{2}"),
		FText::AsNumber(CurrentGoldInt), FText::AsNumber(Shop->GetOwnedUpgrades().Num()), FText::AsNumber(Catalog->MaxOwnedUpgrades)));

	const int32 SelectedIdx = Selector->GetSelectedIndex();

	// Update 3-column Categorized Upgrade Grid cards
	for (int32 i = 0; i < Catalog->Upgrades.Num() && i < CatalogCardLabels.Num(); ++i)
	{
		const FBwayUpgradeDefinition& Entry = Catalog->Upgrades[i];
		const int32 OwnedRank = Shop->GetOwnedRank(Entry.Id);
		const FString RecBadge = Entry.IsEffectiveRecommended() ? TEXT("[RECOMMENDED] ") : TEXT("");
		const FString CostStr = Entry.Ranks.IsValidIndex(OwnedRank)
			? FString::Printf(TEXT("%d G"), Entry.Ranks[OwnedRank].Cost)
			: TEXT("MAX");
		const FText ItemTitle = BwayShopUI::GetItemTitlePrefix(Entry.Id, Entry.DisplayName);
		CatalogCardLabels[i]->SetText(FText::FromString(
			FString::Printf(TEXT("%s%s\nTier %d/%d  |  %s"), *RecBadge, *ItemTitle.ToString(), OwnedRank, Entry.Ranks.Num(), *CostStr)));
		if (CatalogCardButtons.IsValidIndex(i) && CatalogCardButtons[i])
		{
			CatalogCardButtons[i]->SetBackgroundColor(i == SelectedIdx
				? FLinearColor(0.14f, 0.28f, 0.44f, 1.0f)
				: BwayShopUI::CardBg);
		}
	}

	// Update 4-slot Inventory Bar
	const TArray<FBwayOwnedUpgrade> OwnedList = Shop->GetOwnedUpgrades();
	for (int32 SlotIdx = 0; SlotIdx < InventorySlotLabels.Num(); ++SlotIdx)
	{
		if (!InventorySlotLabels[SlotIdx]) continue;
		if (OwnedList.IsValidIndex(SlotIdx))
		{
			const FBwayOwnedUpgrade& OwnedItem = OwnedList[SlotIdx];
			const FBwayUpgradeDefinition* Def = Catalog->FindUpgrade(OwnedItem.Id);
			const FString NameStr = Def ? Def->DisplayName.ToString() : OwnedItem.Id.ToString();
			InventorySlotLabels[SlotIdx]->SetText(FText::FromString(
				FString::Printf(TEXT("[%d] %s (Tier %d)"), SlotIdx + 1, *NameStr, OwnedItem.Rank)));
			InventorySlotLabels[SlotIdx]->SetColorAndOpacity(FSlateColor(BwayShopUI::GoldBright));
		}
		else
		{
			InventorySlotLabels[SlotIdx]->SetText(NSLOCTEXT("BwayShop", "EmptyInvSlot", "EMPTY SLOT"));
			InventorySlotLabels[SlotIdx]->SetColorAndOpacity(FSlateColor(BwayShopUI::MutedText));
		}
	}

	// Update Right-Side Inspector Panel + Tier I-IV Progression Rows
	if (DisplayIds.IsValidIndex(SelectedIdx))
	{
		const FBwayUpgradeDefinition* Entry = Catalog->FindUpgrade(DisplayIds[SelectedIdx]);
		const int32 Rank = Entry ? Shop->GetOwnedRank(Entry->Id) : 0;
		if (Entry)
		{
			if (InspectorTitleText)
			{
				InspectorTitleText->SetText(BwayShopUI::GetItemTitlePrefix(Entry->Id, Entry->DisplayName));
			}
			if (InspectorCategoryText)
			{
				const TCHAR* CatLabel = Entry->GetEffectiveCategory() == EBwayUpgradeCategory::Attack
					? TEXT("ATTACK UPGRADE")
					: (Entry->GetEffectiveCategory() == EBwayUpgradeCategory::Defense ? TEXT("DEFENSE UPGRADE") : TEXT("MISC UPGRADE"));
				InspectorCategoryText->SetText(FText::FromString(
					FString::Printf(TEXT("%s%s"), CatLabel, Entry->IsEffectiveRecommended() ? TEXT("  •  RECOMMENDED") : TEXT(""))));
			}
			Details->SetText(FText::Format(NSLOCTEXT("BwayShop", "Details", "{0}\n\nOwned rank: {1}/{2}"),
				Entry->Description, FText::AsNumber(Rank), FText::AsNumber(Entry->Ranks.Num())));

			for (int32 TierIdx = 0; TierIdx < TierRowTexts.Num(); ++TierIdx)
			{
				if (!TierRowTexts[TierIdx] || !TierRowBorders.IsValidIndex(TierIdx)) continue;
				if (Entry->Ranks.IsValidIndex(TierIdx))
				{
					const FBwayUpgradeRank& RankInfo = Entry->Ranks[TierIdx];
					const bool bOwnedTier = (TierIdx < Rank);
					const bool bNextTier = (TierIdx == Rank);
					const TCHAR* Roman = (TierIdx == 0) ? TEXT("I") : (TierIdx == 1 ? TEXT("II") : (TierIdx == 2 ? TEXT("III") : TEXT("IV")));
					const FString StateTag = bOwnedTier ? TEXT("OWNED") : (bNextTier ? TEXT("NEXT") : TEXT("LOCKED"));
					TierRowTexts[TierIdx]->SetText(FText::FromString(
						FString::Printf(TEXT("TIER %s   |   Bonus: +%.2f   |   Cost: %d Gold   [%s]"),
							Roman, RankInfo.Magnitude, RankInfo.Cost, *StateTag)));
					TierRowTexts[TierIdx]->SetColorAndOpacity(FSlateColor(
						bOwnedTier ? BwayShopUI::GoldBright : (bNextTier ? FLinearColor::White : BwayShopUI::MutedText)));
					TierRowBorders[TierIdx]->SetBrushColor(
						bNextTier ? FLinearColor(0.12f, 0.24f, 0.38f, 0.98f) : BwayShopUI::CardBg);
				}
				else
				{
					TierRowTexts[TierIdx]->SetText(FText::GetEmpty());
				}
			}

			BuyLabel->SetText(Entry->Ranks.IsValidIndex(Rank)
				? FText::Format(NSLOCTEXT("BwayShop", "Buy", "BUY RANK {0} — {1} GOLD"), FText::AsNumber(Rank + 1), FText::AsNumber(Entry->Ranks[Rank].Cost))
				: NSLOCTEXT("BwayShop", "Maximum", "MAXIMUM RANK OWNED"));

			if (SellLabel)
			{
				SellLabel->SetText(Rank > 0 && Entry->Ranks.IsValidIndex(Rank - 1)
					? FText::Format(NSLOCTEXT("BwayShop", "SellFmt", "SELL (+{0}G)"), FText::AsNumber(Entry->Ranks[Rank - 1].Cost))
					: NSLOCTEXT("BwayShop", "SellBtn", "SELL"));
			}
		}
	}
	RefreshAccess();
}

void UBwayUpgradeShopWidget::RefreshAccess()
{
	if (UWorld* World = GetWorld())
	{
		if (const ABwayGameState* GS = World->GetGameState<ABwayGameState>())
		{
			if (const UBwayRoundManagementComponent* RoundMgmt = GS->GetRoundManagement())
			{
				if (PreRoundTimerText)
				{
					if (RoundMgmt->GetCurrentMatchPhase() == EBwayMatchPhase::Warmup)
					{
						const int32 RemSec = RoundMgmt->GetWarmupTimeRemaining();
						PreRoundTimerText->SetText(FText::FromString(FString::Printf(TEXT("PRE-ROUND SHOP — 0:%02d"), RemSec)));
					}
					else
					{
						PreRoundTimerText->SetText(NSLOCTEXT("BwayShop", "InMatchHeader", "IN-MATCH UPGRADE SHOP"));
					}
				}
			}
		}
	}

	if (!Shop || !Shop->GetCatalog() || !BuyButton) return;
	const int32 Index = Selector ? Selector->GetSelectedIndex() : INDEX_NONE;
	const FBwayUpgradeDefinition* Entry = DisplayIds.IsValidIndex(Index) ? Shop->GetCatalog()->FindUpgrade(DisplayIds[Index]) : nullptr;
	const int32 Rank = Entry ? Shop->GetOwnedRank(Entry->Id) : 0;
	const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	const bool bOpen = Shop->IsPurchaseWindowOpen();
	if (bOpen && !bAccessWasOpen && !bPending && Status) Status->SetText(FText::GetEmpty());
	bAccessWasOpen = bOpen;
	const bool bBuyHadFocus = BuyButton->HasUserFocus(GetOwningPlayer());
	BuyButton->SetIsEnabled(!bPending && bOpen && Entry && Entry->Ranks.IsValidIndex(Rank) && Gold
		&& Gold->GetCurrentGold() >= Entry->Ranks[Rank].Cost
		&& (Rank > 0 || Shop->GetOwnedUpgrades().Num() < Shop->GetCatalog()->MaxOwnedUpgrades));
	if (SellButton)
	{
		SellButton->SetIsEnabled(!bPending && bOpen && Entry && Shop->CanSellUpgrade(Entry->Id));
	}
	if (UndoButton)
	{
		UndoButton->SetIsEnabled(!bPending && bOpen && Shop->CanUndoLastTransaction());
	}
	// A pending purchase or a depleted wallet must not strand controller focus
	// on a disabled button. Keep selection available for the next decision.
	if (bBuyHadFocus && !BuyButton->GetIsEnabled() && Selector) Selector->SetUserFocus(GetOwningPlayer());
	if (!bOpen && Status) Status->SetText(NSLOCTEXT("BwayShop", "Access", "Buy at your base, while dead, or during round planning."));
}

void UBwayUpgradeShopWidget::SelectUpgradeById(FName UpgradeId)
{
	const int32 FoundIndex = DisplayIds.IndexOfByKey(UpgradeId);
	if (FoundIndex != INDEX_NONE)
	{
		SelectUpgradeByIndex(FoundIndex);
	}
}

void UBwayUpgradeShopWidget::SelectUpgradeByIndex(int32 CatalogIndex)
{
	if (Selector && DisplayIds.IsValidIndex(CatalogIndex))
	{
		Selector->SetSelectedIndex(CatalogIndex);
		Refresh();
	}
}

void UBwayUpgradeShopWidget::BuySelected()
{
	RefreshAccess();
	const int32 Index = Selector ? Selector->GetSelectedIndex() : INDEX_NONE;
	if (!Shop || bPending || !DisplayIds.IsValidIndex(Index) || !BuyButton->GetIsEnabled()) return;
	bPending = true;
	if (Status) Status->SetText(NSLOCTEXT("BwayShop", "Pending", "Purchasing…"));
	RefreshAccess();
	Shop->ServerPurchaseUpgrade(DisplayIds[Index], Shop->GetOwnedRank(DisplayIds[Index]));
}

void UBwayUpgradeShopWidget::SellSelected()
{
	RefreshAccess();
	const int32 Index = Selector ? Selector->GetSelectedIndex() : INDEX_NONE;
	if (!Shop || bPending || !DisplayIds.IsValidIndex(Index) || !Shop->CanSellUpgrade(DisplayIds[Index])) return;
	bPending = true;
	if (Status) Status->SetText(NSLOCTEXT("BwayShop", "Selling", "Selling upgrade…"));
	RefreshAccess();
	Shop->ServerSellUpgrade(DisplayIds[Index]);
}

void UBwayUpgradeShopWidget::UndoLastAction()
{
	RefreshAccess();
	if (!Shop || bPending || !Shop->CanUndoLastTransaction()) return;
	bPending = true;
	if (Status) Status->SetText(NSLOCTEXT("BwayShop", "Undoing", "Undoing transaction…"));
	RefreshAccess();
	Shop->ServerUndoLastTransaction();
}

void UBwayUpgradeShopWidget::PurchaseResult(FName Id, bool bSuccess)
{
	bPending = false;
	if (Status)
	{
		Status->SetText(bSuccess ? NSLOCTEXT("BwayShop", "Success", "Purchase complete.")
			: NSLOCTEXT("BwayShop", "Rejected", "Purchase declined. Check your gold, rank, and purchase access."));
	}
	Refresh();
}

void UBwayUpgradeShopWidget::SelectionChanged(FString Selected, ESelectInfo::Type Type) { Refresh(); }
void UBwayUpgradeShopWidget::CloseShop() { DeactivateWidget(); }
UWidget* UBwayUpgradeShopWidget::NativeGetDesiredFocusTarget() const { return Selector ? static_cast<UWidget*>(Selector) : CloseButton.Get(); }

void UBwayUpgradeShopWidget::OnCatalogItem0Clicked() { SelectUpgradeByIndex(0); }
void UBwayUpgradeShopWidget::OnCatalogItem1Clicked() { SelectUpgradeByIndex(1); }
void UBwayUpgradeShopWidget::OnCatalogItem2Clicked() { SelectUpgradeByIndex(2); }
void UBwayUpgradeShopWidget::OnCatalogItem3Clicked() { SelectUpgradeByIndex(3); }
void UBwayUpgradeShopWidget::OnCatalogItem4Clicked() { SelectUpgradeByIndex(4); }
void UBwayUpgradeShopWidget::OnCatalogItem5Clicked() { SelectUpgradeByIndex(5); }
void UBwayUpgradeShopWidget::OnCatalogItem6Clicked() { SelectUpgradeByIndex(6); }
void UBwayUpgradeShopWidget::OnCatalogItem7Clicked() { SelectUpgradeByIndex(7); }

void UBwayUpgradeShopWidget::OnInventorySlot0Clicked()
{
	if (Shop && Shop->GetOwnedUpgrades().IsValidIndex(0)) SelectUpgradeById(Shop->GetOwnedUpgrades()[0].Id);
}
void UBwayUpgradeShopWidget::OnInventorySlot1Clicked()
{
	if (Shop && Shop->GetOwnedUpgrades().IsValidIndex(1)) SelectUpgradeById(Shop->GetOwnedUpgrades()[1].Id);
}
void UBwayUpgradeShopWidget::OnInventorySlot2Clicked()
{
	if (Shop && Shop->GetOwnedUpgrades().IsValidIndex(2)) SelectUpgradeById(Shop->GetOwnedUpgrades()[2].Id);
}
void UBwayUpgradeShopWidget::OnInventorySlot3Clicked()
{
	if (Shop && Shop->GetOwnedUpgrades().IsValidIndex(3)) SelectUpgradeById(Shop->GetOwnedUpgrades()[3].Id);
}

FReply UBwayUpgradeShopWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const ABwayPlayerController* Controller = Cast<ABwayPlayerController>(GetOwningPlayer());
	const UEnhancedInputLocalPlayerSubsystem* Input = GetOwningLocalPlayer()
		? GetOwningLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	// CommonUI owns Back. A keyboard toggle closes the menu, but gamepad
	// directions must remain available for navigating the selector and buttons.
	if (!Event.GetKey().IsGamepadKey() && Controller && Input && Input->QueryKeysMappedToAction(Controller->GetUpgradeShopInputAction()).Contains(Event.GetKey()))
	{
		if (!Event.IsRepeat()) CloseShop();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
