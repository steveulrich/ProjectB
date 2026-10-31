// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayTeamPortraitSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/BwayMatchHUDWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayTeamPortraitSlotWidget)

#define LOCTEXT_NAMESPACE "BwayTeamPortraitSlotWidget"

void UBwayTeamPortraitSlotWidget::SetSlotViewModel(const FBwayTeamPortraitSlotViewModel& InViewModel)
{
	ViewModel = InViewModel;
	ApplyViewModel();
}

void UBwayTeamPortraitSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	EnsureRespawnTimerWidget();
	ApplyViewModel();
}

void UBwayTeamPortraitSlotWidget::EnsureRespawnTimerWidget()
{
	if (!WidgetTree)
	{
		return;
	}

	USizeBox* RootSizeBox = WidgetTree->FindWidget<USizeBox>(TEXT("RootSizeBox"));
	if (!RootSizeBox)
	{
		RootSizeBox = Cast<USizeBox>(WidgetTree->RootWidget);
	}
	if (RootSizeBox)
	{
		RootSizeBox->SetWidthOverride(72.0f);
		RootSizeBox->SetHeightOverride(80.0f);
	}

	UOverlay* SlotOverlay = WidgetTree->FindWidget<UOverlay>(TEXT("SlotOverlay"));
	if (!SlotOverlay && Frame)
	{
		SlotOverlay = Cast<UOverlay>(Frame->GetContent());
	}
	if (SlotOverlay)
	{
		if (USizeBoxSlot* RootSlot = Cast<USizeBoxSlot>(SlotOverlay->Slot))
		{
			RootSlot->SetHorizontalAlignment(HAlign_Fill);
			RootSlot->SetVerticalAlignment(VAlign_Fill);
			// Reserve bottom 8px below the 72x72 portrait square for the 5px per-player mini health bar.
			RootSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}
	}

	if (Frame)
	{
		if (UOverlaySlot* FrameSlot = Cast<UOverlaySlot>(Frame->Slot))
		{
			FrameSlot->SetHorizontalAlignment(HAlign_Fill);
			FrameSlot->SetVerticalAlignment(VAlign_Fill);
			FrameSlot->SetPadding(FMargin(0.0f));
		}
	}

	if (Image_Portrait)
	{
		if (UOverlaySlot* PortraitSlot = Cast<UOverlaySlot>(Image_Portrait->Slot))
		{
			PortraitSlot->SetHorizontalAlignment(HAlign_Fill);
			PortraitSlot->SetVerticalAlignment(VAlign_Fill);
			PortraitSlot->SetPadding(FMargin(2.0f));
		}
	}

	if (Image_DeathOverlay)
	{
		if (UOverlaySlot* DeathSlot = Cast<UOverlaySlot>(Image_DeathOverlay->Slot))
		{
			DeathSlot->SetHorizontalAlignment(HAlign_Fill);
			DeathSlot->SetVerticalAlignment(VAlign_Fill);
			DeathSlot->SetPadding(FMargin(2.0f));
		}
	}

	if (Text_RelicBadge)
	{
		if (UOverlaySlot* BadgeSlot = Cast<UOverlaySlot>(Text_RelicBadge->Slot))
		{
			BadgeSlot->SetHorizontalAlignment(HAlign_Center);
			BadgeSlot->SetVerticalAlignment(VAlign_Bottom);
			BadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}
		FSlateFontInfo BadgeFont = UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(10, 0, 1);
		Text_RelicBadge->SetFont(BadgeFont);
		Text_RelicBadge->SetJustification(ETextJustify::Center);
		// Broadcast HUD frames Relic carriers via the gold portrait border without text overlap.
		Text_RelicBadge->SetRenderOpacity(0.0f);
	}

	if (!Text_RespawnTimer && SlotOverlay)
	{
		Text_RespawnTimer = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RespawnTimer"));
		if (Text_RespawnTimer)
		{
			Text_RespawnTimer->SetVisibility(ESlateVisibility::Collapsed);
			if (UOverlaySlot* OverlaySlot = SlotOverlay->AddChildToOverlay(Text_RespawnTimer))
			{
				OverlaySlot->SetHorizontalAlignment(HAlign_Center);
				OverlaySlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}

	if (Text_RespawnTimer)
	{
		const FSlateFontInfo TimerFont = UBwayMatchHUDWidgetBase::GetBroadcastCondensedFont(28, 0, 2);
		Text_RespawnTimer->SetFont(TimerFont);
		Text_RespawnTimer->SetColorAndOpacity(FSlateColor(FLinearColor(0.97f, 0.75f, 0.20f, 1.0f)));
		Text_RespawnTimer->SetShadowOffset(FVector2D(1.5f, 1.5f));
		Text_RespawnTimer->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f));
		Text_RespawnTimer->SetJustification(ETextJustify::Center);
	}
}

void UBwayTeamPortraitSlotWidget::ApplyViewModel()
{
	EnsureRespawnTimerWidget();

	if (Frame)
	{
		if (Frame->Background.DrawAs == ESlateBrushDrawType::NoDrawType)
		{
			Frame->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		}
		const FLinearColor EffectiveFrameColor = (ViewModel.bIsOccupied && ViewModel.bHasRelic)
			? RelicCarrierFrameColor
			: ViewModel.FrameColor;
		Frame->SetBrushColor(EffectiveFrameColor);
	}

	if (Image_Portrait)
	{
		UTexture2D* Texture = ViewModel.PortraitTexture
			? ViewModel.PortraitTexture.Get()
			: EmptyPortraitTexture.Get();
		if (Texture)
		{
			Image_Portrait->SetBrushFromTexture(Texture, false);
		}
		else
		{
			Image_Portrait->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		}

		if (!ViewModel.bIsOccupied)
		{
			Image_Portrait->SetColorAndOpacity(EmptyPortraitTint);
		}
		else
		{
			Image_Portrait->SetColorAndOpacity(
				ViewModel.bIsAlive ? FLinearColor::White : DeadPortraitTint);
		}

		Image_Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (Image_DeathOverlay)
	{
		if (Image_DeathOverlay->GetBrush().GetResourceObject() == nullptr)
		{
			Image_DeathOverlay->SetBrush(*FCoreStyle::Get().GetBrush("WhiteBrush"));
		}
		Image_DeathOverlay->SetColorAndOpacity(FLinearColor(0.02f, 0.02f, 0.05f, 0.74f));

		const bool bShowDeath = ViewModel.bIsOccupied && !ViewModel.bIsAlive;
		Image_DeathOverlay->SetVisibility(
			bShowDeath ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Text_RespawnTimer)
	{
		const bool bShowRespawnCountdown = ViewModel.bIsOccupied
			&& !ViewModel.bIsAlive
			&& ViewModel.bIsWaitingToRespawn
			&& ViewModel.RespawnRemainingSeconds > 0;
		if (bShowRespawnCountdown)
		{
			Text_RespawnTimer->SetText(FText::AsNumber(ViewModel.RespawnRemainingSeconds));
			Text_RespawnTimer->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_RespawnTimer->SetText(FText::GetEmpty());
			Text_RespawnTimer->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Text_RelicBadge)
	{
		Text_RelicBadge->SetText(LOCTEXT("RelicBadge", "RELIC"));
		Text_RelicBadge->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.20f, 1.0f)));
		Text_RelicBadge->SetVisibility(
			(ViewModel.bIsOccupied && ViewModel.bHasRelic)
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	InvalidateLayoutAndVolatility();
	BP_OnSlotViewModelChanged(ViewModel);
}

int32 UBwayTeamPortraitSlotWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	int32 CurrentLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (!ViewModel.bIsOccupied || Size.X <= 2.0f || Size.Y <= 10.0f)
	{
		return CurrentLayer;
	}

	const double BarY = Size.Y - 6.0;
	const double BarH = 5.0;

	// Dark recessed mini-health track directly beneath the 72x72 portrait square
	UBwayMatchHUDWidgetBase::DrawSolidBox(
		AllottedGeometry,
		OutDrawElements,
		++CurrentLayer,
		FVector2D(0.0, BarY),
		FVector2D(Size.X, BarH),
		FLinearColor(0.03f, 0.06f, 0.09f, 0.94f));

	if (ViewModel.bIsAlive && ViewModel.HealthPercent > KINDA_SMALL_NUMBER)
	{
		const double FillW = Size.X * FMath::Clamp(static_cast<double>(ViewModel.HealthPercent), 0.0, 1.0);
		const FLinearColor FillColor = ViewModel.bIsFriendly
			? FLinearColor(0.09f, 0.91f, 0.96f, 1.0f)
			: FLinearColor(0.91f, 0.19f, 0.18f, 1.0f);

		UBwayMatchHUDWidgetBase::DrawSolidBox(
			AllottedGeometry,
			OutDrawElements,
			++CurrentLayer,
			FVector2D(0.0, BarY),
			FVector2D(FillW, BarH),
			FillColor);
	}

	return CurrentLayer;
}

FText UBwayTeamPortraitSlotWidget::GetDisplayedRespawnText() const
{
	return Text_RespawnTimer ? Text_RespawnTimer->GetText() : FText::GetEmpty();
}

bool UBwayTeamPortraitSlotWidget::IsRespawnTimerVisible() const
{
	return Text_RespawnTimer && Text_RespawnTimer->GetVisibility() != ESlateVisibility::Collapsed
		&& Text_RespawnTimer->GetVisibility() != ESlateVisibility::Hidden;
}

bool UBwayTeamPortraitSlotWidget::IsDeathOverlayVisible() const
{
	return Image_DeathOverlay && Image_DeathOverlay->GetVisibility() != ESlateVisibility::Collapsed
		&& Image_DeathOverlay->GetVisibility() != ESlateVisibility::Hidden;
}

bool UBwayTeamPortraitSlotWidget::IsRelicBadgeVisible() const
{
	return Text_RelicBadge && Text_RelicBadge->GetVisibility() != ESlateVisibility::Collapsed
		&& Text_RelicBadge->GetVisibility() != ESlateVisibility::Hidden;
}

#undef LOCTEXT_NAMESPACE
