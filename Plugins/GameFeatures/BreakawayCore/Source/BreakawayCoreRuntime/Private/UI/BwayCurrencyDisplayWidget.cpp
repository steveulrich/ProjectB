// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCurrencyDisplayWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCurrencyDisplayWidget)

UBwayCurrencyDisplayWidget::UBwayCurrencyDisplayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayCurrencyDisplayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	APlayerState* PlayerState = GetOwningPlayerState();
	UAbilitySystemComponent* ASC = PlayerState ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState) : nullptr;
	if (!ASC)
	{
		return;
	}

	BoundAbilitySystemComponent = ASC;
	CurrentGoldChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute())
		.AddUObject(this, &UBwayCurrencyDisplayWidget::HandleCurrentGoldChanged);

	const float CurrentGold = ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute());
	SetAmount(FMath::RoundToInt(CurrentGold));
}

void UBwayCurrencyDisplayWidget::NativeDestruct()
{
	if (BoundAbilitySystemComponent.IsValid() && CurrentGoldChangedHandle.IsValid())
	{
		BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute())
			.Remove(CurrentGoldChangedHandle);
	}

	CurrentGoldChangedHandle.Reset();
	BoundAbilitySystemComponent.Reset();

	Super::NativeDestruct();
}

void UBwayCurrencyDisplayWidget::HandleCurrentGoldChanged(const FOnAttributeChangeData& ChangeData)
{
	SetAmount(FMath::RoundToInt(ChangeData.NewValue));
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

