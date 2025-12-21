// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayAbilityIconWidget.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAbilityIconWidget)

UBwayAbilityIconWidget::UBwayAbilityIconWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayAbilityIconWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBwayAbilityIconWidget::SetAbilityData(const FAbilityDisplayInfo& InAbilityInfo, int32 InAbilityIndex)
{
	AbilityInfo = InAbilityInfo;
	AbilityIndex = InAbilityIndex;

	// Load the icon texture synchronously
	LoadedIconTexture = AbilityInfo.Icon.LoadSynchronous();

	// Notify Blueprint
	OnAbilityDataSet(LoadedIconTexture, AbilityInfo.AbilityName, AbilityInfo.bIsUltimate);
}

void UBwayAbilityIconWidget::SetAbilityDataDirect(UTexture2D* Icon, const FText& Name, const FText& Description, int32 InAbilityIndex)
{
	AbilityInfo.AbilityName = Name;
	AbilityInfo.Description = Description;
	AbilityIndex = InAbilityIndex;
	LoadedIconTexture = Icon;

	// Notify Blueprint
	OnAbilityDataSet(LoadedIconTexture, AbilityInfo.AbilityName, AbilityInfo.bIsUltimate);
}

void UBwayAbilityIconWidget::SetSelected(bool bInSelected)
{
	if (bIsSelected != bInSelected)
	{
		bIsSelected = bInSelected;
		OnSelectionStateChanged(bIsSelected);
	}
}

void UBwayAbilityIconWidget::SetLocked(bool bInLocked)
{
	if (bIsLocked != bInLocked)
	{
		bIsLocked = bInLocked;
		OnLockedStateChanged(bIsLocked);

		// Locked abilities shouldn't be interactable
		SetIsInteractionEnabled(!bIsLocked);
	}
}

void UBwayAbilityIconWidget::SetIsCancelSlot(bool bInIsCancelSlot)
{
	if (bCancelSlot != bInIsCancelSlot)
	{
		bCancelSlot = bInIsCancelSlot;
		OnCancelSlotStateChanged(bCancelSlot);
	}
}

void UBwayAbilityIconWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (!bIsLocked)
	{
		OnAbilityIconClicked.Broadcast(AbilityIndex);
	}
}

void UBwayAbilityIconWidget::NativeOnHovered()
{
	Super::NativeOnHovered();
	OnIconHovered();
	OnAbilityIconHovered.Broadcast(AbilityIndex);
}

void UBwayAbilityIconWidget::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	OnIconUnhovered();
}

