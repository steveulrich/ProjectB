// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "BwayPostRoundSummaryWidget.generated.h"

class UTextBlock;

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

	/** Apply summary payload and refresh optional bound widgets. */
	UFUNCTION(BlueprintCallable, Category = "PostRound")
	void ApplySummaryData(FBwayPostRoundSummaryData SummaryData);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "PostRound|Events")
	void OnSummaryReady(const FBwayPostRoundSummaryData& SummaryData);

	UFUNCTION(BlueprintPure, Category = "PostRound")
	FText GetRoundOutcomeHeaderText() const;

	/** Display column 0 = local (left), 1 = enemy (right). */
	UFUNCTION(BlueprintPure, Category = "PostRound")
	FText GetTeamKDAFormattedText(int32 DisplayColumnIndex) const;

	/** Display column 0 = local (left), 1 = enemy (right). */
	UFUNCTION(BlueprintPure, Category = "PostRound")
	int32 GetTeamStatValue(int32 DisplayColumnIndex, FName StatName) const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostRound|Widgets")
	TObjectPtr<UTextBlock> Text_Header;

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
	void RefreshBoundWidgets();

	static int32 GetStatFieldValue(const FBwayPlayerMatchStats& Stats, FName StatName);

	UPROPERTY()
	FBwayPostRoundSummaryData CachedSummary;
};
