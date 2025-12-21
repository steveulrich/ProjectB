// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BwayProgressBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * UBwayProgressBarWidget
 * 
 * A styled progress bar widget for displaying level progress, XP, etc.
 * Includes text overlay support for showing level number and max level.
 * 
 * Designed to be extended in Blueprint for visual customization.
 */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class BREAKAWAYCORERUNTIME_API UBwayProgressBarWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBwayProgressBarWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set the progress value (0.0 to 1.0)
	 * @param Progress Normalized progress value
	 */
	UFUNCTION(BlueprintCallable, Category = "Progress Bar")
	void SetProgress(float Progress);

	/**
	 * Set the current and max level for display
	 * @param CurrentLevel The current level
	 * @param MaxLevel The maximum level
	 */
	UFUNCTION(BlueprintCallable, Category = "Progress Bar")
	void SetLevel(int32 CurrentLevel, int32 MaxLevel);

	/**
	 * Set whether max level has been reached
	 * @param bIsMaxLevel True if at max level
	 */
	UFUNCTION(BlueprintCallable, Category = "Progress Bar")
	void SetIsMaxLevel(bool bIsMaxLevel);

	/** Get the current progress value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progress Bar")
	float GetProgress() const { return CurrentProgress; }

	/** Get the current level */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progress Bar")
	int32 GetCurrentLevel() const { return Level; }

	/** Check if at max level */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progress Bar")
	bool GetIsMaxLevel() const { return bAtMaxLevel; }

protected:
	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	/**
	 * Called when progress value changes
	 * @param NewProgress The new progress value (0.0-1.0)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Progress Bar|Events")
	void OnProgressChanged(float NewProgress);

	/**
	 * Called when level information changes
	 * @param CurrentLevel The current level
	 * @param MaxLevel The maximum level
	 * @param LevelText Formatted text (e.g., "LEVEL 45" or "MAX LEVEL 99")
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Progress Bar|Events")
	void OnLevelChanged(int32 CurrentLevel, int32 MaxLevel, const FText& LevelText);

	/**
	 * Called when max level state changes
	 * @param bIsMaxLevel True if at max level
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Progress Bar|Events")
	void OnMaxLevelStateChanged(bool bIsMaxLevel);

protected:
	/** Current progress value (0.0 to 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "Progress Bar")
	float CurrentProgress = 0.0f;

	/** Current level */
	UPROPERTY(BlueprintReadOnly, Category = "Progress Bar")
	int32 Level = 1;

	/** Maximum level */
	UPROPERTY(BlueprintReadOnly, Category = "Progress Bar")
	int32 MaxLevelValue = 99;

	/** Whether at max level */
	UPROPERTY(BlueprintReadOnly, Category = "Progress Bar")
	bool bAtMaxLevel = false;

	/** Format string for level display */
	UPROPERTY(EditDefaultsOnly, Category = "Progress Bar")
	FText LevelFormatText = NSLOCTEXT("ProgressBar", "LevelFormat", "LEVEL {0}");

	/** Format string for max level display */
	UPROPERTY(EditDefaultsOnly, Category = "Progress Bar")
	FText MaxLevelFormatText = NSLOCTEXT("ProgressBar", "MaxLevelFormat", "MAX LEVEL {0}");
};

