// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BwayMatchStatsTypes.generated.h"

class UTexture2D;

/**
 * Per-player combat/objective/economy stats for a single round or full match.
 */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayPlayerMatchStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 Assists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 GoldEarned = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 DamageDealt = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 HealingDone = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 RelicScores = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 ForcedFumbles = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 Interceptions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 BuildablesDestroyed = 0;

	void Reset();

	void Add(const FBwayPlayerMatchStats& Other);

	/** Current minus baseline; each field clamped to >= 0. */
	static FBwayPlayerMatchStats Diff(const FBwayPlayerMatchStats& Current, const FBwayPlayerMatchStats& Baseline);

	/** Display format used by summary widgets (e.g. 12/9/18). */
	FText GetKDAFormattedText() const;
};

/**
 * Team-aggregated stats (sum of all players on the team).
 */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayTeamStatAggregate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 TeamIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	FBwayPlayerMatchStats Stats;

	FText GetKDAFormattedText() const;
};

/**
 * View data for the PostRound team-aggregate interstitial (Step 15).
 * Built with authoritative game-team indices (0/1). UBwayPostRoundSummaryWidget remaps
 * Team0/Team1 columns to local-left / enemy-right before display and OnSummaryReady.
 */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayPostRoundSummaryData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PostRound")
	int32 CompletedRoundNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PostRound")
	int32 RoundWinningTeam = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "PostRound")
	float DisplayDurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PostRound")
	int32 Team0Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PostRound")
	int32 Team1Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PostRound")
	int32 PointsToWin = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PostRound")
	bool bLocalTeamWonRound = false;

	UPROPERTY(BlueprintReadOnly, Category = "PostRound")
	FBwayTeamStatAggregate Team0Stats;

	UPROPERTY(BlueprintReadOnly, Category = "PostRound")
	FBwayTeamStatAggregate Team1Stats;
};

/**
 * Per-player column for the post-match breakdown (Step 16).
 */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayMatchBreakdownPlayerColumn
{
	GENERATED_BODY()

	/** Replicated player identity; display names are not unique. */
	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	int32 GameTeamIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	int32 DisplayColumnIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	bool bIsLocalPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	bool bIsMVP = false;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	FText HeroName;

	/** Local presentation data resolved from the player's selected hero. */
	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	TObjectPtr<UTexture2D> HeroPortrait = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	FBwayPlayerMatchStats Stats;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	float MVPScore = 0.0f;
};

/**
 * View data for post-match UI: interstitial + breakdown (Step 16).
 * Team0/Team1 columns are remapped to local-left / enemy-right in widget code.
 */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayPostMatchSummaryData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	int32 WinningTeam = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	bool bLocalPlayerWon = false;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	int32 Team0Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	int32 Team1Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	int32 PointsToWin = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	int32 TotalRounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	float InterstitialDurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	FBwayTeamStatAggregate Team0MatchStats;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	FBwayTeamStatAggregate Team1MatchStats;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	TArray<FBwayMatchBreakdownPlayerColumn> PlayerColumns;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	FText MVPPlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "PostMatch")
	FText MVPHeroName;
};
