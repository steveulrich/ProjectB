// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayStyledButtonWidget.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayStyledButtonWidget)

UBwayStyledButtonWidget::UBwayStyledButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayStyledButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Apply initial values in editor
	OnButtonTextChanged(ButtonText);
	OnButtonStyleChanged(CurrentStyle);
}

void UBwayStyledButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Apply values at runtime
	OnButtonTextChanged(ButtonText);
	OnButtonStyleChanged(CurrentStyle);
}

void UBwayStyledButtonWidget::SetButtonText(const FText& InText)
{
	ButtonText = InText;
	OnButtonTextChanged(ButtonText);
}

void UBwayStyledButtonWidget::SetButtonStyle(EBwayButtonStyle InStyle)
{
	if (CurrentStyle != InStyle)
	{
		CurrentStyle = InStyle;
		OnButtonStyleChanged(CurrentStyle);
	}
}

void UBwayStyledButtonWidget::SetIsLoading(bool bInIsLoading)
{
	if (bIsLoading != bInIsLoading)
	{
		bIsLoading = bInIsLoading;
		OnLoadingStateChanged(bIsLoading);

		// Disable interaction while loading
		SetIsInteractionEnabled(!bIsLoading);
	}
}

void UBwayStyledButtonWidget::SetButtonIcon(UTexture2D* Icon)
{
	ButtonIcon = Icon;
	OnButtonIconChanged(ButtonIcon);
}

