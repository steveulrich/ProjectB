// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayScoreboardWidget.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "GameState/BwayScoringComponent.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayScoreboardWidget)

UBwayScoreboardWidget::UBwayScoreboardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayScoreboardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initial refresh
	RefreshScoreboardData();
}

void UBwayScoreboardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Auto-refresh at interval
	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh >= RefreshInterval)
	{
		RefreshScoreboardData();
		TimeSinceLastRefresh = 0.0f;
	}
}

TArray<FScoreboardPlayerData> UBwayScoreboardWidget::GetTeamPlayers(int32 TeamIndex) const
{
	TArray<FScoreboardPlayerData> Players;

	ABwayGameState* GameState = GetBwayGameState();
	if (!GameState)
	{
		return Players;
	}

	const FTeamInfo& TeamInfo = GameState->GetTeamInfo(TeamIndex);
	for (APlayerState* PS : TeamInfo.TeamMembers)
	{
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
		{
			Players.Add(BuildPlayerData(BwayPS, TeamIndex));
		}
	}

	return Players;
}

int32 UBwayScoreboardWidget::GetTeam1Score() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			return Scoring->GetTeamScore(0);
		}
	}
	return 0;
}

int32 UBwayScoreboardWidget::GetTeam2Score() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
		{
			return Scoring->GetTeamScore(1);
		}
	}
	return 0;
}

int32 UBwayScoreboardWidget::GetCurrentRound() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		return GameState->GetCurrentRoundNumber();
	}
	return 0;
}

FText UBwayScoreboardWidget::GetMatchTimeElapsed() const
{
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		// Calculate elapsed time from server world time
		const float ElapsedSeconds = GameState->GetServerWorldTimeSeconds();
		const int32 Minutes = static_cast<int32>(ElapsedSeconds) / 60;
		const int32 Seconds = static_cast<int32>(ElapsedSeconds) % 60;
		
		return FText::Format(
			NSLOCTEXT("Scoreboard", "TimeFormat", "{0}:{1}"),
			FText::AsNumber(Minutes),
			FText::FromString(FString::Printf(TEXT("%02d"), Seconds))
		);
	}
	return FText::GetEmpty();
}

void UBwayScoreboardWidget::RefreshScoreboardData()
{
	TArray<FScoreboardPlayerData> Team1Players = GetTeamPlayers(0);
	TArray<FScoreboardPlayerData> Team2Players = GetTeamPlayers(1);

	OnScoreboardDataRefreshed(Team1Players, Team2Players);
}

ABwayGameState* UBwayScoreboardWidget::GetBwayGameState() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetGameState<ABwayGameState>();
	}
	return nullptr;
}

FScoreboardPlayerData UBwayScoreboardWidget::BuildPlayerData(ABwayPlayerState* PlayerState, int32 TeamIndex) const
{
	FScoreboardPlayerData Data;

	if (!PlayerState)
	{
		return Data;
	}

	Data.PlayerName = FText::FromString(PlayerState->GetPlayerName());
	Data.TeamIndex = TeamIndex;
	Data.Ping = static_cast<int32>(PlayerState->GetPingInMilliseconds());

	// Check if this is the local player
	if (APlayerController* LocalPC = GetOwningPlayer())
	{
		Data.bIsLocalPlayer = (LocalPC->PlayerState == PlayerState);
	}

	// Get hero name
	FPrimaryAssetId HeroId = PlayerState->GetSelectedHeroId();
	if (HeroId.IsValid())
	{
		if (UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId))
		{
			Data.HeroName = HeroData->DisplayName;
		}
		else
		{
			Data.HeroName = FText::FromString(HeroId.PrimaryAssetName.ToString());
		}
	}

	// TODO: Get K/D/A and objective stats from player state when those properties are added
	// For now, these default to 0

	// Check alive status from game state
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		const FTeamInfo& TeamInfo = GameState->GetTeamInfo(TeamIndex);
		// Simple check - if player is in team members but team has fewer alive than total
		// This is a simplified check; ideally track per-player alive state
		Data.bIsAlive = true; // Default to alive
	}

	return Data;
}

