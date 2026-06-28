// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "BwayMatchStatsLibrary.generated.h"

class ABwayGameState;
class ABwayPlayerState;
class APlayerController;

/**
 * Helpers for reading and aggregating per-player match / round stats.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayMatchStatsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Full match totals for one player. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Stats")
	static FBwayPlayerMatchStats GetPlayerMatchStats(const ABwayPlayerState* PlayerState);

	/** Stats earned during the most recently completed round. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Stats")
	static FBwayPlayerMatchStats GetPlayerLastRoundStats(const ABwayPlayerState* PlayerState);

	/** Sum player stats for a team. Round-only when bRoundOnly is true. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Stats")
	static FBwayTeamStatAggregate AggregateTeamStats(const UObject* WorldContextObject, int32 TeamIndex, bool bRoundOnly);

	/** Build PostRound interstitial data for the local viewer. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Stats")
	static FBwayPostRoundSummaryData BuildPostRoundSummaryData(
		const UObject* WorldContextObject,
		int32 CompletedRoundNumber,
		int32 RoundWinningTeam,
		float DisplayDurationSeconds);

	/** Build post-match interstitial + breakdown data for the local viewer (Step 16). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Stats")
	static FBwayPostMatchSummaryData BuildPostMatchSummaryData(
		const UObject* WorldContextObject,
		int32 WinningTeam,
		int32 TotalRounds,
		float InterstitialDurationSeconds);

	/** Remap authoritative game-team columns to local-left / enemy-right display. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Stats")
	static FBwayPostMatchSummaryData RemapPostMatchSummaryForDisplay(
		const FBwayPostMatchSummaryData& Summary,
		int32 LocalPlayerTeamIndex);

	/** Remap PostRound summary team columns for local-team-left UI. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Stats")
	static FBwayPostRoundSummaryData RemapPostRoundSummaryForDisplay(
		const FBwayPostRoundSummaryData& Summary,
		int32 LocalPlayerTeamIndex);

	/** PostRound team aggregate excludes RelicScores (design: round interstitial only). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Stats")
	static FBwayTeamStatAggregate AggregateTeamRoundStatsForPostRoundSummary(const UObject* WorldContextObject, int32 TeamIndex);

	static FBwayTeamStatAggregate AggregateTeamStatsFromGameState(const ABwayGameState* GameState, int32 TeamIndex, bool bRoundOnly);

private:
	static ABwayGameState* ResolveGameState(const UObject* WorldContextObject);

	static float CalculateMVPScore(const FBwayPlayerMatchStats& Stats);

	static void PopulateMVPAndColumns(
		const UObject* WorldContextObject,
		const ABwayGameState* GameState,
		FBwayPostMatchSummaryData& InOutSummary);
};
