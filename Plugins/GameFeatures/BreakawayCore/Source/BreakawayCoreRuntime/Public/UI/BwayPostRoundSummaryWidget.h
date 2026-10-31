// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "BwayPostRoundSummaryWidget.generated.h"

class SBwayPostRoundSummaryCanvas;
class UFont;
class UTextBlock;
class UTexture2D;

/**
 * Team-aggregate round summary shown during PostRound (Step 15).
 * Blueprint child: WBP_BW_PostRoundSummary
 *
 * Team0 / left column = local player team (blue). Team1 / right column = enemy (red).
 * ApplySummaryData remaps authoritative game-team data before OnSummaryReady.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Post Round Summary"))
class BREAKAWAYCORERUNTIME_API UBwayPostRoundSummaryWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayPostRoundSummaryWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Apply summary payload and refresh optional bound widgets + Breakaway hexagonal canvas. */
	UFUNCTION(BlueprintCallable, Category = "PostRound")
	void ApplySummaryData(FBwayPostRoundSummaryData SummaryData);

	UFUNCTION(BlueprintPure, Category = "PostRound")
	FText GetRoundOutcomeHeaderText() const;

	UFUNCTION(BlueprintPure, Category = "PostRound")
	FText GetWinReasonText() const;

	UFUNCTION(BlueprintPure, Category = "PostRound")
	FText GetSeriesScoreText() const;

	/** Display column 0 = local (left), 1 = enemy (right). */
	UFUNCTION(BlueprintPure, Category = "PostRound")
	FText GetTeamKDAFormattedText(int32 DisplayColumnIndex) const;

	/** Display column 0 = local (left), 1 = enemy (right). */
	UFUNCTION(BlueprintPure, Category = "PostRound")
	int32 GetTeamStatValue(int32 DisplayColumnIndex, FName StatName) const;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "PostRound|Events")
	void OnSummaryReady(const FBwayPostRoundSummaryData& SummaryData);

	/** Breakaway condensed display font for header banner and team stat numbers (Refrigerator Deluxe Heavy). */
	UPROPERTY(EditDefaultsOnly, Category = "PostRound|Style")
	TSoftObjectPtr<UFont> DisplayNumeralFontAsset;

	/** Breakaway geometric sans font for center stat labels (Futura Heavy). */
	UPROPERTY(EditDefaultsOnly, Category = "PostRound|Style")
	TSoftObjectPtr<UFont> StatLabelFontAsset;

	/** Round pip icon for GoalScored wins. */
	UPROPERTY(EditDefaultsOnly, Category = "PostRound|Style")
	TSoftObjectPtr<UTexture2D> RoundIconScoreAsset;

	/** Round pip icon for TeamEliminated (Team Wipe) wins. */
	UPROPERTY(EditDefaultsOnly, Category = "PostRound|Style")
	TSoftObjectPtr<UTexture2D> RoundIconTeamWipeAsset;

	/** Round pip icon for TimeExpired / SuddenDeath territory tiebreaker wins. */
	UPROPERTY(EditDefaultsOnly, Category = "PostRound|Style")
	TSoftObjectPtr<UTexture2D> RoundIconTerritoryAsset;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Header;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_WinReason;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_SeriesScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Team0_KDA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Team1_KDA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Team0_Gold;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Team1_Gold;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Team0_Damage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Team1_Damage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Team0_Healing;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Team1_Healing;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Team0_Buildables;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Team1_Buildables;

private:
	void ResolveStyleAssets();
	void SuppressLegacyTreeVisuals();
	void RefreshBoundWidgets();
	void PushStateToSlateCanvas();

	static int32 GetStatFieldValue(const FBwayPlayerMatchStats& Stats, FName StatName);

	UPROPERTY()
	FBwayPostRoundSummaryData CachedSummary;

	UPROPERTY(Transient)
	TObjectPtr<UFont> ResolvedDisplayFont;

	UPROPERTY(Transient)
	TObjectPtr<UFont> ResolvedLabelFont;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ResolvedIconScore;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ResolvedIconTeamWipe;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ResolvedIconTerritory;

	TSharedPtr<SBwayPostRoundSummaryCanvas> PostRoundSlateCanvas;
};
