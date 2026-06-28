// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "BwayPostMatchInterstitialWidget.generated.h"

class UTextBlock;

/**
 * END OF MATCH interstitial — team aggregate match totals (Step 16).
 * Blueprint child: WBP_BW_MatchSummaryInterstitial
 *
 * Team0 / left = local (blue). Team1 / right = enemy (red).
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Post Match Interstitial"))
class BREAKAWAYCORERUNTIME_API UBwayPostMatchInterstitialWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayPostMatchInterstitialWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "PostMatch")
	void ApplySummaryData(FBwayPostMatchSummaryData SummaryData);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "PostMatch|Events")
	void OnInterstitialReady(const FBwayPostMatchSummaryData& SummaryData);

	UFUNCTION(BlueprintPure, Category = "PostMatch")
	FText GetOutcomeHeaderText() const;

	UFUNCTION(BlueprintPure, Category = "PostMatch")
	FText GetTeamKDAFormattedText(int32 DisplayColumnIndex) const;

	UFUNCTION(BlueprintPure, Category = "PostMatch")
	int32 GetTeamStatValue(int32 DisplayColumnIndex, FName StatName) const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Header;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Team0_KDA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Team1_KDA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Team0_Gold;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Team1_Gold;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Team0_Damage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Team1_Damage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Team0_Healing;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Team1_Healing;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Team0_Buildables;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UTextBlock> Text_Team1_Buildables;

private:
	void RefreshBoundWidgets();

	static int32 GetStatFieldValue(const FBwayPlayerMatchStats& Stats, FName StatName);

	UPROPERTY()
	FBwayPostMatchSummaryData CachedSummary;
};
