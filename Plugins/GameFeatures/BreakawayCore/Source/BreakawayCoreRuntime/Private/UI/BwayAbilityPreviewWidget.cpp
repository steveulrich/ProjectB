// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayAbilityPreviewWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
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
	if (Cast<UWidgetBlueprintGeneratedClass>(GetClass()) != nullptr)
	{
		return;
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	bUsingNativeVisualTree = true;

	NativeRowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewRowBorder"));
	NativeRowBorder->SetPadding(FMargin(8.f, 6.f));
	NativeRowBorder->SetBrushColor(FLinearColor(0.05f, 0.09f, 0.16f, 0.92f));
	WidgetTree->RootWidget = NativeRowBorder;

	UHorizontalBox* RowHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PreviewRowHBox"));
	NativeRowBorder->SetContent(RowHBox);

	// Left Icon (36x36)
	USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PreviewIconSize"));
	IconSize->SetWidthOverride(36.f);
	IconSize->SetHeightOverride(36.f);
	UHorizontalBoxSlot* IconSlot = RowHBox->AddChildToHorizontalBox(IconSize);
	IconSlot->SetVerticalAlignment(VAlign_Top);
	IconSlot->SetPadding(FMargin(0.f, 2.f, 8.f, 0.f));

	NativeIconBackdropBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewIconBackdrop"));
	NativeIconBackdropBorder->SetPadding(FMargin(2.f));
	NativeIconBackdropBorder->SetBrushColor(FLinearColor(0.10f, 0.20f, 0.34f, 0.95f));
	IconSize->AddChild(NativeIconBackdropBorder);

	NativeIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PreviewIconImage"));
	NativeIconImage->SetDesiredSizeOverride(FVector2D(32.f, 32.f));
	NativeIconBackdropBorder->SetContent(NativeIconImage);

	// Right Details Column
	UVerticalBox* DetailsVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PreviewDetailsVBox"));
	RowHBox->AddChildToHorizontalBox(DetailsVBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("PreviewHeaderRow"));
	DetailsVBox->AddChildToVerticalBox(HeaderRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));

	// [Key] Badge
	NativeKeyBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewKeyBadgeBorder"));
	NativeKeyBadgeBorder->SetPadding(FMargin(5.f, 1.f));
	NativeKeyBadgeBorder->SetBrushColor(FLinearColor(0.14f, 0.24f, 0.38f, 0.96f));
	NativeKeyBadgeText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PreviewKeyBadgeText"));
	{
		FSlateFontInfo Font = NativeKeyBadgeText->GetFont();
		Font.Size = 9;
		NativeKeyBadgeText->SetFont(Font);
		NativeKeyBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.84f, 0.32f, 1.0f)));
	}
	NativeKeyBadgeBorder->SetContent(NativeKeyBadgeText);
	UHorizontalBoxSlot* KeySlot = HeaderRow->AddChildToHorizontalBox(NativeKeyBadgeBorder);
	KeySlot->SetVerticalAlignment(VAlign_Center);
	KeySlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

	// Ability Name
	NativeAbilityNameText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PreviewAbilityNameText"));
	{
		FSlateFontInfo Font = NativeAbilityNameText->GetFont();
		Font.Size = 11;
		NativeAbilityNameText->SetFont(Font);
		NativeAbilityNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	UHorizontalBoxSlot* NameSlot = HeaderRow->AddChildToHorizontalBox(NativeAbilityNameText);
	NameSlot->SetVerticalAlignment(VAlign_Center);
	NameSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));

	// Role Badge (PRIMARY / KIT / ULTIMATE / BUILDABLE)
	NativeRoleBadgeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewRoleBadgeBorder"));
	NativeRoleBadgeBorder->SetPadding(FMargin(5.f, 1.f));
	NativeRoleBadgeText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PreviewRoleBadgeText"));
	{
		FSlateFontInfo Font = NativeRoleBadgeText->GetFont();
		Font.Size = 8;
		NativeRoleBadgeText->SetFont(Font);
		NativeRoleBadgeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	NativeRoleBadgeBorder->SetContent(NativeRoleBadgeText);
	HeaderRow->AddChildToHorizontalBox(NativeRoleBadgeBorder)->SetVerticalAlignment(VAlign_Center);

	USpacer* HeaderSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("PreviewHeaderSpacer"));
	HeaderRow->AddChildToHorizontalBox(HeaderSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Cooldown Readout
	NativeCooldownText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PreviewCooldownText"));
	{
		FSlateFontInfo Font = NativeCooldownText->GetFont();
		Font.Size = 10;
		NativeCooldownText->SetFont(Font);
		NativeCooldownText->SetJustification(ETextJustify::Right);
		NativeCooldownText->SetColorAndOpacity(FSlateColor(FLinearColor(0.32f, 0.84f, 0.98f, 1.0f)));
	}
	HeaderRow->AddChildToHorizontalBox(NativeCooldownText)->SetVerticalAlignment(VAlign_Center);

	// Description Text
	NativeDescriptionText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("PreviewDescriptionText"));
	{
		FSlateFontInfo Font = NativeDescriptionText->GetFont();
		Font.Size = 9;
		NativeDescriptionText->SetFont(Font);
		NativeDescriptionText->SetAutoWrapText(true);
		NativeDescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.76f, 0.83f, 0.92f, 0.96f)));
	}
	DetailsVBox->AddChildToVerticalBox(NativeDescriptionText);
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
	return NativeAbilityNameText ? NativeAbilityNameText->GetText() : CurrentAbilityInfo.AbilityName;
}

FText UBwayAbilityPreviewWidget::GetDisplayedKeyLabel() const
{
	return NativeKeyBadgeText ? NativeKeyBadgeText->GetText() : CurrentAbilityInfo.InputKeyLabel;
}

FText UBwayAbilityPreviewWidget::GetDisplayedRoleLabel() const
{
	return NativeRoleBadgeText ? NativeRoleBadgeText->GetText() : FText::GetEmpty();
}

FText UBwayAbilityPreviewWidget::GetDisplayedCooldownText() const
{
	return NativeCooldownText ? NativeCooldownText->GetText() : FText::GetEmpty();
}

FText UBwayAbilityPreviewWidget::GetDisplayedDescription() const
{
	return NativeDescriptionText ? NativeDescriptionText->GetText() : CurrentAbilityInfo.Description;
}

void UBwayAbilityPreviewWidget::RefreshVisualPresentation()
{
	if (!bUsingNativeVisualTree)
	{
		return;
	}

	if (!bHasPreview)
	{
		if (NativeRowBorder)
		{
			NativeRowBorder->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (NativeRowBorder)
	{
		NativeRowBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (CurrentAbilityInfo.bIsBuildable)
		{
			NativeRowBorder->SetBrushColor(FLinearColor(0.11f, 0.13f, 0.06f, 0.94f));
		}
		else if (CurrentAbilityInfo.bIsUltimate)
		{
			NativeRowBorder->SetBrushColor(FLinearColor(0.12f, 0.08f, 0.18f, 0.94f));
		}
		else
		{
			NativeRowBorder->SetBrushColor(FLinearColor(0.05f, 0.09f, 0.16f, 0.92f));
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

	if (NativeIconBackdropBorder)
	{
		NativeIconBackdropBorder->SetBrushColor(FLinearColor(AccentColor.R * 0.35f, AccentColor.G * 0.35f, AccentColor.B * 0.35f, 0.95f));
	}
	if (NativeIconImage)
	{
		if (LoadedPreviewImage)
		{
			NativeIconImage->SetBrushFromTexture(LoadedPreviewImage, false);
			NativeIconImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			NativeIconImage->SetBrushTintColor(FSlateColor(AccentColor));
		}
	}

	if (NativeKeyBadgeBorder && NativeKeyBadgeText)
	{
		const FString RawKey = CurrentAbilityInfo.InputKeyLabel.ToString().TrimStartAndEnd();
		if (RawKey.IsEmpty())
		{
			NativeKeyBadgeBorder->SetVisibility(ESlateVisibility::Collapsed);
			NativeKeyBadgeText->SetText(FText::GetEmpty());
		}
		else
		{
			NativeKeyBadgeBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			NativeKeyBadgeText->SetText(FText::FromString(FString::Printf(TEXT("[%s]"), *RawKey)));
		}
	}

	if (NativeAbilityNameText)
	{
		NativeAbilityNameText->SetText(CurrentAbilityInfo.AbilityName);
	}

	if (NativeRoleBadgeBorder)
	{
		NativeRoleBadgeBorder->SetBrushColor(FLinearColor(AccentColor.R * 0.45f, AccentColor.G * 0.45f, AccentColor.B * 0.45f, 0.95f));
	}
	if (NativeRoleBadgeText)
	{
		NativeRoleBadgeText->SetText(RoleLabel);
	}

	if (NativeCooldownText)
	{
		if (CurrentAbilityInfo.CooldownSeconds > 0.05f)
		{
			const float RoundedTenth = FMath::RoundToFloat(CurrentAbilityInfo.CooldownSeconds * 10.0f) / 10.0f;
			const bool bWholeSecond = FMath::IsNearlyEqual(RoundedTenth, FMath::RoundToFloat(RoundedTenth), 0.01f);
			const FString FormattedSecs = bWholeSecond
				? FString::Printf(TEXT("CD: %ds"), FMath::RoundToInt(RoundedTenth))
				: FString::Printf(TEXT("CD: %.1fs"), RoundedTenth);
			NativeCooldownText->SetText(FText::FromString(FormattedSecs));
		}
		else if (bIsPrimary)
		{
			NativeCooldownText->SetText(NSLOCTEXT("AbilityPreview", "PrimaryNoCD", "Primary"));
		}
		else if (CurrentAbilityInfo.bIsBuildable)
		{
			NativeCooldownText->SetText(NSLOCTEXT("AbilityPreview", "BuildableCD", "1 Active"));
		}
		else
		{
			NativeCooldownText->SetText(FText::GetEmpty());
		}
	}

	if (NativeDescriptionText)
	{
		NativeDescriptionText->SetText(CurrentAbilityInfo.Description);
	}
}
