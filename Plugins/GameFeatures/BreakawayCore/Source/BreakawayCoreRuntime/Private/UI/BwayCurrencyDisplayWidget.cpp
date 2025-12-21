// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCurrencyDisplayWidget.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCurrencyDisplayWidget)

UBwayCurrencyDisplayWidget::UBwayCurrencyDisplayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayCurrencyDisplayWidget::SetAmount(int32 Amount)
{
	CurrentAmount = Amount;

	// Format with thousands separator
	FNumberFormattingOptions Options;
	Options.UseGrouping = true;
	FText FormattedText = FText::AsNumber(Amount, &Options);

	OnAmountChanged(CurrentAmount, FormattedText);
}

void UBwayCurrencyDisplayWidget::SetCurrencyIcon(UTexture2D* Icon)
{
	CurrencyIcon = Icon;
	OnCurrencyIconChanged(CurrencyIcon);
}

void UBwayCurrencyDisplayWidget::SetCanAfford(bool bCanAfford)
{
	if (bPlayerCanAfford != bCanAfford)
	{
		bPlayerCanAfford = bCanAfford;
		OnAffordabilityChanged(bPlayerCanAfford);
	}
}

