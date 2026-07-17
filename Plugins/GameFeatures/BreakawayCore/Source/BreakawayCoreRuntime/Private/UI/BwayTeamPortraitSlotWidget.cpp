// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayTeamPortraitSlotWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

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
	ApplyViewModel();
}

void UBwayTeamPortraitSlotWidget::ApplyViewModel()
{
	if (Frame)
	{
		Frame->SetBrushColor(ViewModel.FrameColor);
	}

	if (Image_Portrait)
	{
		UTexture2D* Texture = ViewModel.PortraitTexture
			? ViewModel.PortraitTexture.Get()
			: EmptyPortraitTexture.Get();
		Image_Portrait->SetBrushFromTexture(Texture);

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
		const bool bShowDeath = ViewModel.bIsOccupied && !ViewModel.bIsAlive;
		Image_DeathOverlay->SetVisibility(
			bShowDeath ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Text_RelicBadge)
	{
		Text_RelicBadge->SetText(LOCTEXT("RelicBadge", "RELIC"));
		Text_RelicBadge->SetVisibility(
			(ViewModel.bIsOccupied && ViewModel.bHasRelic)
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	BP_OnSlotViewModelChanged(ViewModel);
}

#undef LOCTEXT_NAMESPACE
