// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stats/BwayMatchStatsLibrary.h"

#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UI/BwayMatchHUDWidgetBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchStatsLibrary)

ABwayGameState* UBwayMatchStatsLibrary::ResolveGameState(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	return Cast<ABwayGameState>(UGameplayStatics::GetGameState(WorldContextObject));
}

APlayerController* ResolveLocalPlayerController(const UObject* WorldContextObject)
{
	if (const UUserWidget* Widget = Cast<UUserWidget>(WorldContextObject))
	{
		if (APlayerController* PC = Widget->GetOwningPlayer())
		{
			return PC;
		}
	}

	return UGameplayStatics::GetPlayerController(WorldContextObject, 0);
}

FBwayPlayerMatchStats UBwayMatchStatsLibrary::GetPlayerMatchStats(const ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return FBwayPlayerMatchStats();
	}

	return PlayerState->GetMatchStatsSnapshot();
}

FBwayPlayerMatchStats UBwayMatchStatsLibrary::GetPlayerLastRoundStats(const ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return FBwayPlayerMatchStats();
	}

	return PlayerState->GetLastRoundStats();
}

FBwayTeamStatAggregate UBwayMatchStatsLibrary::AggregateTeamStatsFromGameState(
	const ABwayGameState* GameState,
	const int32 TeamIndex,
	const bool bRoundOnly)
{
	FBwayTeamStatAggregate Aggregate;
	Aggregate.TeamIndex = TeamIndex;

	if (!GameState || TeamIndex < 0)
	{
		return Aggregate;
	}

	for (APlayerState* PS : GameState->PlayerArray)
	{
		const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
		if (!BwayPS || GameState->GetPlayerTeam(BwayPS) != TeamIndex)
		{
			continue;
		}

		const FBwayPlayerMatchStats PlayerStats = bRoundOnly
			? BwayPS->GetLastRoundStats()
			: BwayPS->GetMatchStatsSnapshot();
		Aggregate.Stats.Add(PlayerStats);
	}

	return Aggregate;
}

FBwayTeamStatAggregate UBwayMatchStatsLibrary::AggregateTeamStats(
	const UObject* WorldContextObject,
	const int32 TeamIndex,
	const bool bRoundOnly)
{
	return AggregateTeamStatsFromGameState(ResolveGameState(WorldContextObject), TeamIndex, bRoundOnly);
}

FBwayTeamStatAggregate UBwayMatchStatsLibrary::AggregateTeamRoundStatsForPostRoundSummary(
	const UObject* WorldContextObject,
	const int32 TeamIndex)
{
	FBwayTeamStatAggregate Aggregate = AggregateTeamStats(WorldContextObject, TeamIndex, true);
	Aggregate.Stats.RelicScores = 0;
	return Aggregate;
}

FBwayPostRoundSummaryData UBwayMatchStatsLibrary::BuildPostRoundSummaryData(
	const UObject* WorldContextObject,
	const int32 CompletedRoundNumber,
	const int32 RoundWinningTeam,
	const float DisplayDurationSeconds)
{
	FBwayPostRoundSummaryData Summary;
	Summary.CompletedRoundNumber = CompletedRoundNumber;
	Summary.RoundWinningTeam = RoundWinningTeam;
	Summary.DisplayDurationSeconds = DisplayDurationSeconds;

	ABwayGameState* GameState = ResolveGameState(WorldContextObject);
	if (!GameState)
	{
		return Summary;
	}

	if (const UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
	{
		Summary.Team0Score = Scoring->GetTeamScore(0);
		Summary.Team1Score = Scoring->GetTeamScore(1);
	}

	if (const UBwayRoundManagementComponent* RoundMgmt = GameState->FindComponentByClass<UBwayRoundManagementComponent>())
	{
		Summary.PointsToWin = RoundMgmt->PointsToWin;
	}

	Summary.Team0Stats = AggregateTeamRoundStatsForPostRoundSummary(WorldContextObject, 0);
	Summary.Team1Stats = AggregateTeamRoundStatsForPostRoundSummary(WorldContextObject, 1);

	if (APlayerController* PC = ResolveLocalPlayerController(WorldContextObject))
	{
		if (APlayerState* LocalPS = PC->PlayerState)
		{
			const int32 LocalTeam = GameState->GetPlayerTeam(LocalPS);
			Summary.bLocalTeamWonRound = (RoundWinningTeam >= 0 && LocalTeam == RoundWinningTeam);
		}
	}

	return Summary;
}

float UBwayMatchStatsLibrary::CalculateMVPScore(const FBwayPlayerMatchStats& Stats)
{
	return static_cast<float>(Stats.Kills) * 2.0f
		+ static_cast<float>(Stats.Assists) * 1.0f
		+ static_cast<float>(Stats.RelicScores) * 3.0f
		- static_cast<float>(Stats.Deaths) * 0.5f;
}

void UBwayMatchStatsLibrary::PopulateMVPAndColumns(
	const UObject* WorldContextObject,
	const ABwayGameState* GameState,
	FBwayPostMatchSummaryData& InOutSummary)
{
	if (!GameState)
	{
		return;
	}

	int32 LocalTeam = INDEX_NONE;
	const APlayerState* LocalPlayerState = nullptr;
	if (APlayerController* PC = ResolveLocalPlayerController(WorldContextObject))
	{
		if (APlayerState* LocalPS = PC->PlayerState)
		{
			LocalTeam = GameState->GetPlayerTeam(LocalPS);
			LocalPlayerState = LocalPS;
		}
	}

	InOutSummary.PlayerColumns.Reset();

	const ABwayPlayerState* BestPlayer = nullptr;
	float BestMVPScore = -1.0f;
	int32 BestColumnIndex = INDEX_NONE;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
		if (!BwayPS)
		{
			continue;
		}

		const int32 GameTeamIndex = GameState->GetPlayerTeam(BwayPS);
		if (GameTeamIndex < 0)
		{
			continue;
		}

		FBwayMatchBreakdownPlayerColumn Column;
		Column.PlayerId = BwayPS->GetPlayerId();
		Column.GameTeamIndex = GameTeamIndex;
		Column.DisplayColumnIndex = UBwayMatchHUDWidgetBase::MapGameTeamToDisplaySlot(GameTeamIndex, LocalTeam);
		Column.bIsLocalPlayer = (BwayPS == LocalPlayerState);
		Column.PlayerName = FText::FromString(BwayPS->GetPlayerName());
		Column.Stats = BwayPS->GetMatchStatsSnapshot();
		Column.MVPScore = CalculateMVPScore(Column.Stats);

		const FPrimaryAssetId HeroId = BwayPS->GetSelectedHeroId();
		Column.HeroId = HeroId;
		if (HeroId.IsValid())
		{
			if (const UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId))
			{
				Column.HeroName = HeroData->DisplayName;
				Column.HeroPortrait = HeroData->Portrait;
			}
		}

		// Always choose a player, even when every score is negative. Break ties
		// by replicated identity so PlayerArray ordering cannot change the MVP.
		if (!BestPlayer || Column.MVPScore > BestMVPScore
			|| (Column.MVPScore == BestMVPScore && Column.PlayerId < BestPlayer->GetPlayerId()))
		{
			BestMVPScore = Column.MVPScore;
			BestPlayer = BwayPS;
			BestColumnIndex = InOutSummary.PlayerColumns.Num();
		}

		InOutSummary.PlayerColumns.Add(Column);
	}

	// Mark the chosen entry before sorting; a display name can belong to several players.
	if (InOutSummary.PlayerColumns.IsValidIndex(BestColumnIndex))
	{
		InOutSummary.PlayerColumns[BestColumnIndex].bIsMVP = true;
	}

	InOutSummary.PlayerColumns.Sort([](const FBwayMatchBreakdownPlayerColumn& A, const FBwayMatchBreakdownPlayerColumn& B)
	{
		if (A.DisplayColumnIndex != B.DisplayColumnIndex)
		{
			return A.DisplayColumnIndex < B.DisplayColumnIndex;
		}

		const int32 NameOrder = A.PlayerName.ToString().Compare(B.PlayerName.ToString());
		return NameOrder != 0 ? NameOrder < 0 : A.PlayerId < B.PlayerId;
	});

	if (BestPlayer)
	{
		InOutSummary.MVPPlayerName = FText::FromString(BestPlayer->GetPlayerName());

		const FPrimaryAssetId HeroId = BestPlayer->GetSelectedHeroId();
		if (HeroId.IsValid())
		{
			if (const UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId))
			{
				InOutSummary.MVPHeroName = HeroData->DisplayName;
			}
		}
	}
}

FBwayPostMatchSummaryData UBwayMatchStatsLibrary::BuildPostMatchSummaryData(
	const UObject* WorldContextObject,
	const int32 WinningTeam,
	const int32 TotalRounds,
	const float InterstitialDurationSeconds)
{
	FBwayPostMatchSummaryData Summary;
	Summary.WinningTeam = WinningTeam;
	Summary.TotalRounds = TotalRounds;
	Summary.InterstitialDurationSeconds = InterstitialDurationSeconds;

	ABwayGameState* GameState = ResolveGameState(WorldContextObject);
	if (!GameState)
	{
		return Summary;
	}

	if (const UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
	{
		Summary.Team0Score = Scoring->GetTeamScore(0);
		Summary.Team1Score = Scoring->GetTeamScore(1);
	}

	if (const UBwayRoundManagementComponent* RoundMgmt = GameState->FindComponentByClass<UBwayRoundManagementComponent>())
	{
		Summary.PointsToWin = RoundMgmt->PointsToWin;
	}

	Summary.Team0MatchStats = AggregateTeamStats(WorldContextObject, 0, false);
	Summary.Team1MatchStats = AggregateTeamStats(WorldContextObject, 1, false);

	if (APlayerController* PC = ResolveLocalPlayerController(WorldContextObject))
	{
		if (APlayerState* LocalPS = PC->PlayerState)
		{
			const int32 LocalTeam = GameState->GetPlayerTeam(LocalPS);
			Summary.LocalPlayerTeamIndex = LocalTeam;
			Summary.bLocalPlayerWon = (WinningTeam >= 0 && LocalTeam == WinningTeam);
		}
	}

	PopulateMVPAndColumns(WorldContextObject, GameState, Summary);
	return Summary;
}

FBwayPostMatchSummaryData UBwayMatchStatsLibrary::RemapPostMatchSummaryForDisplay(
	const FBwayPostMatchSummaryData& Summary,
	const int32 LocalPlayerTeamIndex)
{
	FBwayPostMatchSummaryData DisplaySummary = Summary;

	if (UBwayMatchHUDWidgetBase::ShouldSwapTeamsForDisplay(LocalPlayerTeamIndex))
	{
		Swap(DisplaySummary.Team0Score, DisplaySummary.Team1Score);
		Swap(DisplaySummary.Team0MatchStats, DisplaySummary.Team1MatchStats);
	}

	for (FBwayMatchBreakdownPlayerColumn& Column : DisplaySummary.PlayerColumns)
	{
		Column.DisplayColumnIndex = UBwayMatchHUDWidgetBase::MapGameTeamToDisplaySlot(
			Column.GameTeamIndex,
			LocalPlayerTeamIndex);
	}

	DisplaySummary.PlayerColumns.Sort([](const FBwayMatchBreakdownPlayerColumn& A, const FBwayMatchBreakdownPlayerColumn& B)
	{
		if (A.DisplayColumnIndex != B.DisplayColumnIndex)
		{
			return A.DisplayColumnIndex < B.DisplayColumnIndex;
		}

		const int32 NameOrder = A.PlayerName.ToString().Compare(B.PlayerName.ToString());
		return NameOrder != 0 ? NameOrder < 0 : A.PlayerId < B.PlayerId;
	});

	return DisplaySummary;
}

FBwayPostRoundSummaryData UBwayMatchStatsLibrary::RemapPostRoundSummaryForDisplay(
	const FBwayPostRoundSummaryData& Summary,
	const int32 LocalPlayerTeamIndex)
{
	return UBwayMatchHUDWidgetBase::RemapPostRoundSummaryForDisplay(Summary, LocalPlayerTeamIndex);
}
