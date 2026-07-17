// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCurrencyDisplayWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
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

	SetAmount(CurrentAmount);
	SetCurrencyIcon(CurrencyIcon);
	TryBindToCurrentGold();
}

void UBwayCurrencyDisplayWidget::NativeDestruct()
{
	UnbindFromCurrentGold();
	Super::NativeDestruct();
}

void UBwayCurrencyDisplayWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!BoundAbilitySystemComponent.IsValid())
	{
		TimeSinceLastBindingAttempt += InDeltaTime;
		if (TimeSinceLastBindingAttempt >= BindingRetryInterval)
		{
			TimeSinceLastBindingAttempt = 0.0f;
			TryBindToCurrentGold();
		}
	}
}

void UBwayCurrencyDisplayWidget::TryBindToCurrentGold()
{
	APlayerState* PlayerState = GetOwningPlayerState();
	UAbilitySystemComponent* ASC = PlayerState ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState) : nullptr;
	if (!ASC)
	{
		return;
	}

	if (BoundAbilitySystemComponent.Get() == ASC && CurrentGoldChangedHandle.IsValid())
	{
		return;
	}

	UnbindFromCurrentGold();
	BoundAbilitySystemComponent = ASC;
	CurrentGoldChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute())
		.AddUObject(this, &UBwayCurrencyDisplayWidget::HandleCurrentGoldChanged);

	const float CurrentGold = ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute());
	SetAmount(FMath::RoundToInt(CurrentGold));
}

void UBwayCurrencyDisplayWidget::UnbindFromCurrentGold()
{
	if (BoundAbilitySystemComponent.IsValid() && CurrentGoldChangedHandle.IsValid())
	{
		BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBwayGoldAttributeSet::GetCurrentGoldAttribute())
			.Remove(CurrentGoldChangedHandle);
	}

	CurrentGoldChangedHandle.Reset();
	BoundAbilitySystemComponent.Reset();
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

	if (Text_Amount)
	{
		Text_Amount->SetText(FormattedText);
	}

	OnAmountChanged(CurrentAmount, FormattedText);
}

void UBwayCurrencyDisplayWidget::SetCurrencyIcon(UTexture2D* Icon)
{
	CurrencyIcon = Icon;

	if (Image_CurrencyIcon)
	{
		Image_CurrencyIcon->SetBrushFromTexture(CurrencyIcon);
		Image_CurrencyIcon->SetVisibility(
			CurrencyIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

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

