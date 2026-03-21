// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "BwayResultsScreenWidget.generated.h"

class ABwayGameState;

/**
 * Match results data structure
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
 * UBwayResultsScreenWidget
 * 
 * End-of-match results screen showing winning team, scores, and MVP.
 * Provides "Play Again" and "Return to Lobby" buttons.
 * Designed to be extended in Blueprint for visual implementation.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Results Screen"))
class BREAKAWAYCORERUNTIME_API UBwayResultsScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayResultsScreenWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ UUserWidget interface
	virtual void NativeConstruct() override;
	//~ End UUserWidget interface

	// ========== DATA ACCESS ==========

	/** Get the match results data */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Results")
	FMatchResultsData GetMatchResults() const;

	/** Check if local player won */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Results")
	bool DidLocalPlayerWin() const;

	/** Get the winning team index (0 or 1) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Results")
	int32 GetWinningTeam() const;

	// ========== ACTIONS ==========

	/** Play another match */
	UFUNCTION(BlueprintCallable, Category = "Results")
	void PlayAgain();

	/** Return to the main menu/lobby */
	UFUNCTION(BlueprintCallable, Category = "Results")
	void ReturnToLobby();

	// ========== BLUEPRINT EVENTS ==========

	/** Called when results data is ready to display */
	UFUNCTION(BlueprintImplementableEvent, Category = "Results|Events")
	void OnResultsReady(const FMatchResultsData& Results);

	/** Called when "Play Again" is triggered - use for confirmation or transition */
	UFUNCTION(BlueprintImplementableEvent, Category = "Results|Events")
	void OnPlayAgainRequested();

	/** Called when "Return to Lobby" is triggered */
	UFUNCTION(BlueprintImplementableEvent, Category = "Results|Events")
	void OnReturnToLobbyRequested();

protected:
	/** Level to load for a new match */
	UPROPERTY(EditDefaultsOnly, Category = "Results")
	TSoftObjectPtr<UWorld> PlayAgainLevel;

	/** Level to load for returning to lobby */
	UPROPERTY(EditDefaultsOnly, Category = "Results")
	TSoftObjectPtr<UWorld> LobbyLevel;

	/** Delay before allowing button presses (prevent accidental clicks) */
	UPROPERTY(EditDefaultsOnly, Category = "Results")
	float ButtonEnableDelay = 2.0f;

private:
	/** Get the game state */
	ABwayGameState* GetBwayGameState() const;

	/** Calculate MVP player */
	void DetermineMVP(FMatchResultsData& OutResults) const;

	/** Cached results */
	UPROPERTY()
	FMatchResultsData CachedResults;
};

