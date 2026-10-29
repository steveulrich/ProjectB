// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayHeroSlotWidget.h"
#include "UI/BwayCommonButton.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroSlotWidget)

namespace
{
	FLinearColor ResolveClassAccentColor(const FText& ClassNameText, EHeroClass HeroClass)
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
}

UBwayHeroSlotWidget::UBwayHeroSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	bHideInputAction = true;
	Style = UBwayTransparentCommonButtonStyle::StaticClass();
}

bool UBwayHeroSlotWidget::Initialize()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		EnsureVisualTreeConstructed();
	}
	return Super::Initialize();
}

void UBwayHeroSlotWidget::EnsureVisualTreeConstructed()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree->RootWidget)
	{
		return;
	}

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotCardSize"));
	CardSize->SetWidthOverride(158.f);
	CardSize->SetHeightOverride(196.f);
	WidgetTree->RootWidget = CardSize;

	OuterFocusBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotOuterFocusBorder"));
	OuterFocusBorder->SetPadding(FMargin(2.5f));
	CardSize->AddChild(OuterFocusBorder);

	InnerCardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotInnerCardBorder"));
	InnerCardBorder->SetPadding(FMargin(8.f, 8.f));
	OuterFocusBorder->SetContent(InnerCardBorder);

	UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotCardVBox"));
	InnerCardBorder->SetContent(CardVBox);

	// Top row: Class Badge + Player/Lock/Taken Status Badge
	UHorizontalBox* TopBadgeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SlotTopBadgeRow"));
	CardVBox->AddChildToVerticalBox(TopBadgeRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	ClassBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotClassBadgeBorder"));
	ClassBadgeBorder->SetPadding(FMargin(6.f, 2.f));
	ClassBadgeText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("SlotClassBadgeText"));
	{
		FSlateFontInfo Font = ClassBadgeText->GetFont();
		Font.Size = 9;
		ClassBadgeText->SetFont(Font);
		ClassBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	ClassBadgeBorder->SetContent(ClassBadgeText);
	TopBadgeRow->AddChildToHorizontalBox(ClassBadgeBorder)->SetVerticalAlignment(VAlign_Center);

	USpacer* TopSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("SlotTopSpacer"));
	TopBadgeRow->AddChildToHorizontalBox(TopSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	StatusBadgeText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("SlotStatusBadgeText"));
	{
		FSlateFontInfo Font = StatusBadgeText->GetFont();
		Font.Size = 9;
		StatusBadgeText->SetFont(Font);
		StatusBadgeText->SetJustification(ETextJustify::Right);
	}
	TopBadgeRow->AddChildToHorizontalBox(StatusBadgeText)->SetVerticalAlignment(VAlign_Center);

	// Center Portrait Frame
	USizeBox* PortraitSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotPortraitSize"));
	PortraitSize->SetWidthOverride(112.f);
	PortraitSize->SetHeightOverride(112.f);
	UVerticalBoxSlot* PortraitSlot = CardVBox->AddChildToVerticalBox(PortraitSize);
	PortraitSlot->SetHorizontalAlignment(HAlign_Center);
	PortraitSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));

	PortraitBackdropBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotPortraitBackdrop"));
	PortraitBackdropBorder->SetPadding(FMargin(2.f));
	PortraitSize->AddChild(PortraitBackdropBorder);

	PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SlotPortraitImage"));
	PortraitImage->SetDesiredSizeOverride(FVector2D(108.f, 108.f));
	PortraitBackdropBorder->SetContent(PortraitImage);

	// Bottom Hero Display Name
	HeroNameText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("SlotHeroNameText"));
	{
		FSlateFontInfo Font = HeroNameText->GetFont();
		Font.Size = 14;
		HeroNameText->SetFont(Font);
		HeroNameText->SetJustification(ETextJustify::Center);
	}
	CardVBox->AddChildToVerticalBox(HeroNameText)->SetHorizontalAlignment(HAlign_Fill);

	RefreshVisualPresentation();
}

void UBwayHeroSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshInteractability();
	RefreshVisualPresentation();
}

void UBwayHeroSlotWidget::SetHeroData(const FHeroDisplayInfo& HeroInfo)
{
	HeroId = HeroInfo.HeroId;
	HeroDisplayName = HeroInfo.DisplayName;
	HeroClassName = HeroInfo.ClassName;
	HeroClass = HeroInfo.HeroClass;
	HeroPortrait = HeroInfo.Portrait;
	bIsAvailable = HeroInfo.bIsAvailable;
	bIsSelected = HeroInfo.bIsSelected;
	bIsLocked = HeroInfo.bIsLocked;
	OwningPlayerIndex = HeroInfo.SelectedByPlayerIndex;

	RefreshInteractability();
	RefreshVisualPresentation();

	// Notify Blueprint hooks
	OnHeroDataSet(HeroPortrait, HeroDisplayName);
	OnAvailabilityChanged(bIsAvailable);
	OnSelectionStateChanged(bIsSelected);
	OnLockedStateChanged(bIsLocked);

	FText PlayerLabel;
	if (OwningPlayerIndex >= 0)
	{
		PlayerLabel = FText::Format(NSLOCTEXT("HeroSlot", "PlayerIndicator", "P{0}"), FText::AsNumber(OwningPlayerIndex + 1));
	}
	OnPlayerIndicatorChanged(OwningPlayerIndex, PlayerLabel);
}

void UBwayHeroSlotWidget::SetSelectionState(bool bNewSelectedState)
{
	if (bIsSelected != bNewSelectedState)
	{
		bIsSelected = bNewSelectedState;
		RefreshVisualPresentation();
		OnSelectionStateChanged(bIsSelected);
	}
}

void UBwayHeroSlotWidget::SetInspectedState(bool bNewInspectedState)
{
	if (bIsInspected != bNewInspectedState)
	{
		bIsInspected = bNewInspectedState;
		RefreshVisualPresentation();
	}
}

void UBwayHeroSlotWidget::SetPlayerIndicator(int32 PlayerIndex)
{
	OwningPlayerIndex = PlayerIndex;

	FText PlayerLabel;
	if (PlayerIndex >= 0)
	{
		PlayerLabel = FText::Format(NSLOCTEXT("HeroSlot", "PlayerIndicator", "P{0}"), FText::AsNumber(PlayerIndex + 1));
	}

	RefreshVisualPresentation();
	OnPlayerIndicatorChanged(PlayerIndex, PlayerLabel);
}

void UBwayHeroSlotWidget::SetAvailability(bool bAvailable)
{
	if (bIsAvailable != bAvailable)
	{
		bIsAvailable = bAvailable;
		RefreshInteractability();
		RefreshVisualPresentation();
		OnAvailabilityChanged(bIsAvailable);
	}
}

void UBwayHeroSlotWidget::SetLockedState(bool bInLocked)
{
	if (bIsLocked != bInLocked)
	{
		bIsLocked = bInLocked;
		RefreshInteractability();
		RefreshVisualPresentation();
		OnLockedStateChanged(bIsLocked);
	}
}

void UBwayHeroSlotWidget::SetLocalPhaseLockedState(bool bInLocalPhaseLocked)
{
	if (bLocalPhaseLocked != bInLocalPhaseLocked)
	{
		bLocalPhaseLocked = bInLocalPhaseLocked;
		RefreshInteractability();
		RefreshVisualPresentation();
	}
}

bool UBwayHeroSlotWidget::SimulateClick()
{
	if (!GetIsEnabled() || !IsInteractionEnabled() || bLocalPhaseLocked || !bIsAvailable || bIsLocked)
	{
		return false;
	}
	NativeOnClicked();
	return true;
}

FText UBwayHeroSlotWidget::GetStatusBadgeText() const
{
	return StatusBadgeText ? StatusBadgeText->GetText() : FText::GetEmpty();
}

void UBwayHeroSlotWidget::RefreshInteractability()
{
	// Keep unlocked grid cards focusable so controller D-pad traversal across the 4-hero row
	// never traps behind a teammate-claimed middle slot; disable interaction once local player locks.
	const bool bAllowGridInteraction = !bLocalPhaseLocked;
	SetIsEnabled(bAllowGridInteraction);
	SetIsInteractionEnabled(bAllowGridInteraction);
}

void UBwayHeroSlotWidget::RefreshVisualPresentation()
{
	const FLinearColor ClassAccent = ResolveClassAccentColor(HeroClassName, HeroClass);

	if (HeroNameText)
	{
		HeroNameText->SetText(FText::FromString(HeroDisplayName.ToString().ToUpper()));
		HeroNameText->SetColorAndOpacity(FSlateColor(
			(!bIsAvailable && !bIsSelected) ? FLinearColor(0.55f, 0.60f, 0.68f, 0.90f) : FLinearColor::White));
	}

	if (ClassBadgeText)
	{
		ClassBadgeText->SetText(FText::FromString(HeroClassName.ToString().ToUpper()));
	}
	if (ClassBadgeBorder)
	{
		ClassBadgeBorder->SetBrushColor(FLinearColor(ClassAccent.R * 0.45f, ClassAccent.G * 0.45f, ClassAccent.B * 0.45f, 0.95f));
	}

	if (StatusBadgeText)
	{
		if (bIsLocked && bIsSelected)
		{
			StatusBadgeText->SetText(NSLOCTEXT("HeroSlot", "StatusLocked", "LOCKED"));
			StatusBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.28f, 0.94f, 0.52f, 1.0f)));
		}
		else if (bIsSelected)
		{
			StatusBadgeText->SetText(OwningPlayerIndex >= 0
				? FText::FromString(FString::Printf(TEXT("P%d"), OwningPlayerIndex + 1))
				: NSLOCTEXT("HeroSlot", "StatusSelected", "PICKED"));
			StatusBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.84f, 0.32f, 1.0f)));
		}
		else if (!bIsAvailable)
		{
			StatusBadgeText->SetText(OwningPlayerIndex >= 0
				? FText::FromString(FString::Printf(TEXT("TAKEN P%d"), OwningPlayerIndex + 1))
				: NSLOCTEXT("HeroSlot", "StatusTaken", "TAKEN"));
			StatusBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.42f, 0.38f, 0.95f)));
		}
		else if (OwningPlayerIndex >= 0)
		{
			StatusBadgeText->SetText(FText::FromString(FString::Printf(TEXT("P%d"), OwningPlayerIndex + 1)));
			StatusBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.84f, 0.95f, 0.95f)));
		}
		else
		{
			StatusBadgeText->SetText(FText::GetEmpty());
		}
	}

	if (PortraitBackdropBorder)
	{
		PortraitBackdropBorder->SetBrushColor(FLinearColor(ClassAccent.R * 0.22f, ClassAccent.G * 0.22f, ClassAccent.B * 0.22f, 0.92f));
	}

	if (PortraitImage)
	{
		if (HeroPortrait)
		{
			PortraitImage->SetBrushFromTexture(HeroPortrait, false);
			PortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			const float Opacity = (!bIsAvailable && !bIsSelected) ? 0.38f : 1.0f;
			PortraitImage->SetColorAndOpacity(FLinearColor(Opacity, Opacity, Opacity, 1.0f));
		}
		else
		{
			PortraitImage->SetBrushTintColor(FSlateColor(ClassAccent));
			PortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}

	if (OuterFocusBorder && InnerCardBorder)
	{
		const bool bHovered = IsHovered();
		FLinearColor RingColor(0.18f, 0.28f, 0.42f, 0.85f);
		FLinearColor CardBg(0.04f, 0.08f, 0.14f, 0.94f);

		if (bIsLocked && bIsSelected)
		{
			RingColor = FLinearColor(0.24f, 0.88f, 0.48f, 1.0f);
			CardBg = FLinearColor(0.06f, 0.18f, 0.13f, 0.97f);
		}
		else if (bHasFocusVisual)
		{
			RingColor = FLinearColor(0.98f, 0.84f, 0.30f, 1.0f);
			CardBg = FLinearColor(0.10f, 0.20f, 0.34f, 0.98f);
		}
		else if (bIsSelected)
		{
			RingColor = FLinearColor(0.95f, 0.76f, 0.26f, 1.0f);
			CardBg = FLinearColor(0.09f, 0.16f, 0.28f, 0.97f);
		}
		else if (bHovered || bIsInspected)
		{
			RingColor = FLinearColor(0.24f, 0.78f, 0.96f, 0.98f);
			CardBg = FLinearColor(0.07f, 0.14f, 0.24f, 0.96f);
		}
		else if (!bIsAvailable)
		{
			RingColor = FLinearColor(0.32f, 0.18f, 0.18f, 0.70f);
			CardBg = FLinearColor(0.04f, 0.05f, 0.08f, 0.80f);
		}
		else if (bLocalPhaseLocked)
		{
			RingColor = FLinearColor(0.14f, 0.18f, 0.24f, 0.60f);
			CardBg = FLinearColor(0.03f, 0.05f, 0.09f, 0.75f);
		}

		OuterFocusBorder->SetBrushColor(RingColor);
		InnerCardBorder->SetBrushColor(CardBg);
	}
}

void UBwayHeroSlotWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (HeroId.IsValid())
	{
		OnHeroSlotFocusedOrHovered.Broadcast(HeroId);
		if (!bLocalPhaseLocked && bIsAvailable && !bIsLocked)
		{
			OnHeroSlotClicked.Broadcast(HeroId);
		}
	}
}

void UBwayHeroSlotWidget::NativeOnHovered()
{
	Super::NativeOnHovered();
	RefreshVisualPresentation();
	OnSlotHovered();
	if (HeroId.IsValid())
	{
		OnHeroSlotFocusedOrHovered.Broadcast(HeroId);
	}
}

void UBwayHeroSlotWidget::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	RefreshVisualPresentation();
	OnSlotUnhovered();
}

void UBwayHeroSlotWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	bHasFocusVisual = true;
	RefreshVisualPresentation();
	if (HeroId.IsValid())
	{
		OnHeroSlotFocusedOrHovered.Broadcast(HeroId);
	}
}

void UBwayHeroSlotWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	bHasFocusVisual = false;
	RefreshVisualPresentation();
}
