// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "BwayResultsScreenWidget.generated.h"

class ABwayGameState;
class UBwayMatchBreakdownWidget;
class UBwayPostMatchInterstitialWidget;

/**
 * @deprecated Legacy results payload — use FBwayPostMatchSummaryData (Step 16).
 */
USTRUCT(BlueprintType)
struct FMatchResultsData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	int32 WinningTeam = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	int32 Team1Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	int32 Team2Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	int32 TotalRounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	bool bLocalPlayerWon = false;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	FText WinnerText;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	FText MVPPlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	FText MVPHeroName;
};

/**
 * Post-match flow orchestrator (Step 16):
 * END OF MATCH interstitial (timed) → Match Breakdown (buttons).
 * Blueprint child: WBP_BW_ResultsWidget (or thin wrapper).
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Results Screen"))
class BREAKAWAYCORERUNTIME_API UBwayResultsScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayResultsScreenWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void ApplyAuthoritativeResults(int32 WinningTeam, int32 Team1Score, int32 Team2Score, int32 TotalRounds);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Results")
	FBwayPostMatchSummaryData GetPostMatchSummary() const { return CachedSummary; }

	/** @deprecated Use GetPostMatchSummary(). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Results")
	FMatchResultsData GetMatchResults() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Results")
	bool DidLocalPlayerWin() const { return CachedSummary.bLocalPlayerWon; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Results")
	int32 GetWinningTeam() const { return CachedSummary.WinningTeam; }

	UFUNCTION(BlueprintCallable, Category = "Results")
	void PlayAgain();

	UFUNCTION(BlueprintCallable, Category = "Results")
	void ReturnToLobby();

	UFUNCTION(BlueprintImplementableEvent, Category = "Results|Events")
	void OnPostMatchFlowStarted(const FBwayPostMatchSummaryData& SummaryData);

	/** @deprecated Use OnPostMatchFlowStarted. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Results|Events")
	void OnResultsReady(const FMatchResultsData& Results);

	UFUNCTION(BlueprintImplementableEvent, Category = "Results|Events")
	void OnPlayAgainRequested();

	UFUNCTION(BlueprintImplementableEvent, Category = "Results|Events")
	void OnReturnToLobbyRequested();

	UPROPERTY(EditDefaultsOnly, Category = "Results|Widgets")
	TSoftClassPtr<UBwayPostMatchInterstitialWidget> InterstitialWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Results|Widgets")
	TSoftClassPtr<UBwayMatchBreakdownWidget> BreakdownWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Results")
	TSoftObjectPtr<UWorld> PlayAgainLevel;

	UPROPERTY(EditDefaultsOnly, Category = "Results")
	TSoftObjectPtr<UWorld> LobbyLevel;

	UPROPERTY(EditDefaultsOnly, Category = "Results")
	float ButtonEnableDelay = 2.0f;

private:
	void BeginPostMatchFlow();
	void ShowInterstitial();
	void AdvanceToBreakdown();
	void ShowBreakdown();
	void CleanupChildWidgets();
	float ResolveInterstitialDuration() const;

	UFUNCTION()
	void HandleBreakdownReturnToLobby();

	UFUNCTION()
	void HandleBreakdownPlayAgain();

	ABwayGameState* GetBwayGameState() const;

	UPROPERTY()
	FBwayPostMatchSummaryData CachedSummary;

	UPROPERTY(Transient)
	TObjectPtr<UBwayPostMatchInterstitialWidget> InterstitialWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBwayMatchBreakdownWidget> BreakdownWidget;

	FTimerHandle InterstitialTimerHandle;
	TWeakObjectPtr<UWidget> SuppressedGameplayLayer;
	ESlateVisibility PreviousGameplayLayerVisibility = ESlateVisibility::SelfHitTestInvisible;

	bool bResultsApplied = false;
};
