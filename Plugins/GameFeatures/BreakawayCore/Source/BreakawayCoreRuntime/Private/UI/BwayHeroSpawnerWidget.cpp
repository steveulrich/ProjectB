// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayHeroSpawnerWidget.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroSpawnerWidget)

UBwayHeroSpawnerWidget::UBwayHeroSpawnerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayHeroSpawnerWidget::SetHeroInfo(const FText& InDisplayName, UTexture2D* InPortrait)
{
	DisplayName = InDisplayName;
	Portrait = InPortrait;
	bHasHeroInfo = !InDisplayName.IsEmpty() || InPortrait != nullptr;

	// Notify Blueprint
	OnHeroInfoChanged(DisplayName, Portrait);
}

void UBwayHeroSpawnerWidget::ClearHeroInfo()
{
	DisplayName = FText::GetEmpty();
	Portrait = nullptr;
	bHasHeroInfo = false;

	// Notify Blueprint
	OnHeroInfoCleared();
}

