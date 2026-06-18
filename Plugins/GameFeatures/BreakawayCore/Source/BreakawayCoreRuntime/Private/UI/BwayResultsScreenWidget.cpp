// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayResultsScreenWidget.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "GameState/BwayScoringComponent.h"
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
	// Authoritative results are applied via ApplyAuthoritativeResults from Client_ShowResults.
}

void UBwayResultsScreenWidget::ApplyAuthoritativeResults(int32 WinningTeam, int32 Team1Score, int32 Team2Score, int32 TotalRounds)
{
	if (bResultsApplied)
	{
		return;
	}

	CachedResults.WinningTeam = WinningTeam;
	CachedResults.Team1Score = Team1Score;
	CachedResults.Team2Score = Team2Score;
	CachedResults.TotalRounds = TotalRounds;

	if (WinningTeam == 0)
	{
		CachedResults.WinnerText = NSLOCTEXT("Results", "Team1Wins", "TEAM 1 WINS!");
	}
	else if (WinningTeam == 1)
	{
		CachedResults.WinnerText = NSLOCTEXT("Results", "Team2Wins", "TEAM 2 WINS!");
	}
	else
	{
		CachedResults.WinnerText = NSLOCTEXT("Results", "Draw", "DRAW!");
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ABwayGameState* GameState = GetBwayGameState())
		{
			if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
			{
				const int32 LocalTeam = GameState->GetPlayerTeam(PS);
				CachedResults.bLocalPlayerWon = (WinningTeam >= 0 && LocalTeam == WinningTeam);
			}
		}
	}

	DetermineMVP(CachedResults);

	bResultsApplied = true;
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

	// Get scores from ScoringComponent
	if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
	{
		Results.Team1Score = Scoring->GetTeamScore(0);
		Results.Team2Score = Scoring->GetTeamScore(1);
	}
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

	// Always route through the PC RPC so non-authority clients work correctly.
	if (ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(GetOwningPlayer()))
	{
		BwayPC->Server_RequestReturnToFrontEnd();
		return;
	}

	// Listen-host / standalone fallback.
	if (ABwayGameState* GameState = GetBwayGameState())
	{
		GameState->ReturnToFrontEnd();
	}
	else if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ConsoleCommand(TEXT("disconnect"));
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

	ABwayPlayerState* BestPlayer = nullptr;
	float BestScore = -1.0f;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
		if (!BwayPS)
		{
			continue;
		}

		const float MVPScore =
			static_cast<float>(BwayPS->GetKills()) * 2.0f +
			static_cast<float>(BwayPS->GetAssists()) * 1.0f +
			static_cast<float>(BwayPS->GetObjectiveScore()) * 3.0f -
			static_cast<float>(BwayPS->GetDeaths()) * 0.5f;

		if (MVPScore > BestScore)
		{
			BestScore = MVPScore;
			BestPlayer = BwayPS;
		}
	}

	if (!BestPlayer)
	{
		return;
	}

	OutResults.MVPPlayerName = FText::FromString(BestPlayer->GetPlayerName());

	FPrimaryAssetId HeroId = BestPlayer->GetSelectedHeroId();
	if (HeroId.IsValid())
	{
		if (UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId))
		{
			OutResults.MVPHeroName = HeroData->DisplayName;
		}
	}
}

