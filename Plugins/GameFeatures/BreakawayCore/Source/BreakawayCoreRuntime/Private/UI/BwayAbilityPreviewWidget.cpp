// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayAbilityPreviewWidget.h"
#include "Engine/Texture2D.h"
#include "MediaSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAbilityPreviewWidget)

UBwayAbilityPreviewWidget::UBwayAbilityPreviewWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayAbilityPreviewWidget::SetAbilityPreview(const FAbilityDisplayInfo& AbilityInfo)
{
	CurrentAbilityInfo = AbilityInfo;
	bHasPreview = true;

	// Load preview image
	LoadedPreviewImage = CurrentAbilityInfo.PreviewImage.LoadSynchronous();

	// Load video if available
	LoadedVideoSource = CurrentAbilityInfo.PreviewVideo.LoadSynchronous();

	// Notify Blueprint
	OnAbilityPreviewSet(
		LoadedPreviewImage,
		CurrentAbilityInfo.AbilityName,
		CurrentAbilityInfo.Description,
		CurrentAbilityInfo.bIsUltimate
	);

	// If video is available, notify
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
	bHasPreview = true;

	OnAbilityPreviewSet(
		LoadedPreviewImage,
		CurrentAbilityInfo.AbilityName,
		CurrentAbilityInfo.Description,
		CurrentAbilityInfo.bIsUltimate
	);
}

void UBwayAbilityPreviewWidget::ClearPreview()
{
	CurrentAbilityInfo = FAbilityDisplayInfo();
	LoadedPreviewImage = nullptr;
	LoadedVideoSource = nullptr;
	bHasPreview = false;

	OnPreviewCleared();
}

