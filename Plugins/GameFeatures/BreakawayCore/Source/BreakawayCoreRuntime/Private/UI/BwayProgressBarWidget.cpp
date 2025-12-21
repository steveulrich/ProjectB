// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayProgressBarWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayProgressBarWidget)

UBwayProgressBarWidget::UBwayProgressBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayProgressBarWidget::SetProgress(float Progress)
{
	CurrentProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
	OnProgressChanged(CurrentProgress);
}

void UBwayProgressBarWidget::SetLevel(int32 CurrentLevel, int32 MaxLevel)
{
	Level = CurrentLevel;
	MaxLevelValue = MaxLevel;

	// Generate level text
	FText LevelText;
	if (Level >= MaxLevelValue)
	{
		LevelText = FText::Format(MaxLevelFormatText, FText::AsNumber(MaxLevelValue));
		bAtMaxLevel = true;
	}
	else
	{
		LevelText = FText::Format(LevelFormatText, FText::AsNumber(Level));
		bAtMaxLevel = false;
	}

	OnLevelChanged(Level, MaxLevelValue, LevelText);
	OnMaxLevelStateChanged(bAtMaxLevel);
}

void UBwayProgressBarWidget::SetIsMaxLevel(bool bIsMaxLevel)
{
	if (bAtMaxLevel != bIsMaxLevel)
	{
		bAtMaxLevel = bIsMaxLevel;
		OnMaxLevelStateChanged(bAtMaxLevel);
	}
}

