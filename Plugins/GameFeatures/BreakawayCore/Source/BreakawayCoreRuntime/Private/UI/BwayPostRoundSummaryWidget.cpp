// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayPostRoundSummaryWidget.h"

#include "BwayGameState.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "UI/BwayMatchHUDWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayPostRoundSummaryWidget)

UBwayPostRoundSummaryWidget::UBwayPostRoundSummaryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayPostRoundSummaryWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBwayPostRoundSummaryWidget::ApplySummaryData(FBwayPostRoundSummaryData SummaryData)
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABwayGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ABwayGameState>() : nullptr)
		{
			if (APlayerState* LocalPS = PC->PlayerState)
			{
				const int32 LocalTeam = GameState->GetPlayerTeam(LocalPS);
				SummaryData.bLocalTeamWonRound =
					(SummaryData.RoundWinningTeam >= 0 && LocalTeam == SummaryData.RoundWinningTeam);
			}
		}
	}

	const int32 LocalTeam = UBwayMatchHUDWidgetBase::GetLocalPlayerTeamForWidget(this);
	CachedSummary = UBwayMatchHUDWidgetBase::RemapPostRoundSummaryForDisplay(SummaryData, LocalTeam);
	RefreshBoundWidgets();
	OnSummaryReady(CachedSummary);
}

FText UBwayPostRoundSummaryWidget::GetRoundOutcomeHeaderText() const
{
	return CachedSummary.bLocalTeamWonRound
		? NSLOCTEXT("PostRound", "RoundWin", "ROUND WIN")
		: NSLOCTEXT("PostRound", "RoundLoss", "ROUND LOSS");
}

FText UBwayPostRoundSummaryWidget::GetTeamKDAFormattedText(const int32 TeamIndex) const
{
	const FBwayTeamStatAggregate& TeamStats = (TeamIndex == 0) ? CachedSummary.Team0Stats : CachedSummary.Team1Stats;
	return TeamStats.GetKDAFormattedText();
}

int32 UBwayPostRoundSummaryWidget::GetTeamStatValue(const int32 TeamIndex, const FName StatName) const
{
	const FBwayPlayerMatchStats& Stats = (TeamIndex == 0)
		? CachedSummary.Team0Stats.Stats
		: CachedSummary.Team1Stats.Stats;
	return GetStatFieldValue(Stats, StatName);
}

int32 UBwayPostRoundSummaryWidget::GetStatFieldValue(const FBwayPlayerMatchStats& Stats, const FName StatName)
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

void UBwayPostRoundSummaryWidget::RefreshBoundWidgets()
{
	if (Text_Header)
	{
		Text_Header->SetText(GetRoundOutcomeHeaderText());
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
