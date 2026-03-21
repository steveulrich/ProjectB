// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayResultsScreenWidget.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayResultsScreenWidget)

UBwayResultsScreenWidget::UBwayResultsScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayResultsScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Build results data
	CachedResults = GetMatchResults();
	
	// Notify Blueprint
	OnResultsReady(CachedResults);
}

FMatchResultsData UBwayResultsScreenWidget::GetMatchResults() const
{
	FMatchResultsData Results;

	ABwayGameState* GameState = GetBwayGameState();
	if (!GameState)
	{
		return Results;
	}

	// Get scores
	Results.Team1Score = GameState->GetTeamScore(0);
	Results.Team2Score = GameState->GetTeamScore(1);
	Results.TotalRounds = GameState->GetCurrentRoundNumber();

	// Determine winner
	if (Results.Team1Score > Results.Team2Score)
	{
		Results.WinningTeam = 0;
		Results.WinnerText = NSLOCTEXT("Results", "Team1Wins", "TEAM 1 WINS!");
	}
	else if (Results.Team2Score > Results.Team1Score)
	{
		Results.WinningTeam = 1;
		Results.WinnerText = NSLOCTEXT("Results", "Team2Wins", "TEAM 2 WINS!");
	}
	else
	{
		Results.WinningTeam = -1;
		Results.WinnerText = NSLOCTEXT("Results", "Draw", "DRAW!");
	}

	// Check if local player won
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			const int32 LocalTeam = GameState->GetPlayerTeam(PS);
			Results.bLocalPlayerWon = (LocalTeam == Results.WinningTeam);
		}
	}

	// Determine MVP
	DetermineMVP(Results);

	return Results;
}

bool UBwayResultsScreenWidget::DidLocalPlayerWin() const
{
	return CachedResults.bLocalPlayerWon;
}

int32 UBwayResultsScreenWidget::GetWinningTeam() const
{
	return CachedResults.WinningTeam;
}

void UBwayResultsScreenWidget::PlayAgain()
{
	OnPlayAgainRequested();

	// If level is set, travel there
	if (!PlayAgainLevel.IsNull())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(World, PlayAgainLevel);
		}
	}
	else
	{
		// Restart the current level
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->ConsoleCommand(TEXT("restartlevel"));
		}
	}
}

void UBwayResultsScreenWidget::ReturnToLobby()
{
	OnReturnToLobbyRequested();

	if (!LobbyLevel.IsNull())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(World, LobbyLevel);
		}
	}
	else
	{
		// Fallback - disconnect
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->ConsoleCommand(TEXT("disconnect"));
		}
	}
}

ABwayGameState* UBwayResultsScreenWidget::GetBwayGameState() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetGameState<ABwayGameState>();
	}
	return nullptr;
}

void UBwayResultsScreenWidget::DetermineMVP(FMatchResultsData& OutResults) const
{
	ABwayGameState* GameState = GetBwayGameState();
	if (!GameState)
	{
		return;
	}

	// Simple MVP logic: pick the first player from the winning team
	// TODO: Implement proper MVP calculation based on K/D/A, objective score, etc.
	
	if (OutResults.WinningTeam >= 0 && OutResults.WinningTeam < 2)
	{
		const FTeamInfo& WinningTeamInfo = GameState->GetTeamInfo(OutResults.WinningTeam);
		if (WinningTeamInfo.TeamMembers.Num() > 0)
		{
			if (ABwayPlayerState* MVPPS = Cast<ABwayPlayerState>(WinningTeamInfo.TeamMembers[0]))
			{
				OutResults.MVPPlayerName = FText::FromString(MVPPS->GetPlayerName());

				// Get hero name
				FPrimaryAssetId HeroId = MVPPS->GetSelectedHeroId();
				if (HeroId.IsValid())
				{
					if (UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId))
					{
						OutResults.MVPHeroName = HeroData->DisplayName;
					}
				}
			}
		}
	}
}

