// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCommonButton.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCommonButton)

UBwayTransparentCommonButtonStyle::UBwayTransparentCommonButtonStyle()
{
	bSingleMaterial = false;
	NormalBase.DrawAs = ESlateBrushDrawType::NoDrawType;
	NormalHovered.DrawAs = ESlateBrushDrawType::NoDrawType;
	NormalPressed.DrawAs = ESlateBrushDrawType::NoDrawType;
	SelectedBase.DrawAs = ESlateBrushDrawType::NoDrawType;
	SelectedHovered.DrawAs = ESlateBrushDrawType::NoDrawType;
	SelectedPressed.DrawAs = ESlateBrushDrawType::NoDrawType;
	Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
	ButtonPadding = FMargin(0.f);
	CustomPadding = FMargin(0.f);
	MinWidth = 0;
	MinHeight = 0;
}

UBwayCommonButton::UBwayCommonButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Style = UBwayTransparentCommonButtonStyle::StaticClass();
	SetIsFocusable(true);
	bHideInputAction = true;
}

bool UBwayCommonButton::Initialize()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		EnsureVisualTreeConstructed();
	}
	return Super::Initialize();
}

void UBwayCommonButton::EnsureVisualTreeConstructed()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree->RootWidget)
	{
		return;
	}

	OuterFocusBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterFocusBorder"));
	OuterFocusBorder->SetPadding(FMargin(2.f));
	OuterFocusBorder->SetBrushColor(NormalRingColor);

	InnerBackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InnerBackgroundBorder"));
	InnerBackgroundBorder->SetPadding(ContentPadding);
	InnerBackgroundBorder->SetBrushColor(NormalBackgroundColor);

	LabelText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("ButtonLabel"));
	LabelText->SetText(CachedLabelText);
	LabelText->SetJustification(TextJustification);
	LabelText->SetColorAndOpacity(FSlateColor(NormalTextColor));
	FSlateFontInfo Font = LabelText->GetFont();
	Font.Size = FontSize;
	LabelText->SetFont(Font);

	InnerBackgroundBorder->SetContent(LabelText);
	OuterFocusBorder->SetContent(InnerBackgroundBorder);
	WidgetTree->RootWidget = OuterFocusBorder;
}

void UBwayCommonButton::NativeConstruct()
{
	Super::NativeConstruct();

	OnFocusReceived().RemoveAll(this);
	OnFocusLost().RemoveAll(this);
	OnFocusReceived().AddUObject(this, &ThisClass::RefreshVisualState);
	OnFocusLost().AddUObject(this, &ThisClass::RefreshVisualState);

	if (LabelText)
	{
		LabelText->SetText(CachedLabelText);
		LabelText->SetJustification(TextJustification);
		FSlateFontInfo Font = LabelText->GetFont();
		Font.Size = FontSize;
		LabelText->SetFont(Font);
	}
	if (InnerBackgroundBorder)
	{
		InnerBackgroundBorder->SetPadding(ContentPadding);
	}
	RefreshVisualState();
}

void UBwayCommonButton::SetLabelText(const FText& InText)
{
	CachedLabelText = InText;
	if (LabelText)
	{
		LabelText->SetText(InText);
	}
}

FText UBwayCommonButton::GetLabelText() const
{
	return LabelText ? LabelText->GetText() : CachedLabelText;
}

void UBwayCommonButton::SetFontSize(int32 InFontSize)
{
	FontSize = FMath::Max(8, InFontSize);
	if (LabelText)
	{
		FSlateFontInfo Font = LabelText->GetFont();
		Font.Size = FontSize;
		LabelText->SetFont(Font);
	}
}

void UBwayCommonButton::SetJustification(ETextJustify::Type InJustification)
{
	TextJustification = InJustification;
	if (LabelText)
	{
		LabelText->SetJustification(InJustification);
	}
}

void UBwayCommonButton::SetContentPadding(FMargin InPadding)
{
	ContentPadding = InPadding;
	if (InnerBackgroundBorder)
	{
		InnerBackgroundBorder->SetPadding(ContentPadding);
	}
}

void UBwayCommonButton::SetButtonColors(
	const FLinearColor& InNormalBackground,
	const FLinearColor& InFocusedBackground,
	const FLinearColor& InFocusRingColor,
	const FLinearColor& InNormalTextColor)
{
	NormalBackgroundColor = InNormalBackground;
	FocusedBackgroundColor = InFocusedBackground;
	FocusRingColor = InFocusRingColor;
	NormalTextColor = InNormalTextColor;
	RefreshVisualState();
}

void UBwayCommonButton::SetHighlightedState(bool bInHighlighted)
{
	if (bHighlighted != bInHighlighted)
	{
		bHighlighted = bInHighlighted;
		RefreshVisualState();
	}
}

bool UBwayCommonButton::SimulateClick()
{
	if (!GetIsEnabled() || !IsInteractionEnabled())
	{
		return false;
	}
	NativeOnClicked();
	return true;
}

void UBwayCommonButton::NativeOnHovered()
{
	Super::NativeOnHovered();
	RefreshVisualState();
}

void UBwayCommonButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	RefreshVisualState();
}

void UBwayCommonButton::NativeOnEnabled()
{
	Super::NativeOnEnabled();
	RefreshVisualState();
}

void UBwayCommonButton::NativeOnDisabled()
{
	Super::NativeOnDisabled();
	RefreshVisualState();
}

void UBwayCommonButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	bHasFocusVisual = true;
	RefreshVisualState();
}

void UBwayCommonButton::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	bHasFocusVisual = false;
	RefreshVisualState();
}

void UBwayCommonButton::RefreshVisualState()
{
	if (!OuterFocusBorder || !InnerBackgroundBorder || !LabelText)
	{
		return;
	}

	const bool bEnabled = GetIsEnabled() && IsInteractionEnabled();
	if (!bEnabled)
	{
		OuterFocusBorder->SetBrushColor(DisabledRingColor);
		InnerBackgroundBorder->SetBrushColor(DisabledBackgroundColor);
		LabelText->SetColorAndOpacity(FSlateColor(DisabledTextColor));
		return;
	}

	const bool bFocused = bHasFocusVisual || HasKeyboardFocus() || HasAnyUserFocus();
	const bool bHoveredState = IsHovered();

	if (bFocused)
	{
		OuterFocusBorder->SetBrushColor(FocusRingColor);
		InnerBackgroundBorder->SetBrushColor(FocusedBackgroundColor);
		LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	else if (bHoveredState)
	{
		OuterFocusBorder->SetBrushColor(HoverRingColor);
		InnerBackgroundBorder->SetBrushColor(FocusedBackgroundColor);
		LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	else if (bHighlighted || GetSelected())
	{
		OuterFocusBorder->SetBrushColor(FocusRingColor);
		InnerBackgroundBorder->SetBrushColor(FocusedBackgroundColor);
		LabelText->SetColorAndOpacity(FSlateColor(NormalTextColor));
	}
	else
	{
		OuterFocusBorder->SetBrushColor(NormalRingColor);
		InnerBackgroundBorder->SetBrushColor(NormalBackgroundColor);
		LabelText->SetColorAndOpacity(FSlateColor(NormalTextColor));
	}
}
