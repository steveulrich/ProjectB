// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayHeroSlotWidget.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroSlotWidget)

UBwayHeroSlotWidget::UBwayHeroSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayHeroSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBwayHeroSlotWidget::SetHeroData(const FHeroDisplayInfo& HeroInfo)
{
	HeroId = HeroInfo.HeroId;
	HeroDisplayName = HeroInfo.DisplayName;
	HeroPortrait = HeroInfo.Portrait;
	bIsAvailable = HeroInfo.bIsAvailable;
	bIsSelected = HeroInfo.bIsSelected;
	bIsLocked = HeroInfo.bIsLocked;

	// Notify Blueprint
	OnHeroDataSet(HeroPortrait, HeroDisplayName);
	OnAvailabilityChanged(bIsAvailable);
	OnSelectionStateChanged(bIsSelected);
	OnLockedStateChanged(bIsLocked);
}

void UBwayHeroSlotWidget::SetSelectionState(bool bNewSelectedState)
{
	if (bIsSelected != bNewSelectedState)
	{
		bIsSelected = bNewSelectedState;
		OnSelectionStateChanged(bIsSelected);
	}
}

void UBwayHeroSlotWidget::SetPlayerIndicator(int32 PlayerIndex)
{
	OwningPlayerIndex = PlayerIndex;

	FText PlayerLabel;
	if (PlayerIndex >= 0)
	{
		// Create label like "P1", "P2", etc.
		PlayerLabel = FText::Format(NSLOCTEXT("HeroSlot", "PlayerIndicator", "P{0}"), FText::AsNumber(PlayerIndex + 1));
	}

	OnPlayerIndicatorChanged(PlayerIndex, PlayerLabel);
}

void UBwayHeroSlotWidget::SetAvailability(bool bAvailable)
{
	if (bIsAvailable != bAvailable)
	{
		bIsAvailable = bAvailable;
		OnAvailabilityChanged(bIsAvailable);

		// Update interactability based on availability
		SetIsInteractionEnabled(bIsAvailable && !bIsLocked);
	}
}

void UBwayHeroSlotWidget::SetLockedState(bool bInLocked)
{
	if (bIsLocked != bInLocked)
	{
		bIsLocked = bInLocked;
		OnLockedStateChanged(bIsLocked);

		// Update interactability
		SetIsInteractionEnabled(bIsAvailable && !bIsLocked);
	}
}

void UBwayHeroSlotWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (HeroId.IsValid() && bIsAvailable && !bIsLocked)
	{
		OnHeroSlotClicked.Broadcast(HeroId);
	}
}

void UBwayHeroSlotWidget::NativeOnHovered()
{
	Super::NativeOnHovered();
	OnSlotHovered();
}

void UBwayHeroSlotWidget::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	OnSlotUnhovered();
}

