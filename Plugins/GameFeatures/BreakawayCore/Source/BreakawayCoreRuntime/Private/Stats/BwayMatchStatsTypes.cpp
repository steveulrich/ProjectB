// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stats/BwayMatchStatsTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchStatsTypes)

void FBwayPlayerMatchStats::Reset()
{
	*this = FBwayPlayerMatchStats();
}

void FBwayPlayerMatchStats::Add(const FBwayPlayerMatchStats& Other)
{
	Kills += Other.Kills;
	Deaths += Other.Deaths;
	Assists += Other.Assists;
	GoldEarned += Other.GoldEarned;
	DamageDealt += Other.DamageDealt;
	HealingDone += Other.HealingDone;
	RelicScores += Other.RelicScores;
	ForcedFumbles += Other.ForcedFumbles;
	Interceptions += Other.Interceptions;
	BuildablesDestroyed += Other.BuildablesDestroyed;
}

FBwayPlayerMatchStats FBwayPlayerMatchStats::Diff(const FBwayPlayerMatchStats& Current, const FBwayPlayerMatchStats& Baseline)
{
	FBwayPlayerMatchStats Delta;
	Delta.Kills = FMath::Max(0, Current.Kills - Baseline.Kills);
	Delta.Deaths = FMath::Max(0, Current.Deaths - Baseline.Deaths);
	Delta.Assists = FMath::Max(0, Current.Assists - Baseline.Assists);
	Delta.GoldEarned = FMath::Max(0, Current.GoldEarned - Baseline.GoldEarned);
	Delta.DamageDealt = FMath::Max(0, Current.DamageDealt - Baseline.DamageDealt);
	Delta.HealingDone = FMath::Max(0, Current.HealingDone - Baseline.HealingDone);
	Delta.RelicScores = FMath::Max(0, Current.RelicScores - Baseline.RelicScores);
	Delta.ForcedFumbles = FMath::Max(0, Current.ForcedFumbles - Baseline.ForcedFumbles);
	Delta.Interceptions = FMath::Max(0, Current.Interceptions - Baseline.Interceptions);
	Delta.BuildablesDestroyed = FMath::Max(0, Current.BuildablesDestroyed - Baseline.BuildablesDestroyed);
	return Delta;
}

FText FBwayPlayerMatchStats::GetKDAFormattedText() const
{
	return FText::FromString(FString::Printf(TEXT("%d/%d/%d"), Kills, Deaths, Assists));
}

FText FBwayTeamStatAggregate::GetKDAFormattedText() const
{
	return Stats.GetKDAFormattedText();
}
