// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayAbilityPreviewWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "MediaSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAbilityPreviewWidget)

UBwayAbilityPreviewWidget::UBwayAbilityPreviewWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBwayAbilityPreviewWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	EnsureVisualTreeConstructed();
	return bSuperInitialized;
}

void UBwayAbilityPreviewWidget::EnsureVisualTreeConstructed()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewRowBorder"));
		RowBorder->SetPadding(FMargin(8.f, 6.f));
		RowBorder->SetBrushColor(FLinearColor(0.05f, 0.09f, 0.16f, 0.92f));
		WidgetTree->RootWidget = RowBorder;

		UHorizontalBox* RowHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PreviewRowHBox"));
		RowBorder->SetContent(RowHBox);

		// Left Icon (36x36)
		USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PreviewIconSize"));
		IconSize->SetWidthOverride(36.f);
		IconSize->SetHeightOverride(36.f);
		UHorizontalBoxSlot* IconSlot = RowHBox->AddChildToHorizontalBox(IconSize);
		IconSlot->SetVerticalAlignment(VAlign_Top);
		IconSlot->SetPadding(FMargin(0.f, 2.f, 8.f, 0.f));

		IconBackdropBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewIconBackdrop"));
		IconBackdropBorder->SetPadding(FMargin(2.f));
		IconBackdropBorder->SetBrushColor(FLinearColor(0.10f, 0.20f, 0.34f, 0.95f));
		IconSize->AddChild(IconBackdropBorder);

		IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviewIconImage"));
		IconImage->SetDesiredSizeOverride(FVector2D(32.f, 32.f));
		IconBackdropBorder->SetContent(IconImage);

		// Right Details Column
		UVerticalBox* DetailsVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PreviewDetailsVBox"));
		RowHBox->AddChildToHorizontalBox(DetailsVBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PreviewHeaderRow"));
		DetailsVBox->AddChildToVerticalBox(HeaderRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));

		// [Key] Badge
		KeyBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewKeyBadgeBorder"));
		KeyBadgeBorder->SetPadding(FMargin(5.f, 1.f));
		KeyBadgeBorder->SetBrushColor(FLinearColor(0.14f, 0.24f, 0.38f, 0.96f));
		KeyBadgeText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PreviewKeyBadgeText"));
		{
			FSlateFontInfo Font = KeyBadgeText->GetFont();
			Font.Size = 9;
			KeyBadgeText->SetFont(Font);
			KeyBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.84f, 0.32f, 1.0f)));
		}
		KeyBadgeBorder->SetContent(KeyBadgeText);
		UHorizontalBoxSlot* KeySlot = HeaderRow->AddChildToHorizontalBox(KeyBadgeBorder);
		KeySlot->SetVerticalAlignment(VAlign_Center);
		KeySlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

		// Ability Name
		AbilityNameText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PreviewAbilityNameText"));
		{
			FSlateFontInfo Font = AbilityNameText->GetFont();
			Font.Size = 11;
			AbilityNameText->SetFont(Font);
			AbilityNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
		UHorizontalBoxSlot* NameSlot = HeaderRow->AddChildToHorizontalBox(AbilityNameText);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

		// Role Badge (PRIMARY / KIT / ULTIMATE / BUILDABLE)
		RoleBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewRoleBadgeBorder"));
		RoleBadgeBorder->SetPadding(FMargin(5.f, 1.f));
		RoleBadgeText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PreviewRoleBadgeText"));
		{
			FSlateFontInfo Font = RoleBadgeText->GetFont();
			Font.Size = 8;
			RoleBadgeText->SetFont(Font);
			RoleBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}
		RoleBadgeBorder->SetContent(RoleBadgeText);
		HeaderRow->AddChildToHorizontalBox(RoleBadgeBorder)->SetVerticalAlignment(VAlign_Center);

		USpacer* HeaderSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("PreviewHeaderSpacer"));
		HeaderRow->AddChildToHorizontalBox(HeaderSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// Cooldown Readout
		CooldownText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PreviewCooldownText"));
		{
			FSlateFontInfo Font = CooldownText->GetFont();
			Font.Size = 10;
			CooldownText->SetFont(Font);
			CooldownText->SetJustification(ETextJustify::Right);
			CooldownText->SetColorAndOpacity(FSlateColor(FLinearColor(0.32f, 0.84f, 0.98f, 1.0f)));
		}
		HeaderRow->AddChildToHorizontalBox(CooldownText)->SetVerticalAlignment(VAlign_Center);

		// Description Text
		DescriptionText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PreviewDescriptionText"));
		{
			FSlateFontInfo Font = DescriptionText->GetFont();
			Font.Size = 9;
			DescriptionText->SetFont(Font);
			DescriptionText->SetAutoWrapText(true);
			DescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.76f, 0.83f, 0.92f, 0.96f)));
		}
		DetailsVBox->AddChildToVerticalBox(DescriptionText);
	RefreshVisualPresentation();
}

TSharedRef<SWidget> UBwayAbilityPreviewWidget::RebuildWidget()
{
	EnsureVisualTreeConstructed();
	return Super::RebuildWidget();
}

void UBwayAbilityPreviewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshVisualPresentation();
}

void UBwayAbilityPreviewWidget::SetAbilityPreview(const FAbilityDisplayInfo& AbilityInfo)
{
	CurrentAbilityInfo = AbilityInfo;
	bHasPreview = !CurrentAbilityInfo.AbilityName.IsEmpty();

	LoadedPreviewImage = !CurrentAbilityInfo.Icon.IsNull()
		? CurrentAbilityInfo.Icon.LoadSynchronous()
		: CurrentAbilityInfo.PreviewImage.LoadSynchronous();

	LoadedVideoSource = CurrentAbilityInfo.PreviewVideo.LoadSynchronous();

	RefreshVisualPresentation();

	OnAbilityPreviewSet(
		LoadedPreviewImage,
		CurrentAbilityInfo.AbilityName,
		CurrentAbilityInfo.Description,
		CurrentAbilityInfo.bIsUltimate
	);

	if (LoadedVideoSource)
	{
		OnPreviewVideoAvailable(LoadedVideoSource);
	}
}

void UBwayAbilityPreviewWidget::SetAbilityPreviewDirect(UTexture2D* PreviewImage, const FText& AbilityName, const FText& Description)
{
	CurrentAbilityInfo.AbilityName = AbilityName;
	CurrentAbilityInfo.Description = Description;
	LoadedPreviewImage = PreviewImage;
	bHasPreview = !AbilityName.IsEmpty();

	RefreshVisualPresentation();

	OnAbilityPreviewSet(
		LoadedPreviewImage,
		CurrentAbilityInfo.AbilityName,
		CurrentAbilityInfo.Description,
		CurrentAbilityInfo.bIsUltimate
	);
}

void UBwayAbilityPreviewWidget::SetInputKeyLabelOverride(const FText& NewKeyLabel)
{
	CurrentAbilityInfo.InputKeyLabel = NewKeyLabel;
	RefreshVisualPresentation();
}

void UBwayAbilityPreviewWidget::ClearPreview()
{
	CurrentAbilityInfo = FAbilityDisplayInfo();
	LoadedPreviewImage = nullptr;
	LoadedVideoSource = nullptr;
	bHasPreview = false;

	RefreshVisualPresentation();
	OnPreviewCleared();
}

FText UBwayAbilityPreviewWidget::GetDisplayedAbilityName() const
{
	return AbilityNameText ? AbilityNameText->GetText() : CurrentAbilityInfo.AbilityName;
}

FText UBwayAbilityPreviewWidget::GetDisplayedKeyLabel() const
{
	return KeyBadgeText ? KeyBadgeText->GetText() : CurrentAbilityInfo.InputKeyLabel;
}

FText UBwayAbilityPreviewWidget::GetDisplayedRoleLabel() const
{
	return RoleBadgeText ? RoleBadgeText->GetText() : FText::GetEmpty();
}

FText UBwayAbilityPreviewWidget::GetDisplayedCooldownText() const
{
	return CooldownText ? CooldownText->GetText() : FText::GetEmpty();
}

FText UBwayAbilityPreviewWidget::GetDisplayedDescription() const
{
	return DescriptionText ? DescriptionText->GetText() : CurrentAbilityInfo.Description;
}

void UBwayAbilityPreviewWidget::RefreshVisualPresentation()
{
	if (!bHasPreview)
	{
		if (RowBorder)
		{
			RowBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (RowBorder)
	{
		RowBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (CurrentAbilityInfo.bIsBuildable)
		{
			RowBorder->SetBrushColor(FLinearColor(0.11f, 0.13f, 0.06f, 0.94f));
		}
		else if (CurrentAbilityInfo.bIsUltimate)
		{
			RowBorder->SetBrushColor(FLinearColor(0.12f, 0.08f, 0.18f, 0.94f));
		}
		else
		{
			RowBorder->SetBrushColor(FLinearColor(0.05f, 0.09f, 0.16f, 0.92f));
		}
	}

	const FString InputTagStr = CurrentAbilityInfo.InputActionTag.ToString();
	const bool bIsPrimary = InputTagStr.Contains(TEXT("AbilityPrimary"));

	FLinearColor AccentColor(0.22f, 0.52f, 0.82f, 1.0f);
	FText RoleLabel = NSLOCTEXT("AbilityPreview", "RoleKit", "KIT");
	if (CurrentAbilityInfo.bIsBuildable)
	{
		AccentColor = FLinearColor(0.88f, 0.66f, 0.18f, 1.0f);
		RoleLabel = NSLOCTEXT("AbilityPreview", "RoleBuildable", "SIGNATURE BUILDABLE");
	}
	else if (CurrentAbilityInfo.bIsUltimate)
	{
		AccentColor = FLinearColor(0.72f, 0.34f, 0.96f, 1.0f);
		RoleLabel = NSLOCTEXT("AbilityPreview", "RoleUltimate", "ULTIMATE");
	}
	else if (bIsPrimary)
	{
		AccentColor = FLinearColor(0.90f, 0.42f, 0.24f, 1.0f);
		RoleLabel = NSLOCTEXT("AbilityPreview", "RolePrimary", "PRIMARY");
	}

	if (IconBackdropBorder)
	{
		IconBackdropBorder->SetBrushColor(FLinearColor(AccentColor.R * 0.35f, AccentColor.G * 0.35f, AccentColor.B * 0.35f, 0.95f));
	}
	if (IconImage)
	{
		if (LoadedPreviewImage)
		{
			IconImage->SetBrushFromTexture(LoadedPreviewImage, false);
			IconImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			IconImage->SetBrushTintColor(FSlateColor(AccentColor));
		}
	}

	if (KeyBadgeBorder && KeyBadgeText)
	{
		const FString RawKey = CurrentAbilityInfo.InputKeyLabel.ToString().TrimStartAndEnd();
		if (RawKey.IsEmpty())
		{
			KeyBadgeBorder->SetVisibility(ESlateVisibility::Collapsed);
			KeyBadgeText->SetText(FText::GetEmpty());
		}
		else
		{
			KeyBadgeBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			KeyBadgeText->SetText(FText::FromString(FString::Printf(TEXT("[%s]"), *RawKey)));
		}
	}

	if (AbilityNameText)
	{
		AbilityNameText->SetText(CurrentAbilityInfo.AbilityName);
	}

	if (RoleBadgeBorder)
	{
		RoleBadgeBorder->SetBrushColor(FLinearColor(AccentColor.R * 0.45f, AccentColor.G * 0.45f, AccentColor.B * 0.45f, 0.95f));
	}
	if (RoleBadgeText)
	{
		RoleBadgeText->SetText(RoleLabel);
	}

	if (CooldownText)
	{
		if (CurrentAbilityInfo.CooldownSeconds > 0.05f)
		{
			const float RoundedTenth = FMath::RoundToFloat(CurrentAbilityInfo.CooldownSeconds * 10.0f) / 10.0f;
			const bool bWholeSecond = FMath::IsNearlyEqual(RoundedTenth, FMath::RoundToFloat(RoundedTenth), 0.01f);
			const FString FormattedSecs = bWholeSecond
				? FString::Printf(TEXT("CD: %ds"), FMath::RoundToInt(RoundedTenth))
				: FString::Printf(TEXT("CD: %.1fs"), RoundedTenth);
			CooldownText->SetText(FText::FromString(FormattedSecs));
		}
		else if (bIsPrimary)
		{
			CooldownText->SetText(NSLOCTEXT("AbilityPreview", "PrimaryNoCD", "Primary"));
		}
		else if (CurrentAbilityInfo.bIsBuildable)
		{
			CooldownText->SetText(NSLOCTEXT("AbilityPreview", "BuildableCD", "1 Active"));
		}
		else
		{
			CooldownText->SetText(FText::GetEmpty());
		}
	}

	if (DescriptionText)
	{
		DescriptionText->SetText(CurrentAbilityInfo.Description);
	}
}
