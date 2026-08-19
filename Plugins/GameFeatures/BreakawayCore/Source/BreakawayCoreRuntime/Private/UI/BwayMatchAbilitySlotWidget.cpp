#include "UI/BwayMatchAbilitySlotWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchAbilitySlotWidget)

#define LOCTEXT_NAMESPACE "BwayMatchAbilitySlotWidget"

void UBwayMatchAbilitySlotWidget::SetSlotViewModel(const FBwayMatchAbilitySlotViewModel& InViewModel)
{
	ViewModel = InViewModel;
	ApplyViewModel();
}

void UBwayMatchAbilitySlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyViewModel();
}

void UBwayMatchAbilitySlotWidget::ApplyViewModel()
{
	const bool bDisabled = !ViewModel.bIsAvailable || ViewModel.bIsConsumed;
	const bool bOnCooldown = ViewModel.bIsOnCooldown && !ViewModel.bIsConsumed;

	if (Image_Icon)
	{
		UTexture2D* IconTexture = ViewModel.DisplayInfo.Icon.LoadSynchronous();
		Image_Icon->SetBrushFromTexture(IconTexture);

		const FLinearColor IconTint = (bDisabled || bOnCooldown)
			? FLinearColor(0.22f, 0.22f, 0.22f, 1.0f)
			: FLinearColor::White;
		Image_Icon->SetColorAndOpacity(IconTint);
	}

	if (Image_DisabledOverlay)
	{
		Image_DisabledOverlay->SetVisibility(bDisabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Image_CooldownOverlay)
	{
		Image_CooldownOverlay->SetVisibility(bOnCooldown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ProgressBar_Cooldown)
	{
		if (bOnCooldown)
		{
			ProgressBar_Cooldown->SetVisibility(ESlateVisibility::HitTestInvisible);
			ProgressBar_Cooldown->SetPercent(FMath::Clamp(ViewModel.CooldownPercent, 0.f, 1.f));
		}
		else
		{
			ProgressBar_Cooldown->SetPercent(0.f);
			ProgressBar_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (Image_RelicFrame)
	{
		Image_RelicFrame->SetVisibility(ViewModel.bIsRelicAbility ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Text_AbilityName)
	{
		Text_AbilityName->SetText(
			ViewModel.DisplayInfo.AbilityName.IsEmpty()
				? LOCTEXT("UnresolvedAbility", "Unresolved")
				: ViewModel.DisplayInfo.AbilityName);
	}

	if (Text_Keybind)
	{
		Text_Keybind->SetText(ViewModel.KeyLabel);
	}

	if (Text_State)
	{
		FText StateText;
		if (ViewModel.bIsConsumed)
		{
			StateText = LOCTEXT("BuildableUsed", "USED");
		}
		else if (!ViewModel.bIsAvailable)
		{
			StateText = LOCTEXT("AbilityUnavailable", "—");
		}

		Text_State->SetText(StateText);
		Text_State->SetVisibility(StateText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (Text_CooldownTimer)
	{
		if (bOnCooldown)
		{
			const int32 SecondsRemaining = FMath::Max(1, FMath::CeilToInt(ViewModel.CountdownTime));
			Text_CooldownTimer->SetText(FText::AsNumber(SecondsRemaining));
			Text_CooldownTimer->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_CooldownTimer->SetText(FText::GetEmpty());
			Text_CooldownTimer->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	BP_OnSlotViewModelChanged(ViewModel);
}

#undef LOCTEXT_NAMESPACE
