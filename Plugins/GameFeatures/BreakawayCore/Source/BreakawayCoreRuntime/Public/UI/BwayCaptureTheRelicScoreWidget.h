// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "BwayCaptureTheRelicScoreWidget.generated.h"

class UCommonNumericTextBlock;
class UCommonTextBlock;

/**
 * Lyra HUD slot widget for Capture-the-Relic team round-win scores.
 * Injected via UGameFeatureAction_AddWidgets into HUD.Slot.TeamScore.
 *
 * C++ owns delegate binding and optional BindWidget text updates;
 * Blueprint subclasses supply UMG layout and styling only.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway CTR Score Widget"))
class BREAKAWAYCORERUNTIME_API UBwayCaptureTheRelicScoreWidget : public UBwayMatchHUDWidgetBase
{
	GENERATED_BODY()

public:
	UBwayCaptureTheRelicScoreWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Push current scores to bound text blocks and fire OnScoreChanged. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Score")
	void RefreshScoreDisplay();

	/** Push current round timer to bound text block and fire OnRoundTimeUpdated. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Timer")
	void RefreshTimerDisplay();

	/** Called when either display-column score changes (Team1 = left/friendly, Team2 = right/enemy). */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnScoreChanged(int32 Team1Score, int32 Team2Score);

	/** Called when round timer display refreshes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnRoundTimeUpdated(int32 SecondsRemaining, const FText& FormattedTime);

	/** Called when round timer enters sudden-death warning window (<= SuddenDeathWarningSeconds). */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnSuddenDeathTimerStateChanged(bool bSuddenDeathWarningActive);

	/** Optional named bindings — auto-updated when present in widget tree. BlueprintReadOnly required for Event Graph access. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UCommonNumericTextBlock> Text_Team1Score;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UCommonNumericTextBlock> Text_Team2Score;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UCommonTextBlock> Text_RoundLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UCommonTextBlock> Text_Timer;

	/** Poll round timer on clients (OnRoundTimeChanged is authority-only). */
	UPROPERTY(EditDefaultsOnly, Category = "HUD|Timer")
	float TimerPollInterval = 1.0f;

	/** When false, timer text and OnRoundTimeUpdated are skipped. */
	UPROPERTY(EditDefaultsOnly, Category = "HUD|Timer")
	bool bShowRoundTimer = true;

	/** Push current round number to bound round label text. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Timer")
	void RefreshRoundLabelDisplay();

private:
	virtual void NotifyTeamScoreChanged(int32 TeamIndex, int32 NewScore) override;
	virtual void NotifyRoundTimeChanged(int32 SecondsRemaining) override;
	virtual void NotifyRoundStateChanged(FName NewState) override;

	void UpdateBoundScoreTexts(int32 Team1Score, int32 Team2Score);
	void UpdateBoundTimerText(int32 SecondsRemaining);
	void UpdateBoundRoundLabelText();

	FTimerHandle TimerPollHandle;
	bool bSuddenDeathWarningShown = false;
};
