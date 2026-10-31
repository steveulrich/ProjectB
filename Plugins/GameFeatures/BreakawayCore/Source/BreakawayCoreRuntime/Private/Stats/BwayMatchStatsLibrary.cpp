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

	TSet<int32> SeenPlayerIds;
	for (APlayerState* PS : GameState->PlayerArray)
	{
		const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
		if (!BwayPS)
		{
			continue;
		}
		SeenPlayerIds.Add(BwayPS->GetPlayerId());
		if (GameState->GetPlayerTeam(BwayPS) != TeamIndex)
		{
			continue;
		}

		const FBwayPlayerMatchStats PlayerStats = bRoundOnly
			? BwayPS->GetLastRoundStats()
			: BwayPS->GetMatchStatsSnapshot();
		Aggregate.Stats.Add(PlayerStats);
	}

	if (!bRoundOnly)
	{
		for (const FBwayMatchBreakdownPlayerColumn& Departed : GameState->GetDisconnectedMatchParticipants())
		{
			if (Departed.GameTeamIndex == TeamIndex && !SeenPlayerIds.Contains(Departed.PlayerId))
			{
				Aggregate.Stats.Add(Departed.Stats);
			}
		}
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
		Summary.Team0RoundWinConditions = RoundMgmt->GetTeamRoundWinHistory(0);
		Summary.Team1RoundWinConditions = RoundMgmt->GetTeamRoundWinHistory(1);
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

float UBwayMatchStatsLibrary::ComputePlayerMVPScore(const FBwayPlayerMatchStats& Stats)
{
	return CalculateMVPScore(Stats);
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

	float BestMVPScore = -FLT_MAX;
	int32 BestPlayerId = MAX_int32;
	int32 BestColumnIndex = INDEX_NONE;
	TSet<int32> SeenPlayerIds;

	auto ConsiderColumnForMVP = [&](const FBwayMatchBreakdownPlayerColumn& Column, int32 ColumnIndex)
	{
		// Always choose a player, even when every score is negative. Break ties
		// by replicated identity so PlayerArray ordering cannot change the MVP.
		if (BestColumnIndex == INDEX_NONE
			|| Column.MVPScore > BestMVPScore
			|| (FMath::IsNearlyEqual(Column.MVPScore, BestMVPScore) && Column.PlayerId < BestPlayerId))
		{
			BestMVPScore = Column.MVPScore;
			BestPlayerId = Column.PlayerId;
			BestColumnIndex = ColumnIndex;
		}
	};

	for (APlayerState* PS : GameState->PlayerArray)
	{
		const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
		if (!BwayPS)
		{
			continue;
		}

		SeenPlayerIds.Add(BwayPS->GetPlayerId());
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
		Column.bIsConnected = true;
		Column.PlayerName = FText::FromString(BwayPS->GetPlayerName());
		Column.Stats = BwayPS->GetMatchStatsSnapshot();
		Column.MVPScore = CalculateMVPScore(Column.Stats);
		Column.EquippedUpgrades = BwayPS->GetReplicatedEquippedUpgrades();

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

		ConsiderColumnForMVP(Column, InOutSummary.PlayerColumns.Num());
		InOutSummary.PlayerColumns.Add(Column);
	}

	for (const FBwayMatchBreakdownPlayerColumn& Departed : GameState->GetDisconnectedMatchParticipants())
	{
		if (Departed.GameTeamIndex < 0 || SeenPlayerIds.Contains(Departed.PlayerId))
		{
			continue;
		}

		FBwayMatchBreakdownPlayerColumn Column = Departed;
		Column.DisplayColumnIndex = UBwayMatchHUDWidgetBase::MapGameTeamToDisplaySlot(Departed.GameTeamIndex, LocalTeam);
		Column.bIsLocalPlayer = false;
		Column.bIsConnected = false;
		Column.MVPScore = CalculateMVPScore(Column.Stats);
		if (Column.HeroId.IsValid())
		{
			if (const UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(Column.HeroId))
			{
				Column.HeroName = HeroData->DisplayName;
				Column.HeroPortrait = HeroData->Portrait;
			}
		}

		ConsiderColumnForMVP(Column, InOutSummary.PlayerColumns.Num());
		InOutSummary.PlayerColumns.Add(Column);
	}

	// Mark the chosen entry before sorting; a display name can belong to several players.
	if (InOutSummary.PlayerColumns.IsValidIndex(BestColumnIndex))
	{
		InOutSummary.PlayerColumns[BestColumnIndex].bIsMVP = true;
		InOutSummary.MVPPlayerId = InOutSummary.PlayerColumns[BestColumnIndex].PlayerId;
		InOutSummary.MVPPlayerName = InOutSummary.PlayerColumns[BestColumnIndex].PlayerName;
		InOutSummary.MVPHeroName = InOutSummary.PlayerColumns[BestColumnIndex].HeroName;
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
