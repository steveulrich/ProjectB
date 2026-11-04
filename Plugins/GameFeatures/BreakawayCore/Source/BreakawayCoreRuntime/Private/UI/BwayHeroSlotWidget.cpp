// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayHeroSlotWidget.h"
#include "UI/BwayCommonButton.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

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

	UTexture2D* ResolveCharSelectPortraitForHero(const FPrimaryAssetId& InHeroId, const FText& InDisplayName, UTexture2D* FallbackPortrait)
	{
		const FString Key = (InHeroId.PrimaryAssetName.ToString() + TEXT(" ") + InDisplayName.ToString()).ToLower();
		const TCHAR* TexPath = nullptr;
		if (Key.Contains(TEXT("spartacus")) || Key.Contains(TEXT("argus")) || Key.Contains(TEXT("magnus")))
		{
			TexPath = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselectportraitmagnus.charselectportraitmagnus");
		}
		else if (Key.Contains(TEXT("rawlins")))
		{
			TexPath = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselectportraitrawlins.charselectportraitrawlins");
		}
		else if (Key.Contains(TEXT("alona")) || Key.Contains(TEXT("sunpriestess")))
		{
			TexPath = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselectportraitsunpriestess.charselectportraitsunpriestess");
		}
		else if (Key.Contains(TEXT("korryn")) || Key.Contains(TEXT("morgan")))
		{
			TexPath = TEXT("/BreakawayCore/UI/Art/HeroSelect/charselectportraitmorganlefay.charselectportraitmorganlefay");
		}

		if (TexPath)
		{
			if (UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, TexPath))
			{
				return Loaded;
			}
		}
		return FallbackPortrait;
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

	if (UWidget* ExistingCellCanvas = WidgetTree->FindWidget(TEXT("SlotCellCanvas")))
	{
		(void)ExistingCellCanvas;
		NativeOuterFocusBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("SlotOuterFocusBorder")));
		NativeInnerCardBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("SlotInnerCardBorder")));
		NativePortraitBackdropBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("SlotPortraitBackdrop")));
		NativePortraitImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("SlotPortraitImage")));
		NativeStatusBadgeText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("SlotStatusBadgeText")));
		NativeClassBadgeBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("SlotClassBadgeBorder")));
		NativeClassBadgeText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("SlotClassBadgeText")));
		NativeHeroNameText = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("SlotHeroNameText")));
		bUsingNativeVisualTree = (NativeOuterFocusBorder && NativeInnerCardBorder && NativePortraitImage);
		if (bUsingNativeVisualTree)
		{
			RefreshVisualPresentation();
			return;
		}
	}

	if (WidgetTree->RootWidget)
	{
		TArray<UWidget*> OldWidgets;
		WidgetTree->GetAllWidgets(OldWidgets);
		for (UWidget* OldW : OldWidgets)
		{
			if (OldW)
			{
				WidgetTree->RemoveWidget(OldW);
			}
		}
		WidgetTree->RootWidget = nullptr;
	}

	bUsingNativeVisualTree = true;

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotCardSize"));
	CardSize->SetWidthOverride(97.f);
	CardSize->SetHeightOverride(143.f);
	WidgetTree->RootWidget = CardSize;

	NativeOuterFocusBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotOuterFocusBorder"));
	NativeOuterFocusBorder->SetPadding(FMargin(1.0f));
	CardSize->AddChild(NativeOuterFocusBorder);

	NativeInnerCardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotInnerCardBorder"));
	NativeInnerCardBorder->SetPadding(FMargin(0.f));
	NativeInnerCardBorder->SetClipping(EWidgetClipping::ClipToBounds);
	NativeOuterFocusBorder->SetContent(NativeInnerCardBorder);

	UCanvasPanel* CellCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SlotCellCanvas"));
	CellCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	NativeInnerCardBorder->SetContent(CellCanvas);

	// 1. Pre-counter-sheared (-15 deg baked) Character Select Portrait (97x143 filling the 97x143 skewed cell cleanly)
	NativePortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SlotPortraitImage"));
	NativePortraitImage->SetDesiredSizeOverride(FVector2D(97.f, 143.f));
	NativePortraitImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	NativePortraitImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D::ZeroVector, 0.0f));
	if (UCanvasPanelSlot* ImgSlot = CellCanvas->AddChildToCanvas(NativePortraitImage))
	{
		ImgSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		ImgSlot->SetAlignment(FVector2D(0.f, 0.f));
		ImgSlot->SetPosition(FVector2D(0.f, 0.f));
		ImgSlot->SetSize(FVector2D(97.f, 143.f));
		ImgSlot->SetZOrder(1);
	}

	// 2. Top-Left 'P1' Selection Indicator Badge (26x22 at top-left corner, #EBEBEB border, #343434 inner fill)
	NativePortraitBackdropBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotPortraitBackdrop"));
	NativePortraitBackdropBorder->SetPadding(FMargin(0.f, 0.f, 2.f, 2.f));
	NativePortraitBackdropBorder->SetBrushColor(FLinearColor::FromSRGBColor(FColor(235, 235, 235, 255)));
	if (UCanvasPanelSlot* BadgeSlot = CellCanvas->AddChildToCanvas(NativePortraitBackdropBorder))
	{
		BadgeSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		BadgeSlot->SetAlignment(FVector2D(0.f, 0.f));
		BadgeSlot->SetPosition(FVector2D(0.f, 0.f));
		BadgeSlot->SetSize(FVector2D(26.f, 22.f));
		BadgeSlot->SetZOrder(3);
	}

	UBorder* BadgeInnerFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotPlayerBadgeInner"));
	BadgeInnerFill->SetPadding(FMargin(2.f, 1.f));
	BadgeInnerFill->SetBrushColor(FLinearColor::FromSRGBColor(FColor(52, 52, 52, 255)));
	NativePortraitBackdropBorder->SetContent(BadgeInnerFill);

	const UObject* RefrigFont = LoadObject<UFont>(nullptr, TEXT("/BreakawayCore/UI/Fonts/refrigeratordeluxe-heavy_Font.refrigeratordeluxe-heavy_Font"));
	NativeStatusBadgeText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("SlotStatusBadgeText"));
	{
		FSlateFontInfo Font = RefrigFont ? FSlateFontInfo(RefrigFont, 12) : FCoreStyle::GetDefaultFontStyle("Bold", 12);
		if (RefrigFont)
		{
			Font.TypefaceFontName = FName(TEXT("Default"));
		}
		NativeStatusBadgeText->SetFont(Font);
		NativeStatusBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		NativeStatusBadgeText->SetJustification(ETextJustify::Center);
		NativeStatusBadgeText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		NativeStatusBadgeText->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D(-15.0f, 0.0f), 0.0f));
	}
	BadgeInnerFill->SetContent(NativeStatusBadgeText);

	// 3. Collapsed metadata widgets for data-binding & test contracts
	UVerticalBox* HiddenMetaBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotHiddenMetaBox"));
	HiddenMetaBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* MetaSlot = CellCanvas->AddChildToCanvas(HiddenMetaBox))
	{
		MetaSlot->SetPosition(FVector2D(0.f, 0.f));
		MetaSlot->SetSize(FVector2D(1.f, 1.f));
		MetaSlot->SetZOrder(0);
	}

	NativeClassBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotClassBadgeBorder"));
	NativeClassBadgeText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("SlotClassBadgeText"));
	NativeClassBadgeBorder->SetContent(NativeClassBadgeText);
	HiddenMetaBox->AddChildToVerticalBox(NativeClassBadgeBorder);

	NativeHeroNameText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("SlotHeroNameText"));
	HiddenMetaBox->AddChildToVerticalBox(NativeHeroNameText);

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
	return NativeStatusBadgeText ? NativeStatusBadgeText->GetText() : FText::GetEmpty();
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
	if (!bUsingNativeVisualTree)
	{
		return;
	}

	const FLinearColor ClassAccent = ResolveClassAccentColor(HeroClassName, HeroClass);

	if (NativeHeroNameText)
	{
		NativeHeroNameText->SetText(FText::FromString(HeroDisplayName.ToString().ToUpper()));
		NativeHeroNameText->SetColorAndOpacity(FSlateColor(
			(!bIsAvailable && !bIsSelected) ? FLinearColor(0.55f, 0.60f, 0.68f, 0.90f) : FLinearColor::White));
	}

	if (NativeClassBadgeText)
	{
		NativeClassBadgeText->SetText(FText::FromString(HeroClassName.ToString().ToUpper()));
	}
	if (NativeClassBadgeBorder)
	{
		NativeClassBadgeBorder->SetBrushColor(FLinearColor(ClassAccent.R * 0.45f, ClassAccent.G * 0.45f, ClassAccent.B * 0.45f, 0.95f));
	}

	const bool bShowPlayerBadge = bIsSelected || bIsInspected || (OwningPlayerIndex >= 0);
	if (NativePortraitBackdropBorder)
	{
		NativePortraitBackdropBorder->SetVisibility(bShowPlayerBadge ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		NativePortraitBackdropBorder->SetBrushColor(FLinearColor::FromSRGBColor(FColor(235, 235, 235, 255)));
	}

	if (NativeStatusBadgeText)
	{
		if (bShowPlayerBadge)
		{
			const int32 BadgeIndex = (OwningPlayerIndex >= 0) ? (OwningPlayerIndex + 1) : 1;
			NativeStatusBadgeText->SetText(FText::FromString(FString::Printf(TEXT("P%d"), BadgeIndex)));
			NativeStatusBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
		else if (!bIsAvailable)
		{
			NativeStatusBadgeText->SetText(NSLOCTEXT("HeroSlot", "StatusTaken", "TAKEN"));
			NativeStatusBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.42f, 0.38f, 0.95f)));
		}
		else
		{
			NativeStatusBadgeText->SetText(FText::GetEmpty());
		}
	}

	if (NativePortraitImage)
	{
		NativePortraitImage->SetDesiredSizeOverride(FVector2D(97.f, 143.f));
		NativePortraitImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D::UnitVector, FVector2D::ZeroVector, 0.0f));
		if (UCanvasPanelSlot* ImgSlot = Cast<UCanvasPanelSlot>(NativePortraitImage->Slot))
		{
			ImgSlot->SetPosition(FVector2D(0.f, 0.f));
			ImgSlot->SetSize(FVector2D(97.f, 143.f));
		}

		UTexture2D* DisplayTex = ResolveCharSelectPortraitForHero(HeroId, HeroDisplayName, HeroPortrait);
		if (DisplayTex)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(DisplayTex);
			Brush.ImageSize = FVector2D(97.f, 143.f);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			NativePortraitImage->SetBrush(Brush);
			NativePortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			const float Opacity = (!bIsAvailable && !bIsSelected) ? 0.38f : 1.0f;
			NativePortraitImage->SetColorAndOpacity(FLinearColor(Opacity, Opacity, Opacity, 1.0f));
		}
		else
		{
			NativePortraitImage->SetBrushTintColor(FSlateColor(ClassAccent));
			NativePortraitImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}

	if (NativeOuterFocusBorder && NativeInnerCardBorder)
	{
		const bool bHovered = IsHovered();
		FLinearColor RingColor(1.0f, 1.0f, 1.0f, 0.06f);
		FLinearColor CardBg(0.005f, 0.006f, 0.008f, 0.78f);
		float BorderPad = 1.0f;

		if (bIsSelected || bIsInspected || bHasFocusVisual)
		{
			RingColor = FLinearColor::FromSRGBColor(FColor(235, 235, 235, 255));
			BorderPad = 3.0f;
		}
		else if (bHovered)
		{
			RingColor = FLinearColor::FromSRGBColor(FColor(255, 162, 0, 255));
			BorderPad = 2.0f;
		}
		else if (!bIsAvailable)
		{
			RingColor = FLinearColor(0.32f, 0.18f, 0.18f, 0.70f);
		}

		NativeOuterFocusBorder->SetPadding(FMargin(BorderPad));
		NativeOuterFocusBorder->SetBrushColor(RingColor);
		NativeInnerCardBorder->SetBrushColor(CardBg);
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
