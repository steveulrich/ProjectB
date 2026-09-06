#include "UI/BwayUpgradeShopWidget.h"
#include "BwayPlayerState.h"
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
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "CommonInputModeTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Input/Reply.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayUpgradeShopWidget)

TSharedRef<SWidget> UBwayUpgradeShopWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
		WidgetTree->RootWidget = Root;
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
		Backdrop->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.045f, 0.96f));
		UOverlaySlot* BackdropSlot = Root->AddChildToOverlay(Backdrop);
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(680.0f);
		UOverlaySlot* Center = Root->AddChildToOverlay(Size);
		Center->SetHorizontalAlignment(HAlign_Center);
		Center->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Size->AddChild(Column);
		auto AddText = [this, Column](const FText& Text)
		{
			UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
			Block->SetText(Text);
			Block->SetAutoWrapText(true);
			Column->AddChildToVerticalBox(Block)->SetPadding(FMargin(0, 8));
			return Block;
		};
		AddText(NSLOCTEXT("BwayShop", "Title", "MATCH UPGRADES"));
		Balance = AddText(FText::GetEmpty());
		Selector = WidgetTree->ConstructWidget<UComboBoxString>();
		Column->AddChildToVerticalBox(Selector)->SetPadding(FMargin(0, 12));
		Selector->OnSelectionChanged.AddDynamic(this, &ThisClass::SelectionChanged);
		Details = AddText(FText::GetEmpty());
		BuyButton = WidgetTree->ConstructWidget<UButton>();
		BuyLabel = WidgetTree->ConstructWidget<UTextBlock>();
		BuyButton->AddChild(BuyLabel);
		Column->AddChildToVerticalBox(BuyButton)->SetPadding(FMargin(0, 12));
		BuyButton->OnClicked.AddDynamic(this, &ThisClass::BuySelected);
		Status = AddText(FText::GetEmpty());
		CloseButton = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>();
		CloseLabel->SetText(NSLOCTEXT("BwayShop", "Close", "Close"));
		CloseButton->AddChild(CloseLabel);
		Column->AddChildToVerticalBox(CloseButton)->SetPadding(FMargin(0, 12));
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::CloseShop);
	}
	return Super::RebuildWidget();
}

TOptional<FUIInputConfig> UBwayUpgradeShopWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
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
	// Spatial eligibility changes without replicated attribute events. Poll only
	// while open; gold and ownership updates remain delegate-driven.
	GetWorld()->GetTimerManager().SetTimer(AccessTimer, this, &ThisClass::RefreshAccess, 0.25f, true);
}

void UBwayUpgradeShopWidget::NativeOnDeactivated()
{
	GetWorld()->GetTimerManager().ClearTimer(AccessTimer);
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

void UBwayUpgradeShopWidget::Refresh()
{
	if (!Selector) return;
	const UBwayUpgradeCatalog* Catalog = Shop ? Shop->GetCatalog() : nullptr;
	if (!Catalog)
	{
		Status->SetText(NSLOCTEXT("BwayShop", "Unavailable", "Upgrades are unavailable in this session."));
		BuyButton->SetIsEnabled(false);
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
		if (!DisplayIds.IsEmpty()) Selector->SetSelectedIndex(0);
	}
	const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	Balance->SetText(FText::Format(NSLOCTEXT("BwayShop", "Balance", "Gold: {0}    Upgrades: {1}/{2}"),
		FText::AsNumber(Gold ? FMath::FloorToInt(Gold->GetCurrentGold()) : 0), FText::AsNumber(Shop->GetOwnedUpgrades().Num()), FText::AsNumber(Catalog->MaxOwnedUpgrades)));
	const int32 Index = Selector->GetSelectedIndex();
	if (DisplayIds.IsValidIndex(Index))
	{
		const FBwayUpgradeDefinition* Entry = Catalog->FindUpgrade(DisplayIds[Index]);
		const int32 Rank = Shop->GetOwnedRank(Entry->Id);
		Details->SetText(FText::Format(NSLOCTEXT("BwayShop", "Details", "{0}\n\nOwned rank: {1}/{2}"), Entry->Description, FText::AsNumber(Rank), FText::AsNumber(Entry->Ranks.Num())));
		BuyLabel->SetText(Entry->Ranks.IsValidIndex(Rank)
			? FText::Format(NSLOCTEXT("BwayShop", "Buy", "Buy rank {0} — {1} gold"), FText::AsNumber(Rank + 1), FText::AsNumber(Entry->Ranks[Rank].Cost))
			: NSLOCTEXT("BwayShop", "Maximum", "Maximum rank owned"));
	}
	RefreshAccess();
}

void UBwayUpgradeShopWidget::RefreshAccess()
{
	if (!Shop || !Shop->GetCatalog() || !BuyButton) return;
	const int32 Index = Selector->GetSelectedIndex();
	const FBwayUpgradeDefinition* Entry = DisplayIds.IsValidIndex(Index) ? Shop->GetCatalog()->FindUpgrade(DisplayIds[Index]) : nullptr;
	const int32 Rank = Entry ? Shop->GetOwnedRank(Entry->Id) : 0;
	const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	const bool bOpen = Shop->IsPurchaseWindowOpen();
	if (bOpen && !bAccessWasOpen && !bPending) Status->SetText(FText::GetEmpty());
	bAccessWasOpen = bOpen;
	BuyButton->SetIsEnabled(!bPending && bOpen && Entry && Entry->Ranks.IsValidIndex(Rank) && Gold
		&& Gold->GetCurrentGold() >= Entry->Ranks[Rank].Cost
		&& (Rank > 0 || Shop->GetOwnedUpgrades().Num() < Shop->GetCatalog()->MaxOwnedUpgrades));
	if (!bOpen) Status->SetText(NSLOCTEXT("BwayShop", "Access", "Buy at your base, while dead, or during round planning."));
}

void UBwayUpgradeShopWidget::BuySelected()
{
	const int32 Index = Selector->GetSelectedIndex();
	if (!Shop || bPending || !DisplayIds.IsValidIndex(Index) || !BuyButton->GetIsEnabled()) return;
	bPending = true;
	Status->SetText(NSLOCTEXT("BwayShop", "Pending", "Purchasing…"));
	RefreshAccess();
	Shop->ServerPurchaseUpgrade(DisplayIds[Index], Shop->GetOwnedRank(DisplayIds[Index]));
}

void UBwayUpgradeShopWidget::PurchaseResult(FName Id, bool bSuccess)
{
	bPending = false;
	Status->SetText(bSuccess ? NSLOCTEXT("BwayShop", "Success", "Purchase complete.")
		: NSLOCTEXT("BwayShop", "Rejected", "Purchase declined. Check your gold, rank, and purchase access."));
	Refresh();
}

void UBwayUpgradeShopWidget::SelectionChanged(FString Selected, ESelectInfo::Type Type) { Refresh(); }
void UBwayUpgradeShopWidget::CloseShop() { DeactivateWidget(); }
UWidget* UBwayUpgradeShopWidget::NativeGetDesiredFocusTarget() const { return Selector ? static_cast<UWidget*>(Selector) : CloseButton.Get(); }
FReply UBwayUpgradeShopWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::B || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		CloseShop();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}
