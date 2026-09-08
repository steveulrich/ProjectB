// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayPostMatchInterstitialWidget.h"

#include "Components/TextBlock.h"
#include "Stats/BwayMatchStatsLibrary.h"
#include "UI/BwayMatchHUDWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayPostMatchInterstitialWidget)

UBwayPostMatchInterstitialWidget::UBwayPostMatchInterstitialWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UBwayPostMatchInterstitialWidget::ApplySummaryData(FBwayPostMatchSummaryData SummaryData)
{
	const int32 LocalTeam = SummaryData.LocalPlayerTeamIndex;
	CachedSummary = UBwayMatchStatsLibrary::RemapPostMatchSummaryForDisplay(SummaryData, LocalTeam);
	RefreshBoundWidgets();
	OnInterstitialReady(CachedSummary);
}

FText UBwayPostMatchInterstitialWidget::GetOutcomeHeaderText() const
{
	return CachedSummary.bLocalPlayerWon
		? NSLOCTEXT("PostMatch", "Victory", "VICTORY")
		: NSLOCTEXT("PostMatch", "Defeat", "DEFEAT");
}

FText UBwayPostMatchInterstitialWidget::GetTeamKDAFormattedText(const int32 DisplayColumnIndex) const
{
	const FBwayTeamStatAggregate& TeamStats = (DisplayColumnIndex == 0)
		? CachedSummary.Team0MatchStats
		: CachedSummary.Team1MatchStats;
	return TeamStats.GetKDAFormattedText();
}

int32 UBwayPostMatchInterstitialWidget::GetTeamStatValue(const int32 DisplayColumnIndex, const FName StatName) const
{
	const FBwayPlayerMatchStats& Stats = (DisplayColumnIndex == 0)
		? CachedSummary.Team0MatchStats.Stats
		: CachedSummary.Team1MatchStats.Stats;
	return GetStatFieldValue(Stats, StatName);
}

int32 UBwayPostMatchInterstitialWidget::GetStatFieldValue(const FBwayPlayerMatchStats& Stats, const FName StatName)
{
	if (StatName == TEXT("GoldEarned"))
	{
		return Stats.GoldEarned;
	}
	if (StatName == TEXT("DamageDealt"))
	{
		return Stats.DamageDealt;
	}
	if (StatName == TEXT("HealingDone"))
	{
		return Stats.HealingDone;
	}
	if (StatName == TEXT("BuildablesDestroyed"))
	{
		return Stats.BuildablesDestroyed;
	}

	return 0;
}

void UBwayPostMatchInterstitialWidget::RefreshBoundWidgets()
{
	if (Text_Header)
	{
		Text_Header->SetText(GetOutcomeHeaderText());
	}

	if (Text_Team0_KDA)
	{
		Text_Team0_KDA->SetText(GetTeamKDAFormattedText(0));
	}
	if (Text_Team1_KDA)
	{
		Text_Team1_KDA->SetText(GetTeamKDAFormattedText(1));
	}

	if (Text_Team0_Gold)
	{
		Text_Team0_Gold->SetText(FText::AsNumber(GetTeamStatValue(0, TEXT("GoldEarned"))));
	}
	if (Text_Team1_Gold)
	{
		Text_Team1_Gold->SetText(FText::AsNumber(GetTeamStatValue(1, TEXT("GoldEarned"))));
	}

	if (Text_Team0_Damage)
	{
		Text_Team0_Damage->SetText(FText::AsNumber(GetTeamStatValue(0, TEXT("DamageDealt"))));
	}
	if (Text_Team1_Damage)
	{
		Text_Team1_Damage->SetText(FText::AsNumber(GetTeamStatValue(1, TEXT("DamageDealt"))));
	}

	if (Text_Team0_Healing)
	{
		Text_Team0_Healing->SetText(FText::AsNumber(GetTeamStatValue(0, TEXT("HealingDone"))));
	}
	if (Text_Team1_Healing)
	{
		Text_Team1_Healing->SetText(FText::AsNumber(GetTeamStatValue(1, TEXT("HealingDone"))));
	}

	if (Text_Team0_Buildables)
	{
		Text_Team0_Buildables->SetText(FText::AsNumber(GetTeamStatValue(0, TEXT("BuildablesDestroyed"))));
	}
	if (Text_Team1_Buildables)
	{
		Text_Team1_Buildables->SetText(FText::AsNumber(GetTeamStatValue(1, TEXT("BuildablesDestroyed"))));
	}
}
