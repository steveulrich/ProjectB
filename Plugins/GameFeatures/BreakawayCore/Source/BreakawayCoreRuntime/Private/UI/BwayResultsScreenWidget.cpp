// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayResultsScreenWidget.h"

#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "UI/BwayMatchBreakdownWidget.h"
#include "UI/BwayPostMatchInterstitialWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "PrimaryGameLayout.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "GameplayTagContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayResultsScreenWidget)

UBwayResultsScreenWidget::UBwayResultsScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InterstitialWidgetClass = TSoftClassPtr<UBwayPostMatchInterstitialWidget>(
		FSoftObjectPath(TEXT("/BreakawayCore/UI/Match/WBP_BW_MatchSummaryInterstitial.WBP_BW_MatchSummaryInterstitial_C")));
	BreakdownWidgetClass = TSoftClassPtr<UBwayMatchBreakdownWidget>(
		FSoftObjectPath(TEXT("/BreakawayCore/UI/Match/WBP_BW_MatchBreakdown.WBP_BW_MatchBreakdown_C")));
}

void UBwayResultsScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBwayResultsScreenWidget::NativeDestruct()
{
	CleanupChildWidgets();
	if (UWidget* GameplayLayer = SuppressedGameplayLayer.Get())
	{
		GameplayLayer->SetVisibility(PreviousGameplayLayerVisibility);
		SuppressedGameplayLayer.Reset();
	}
	Super::NativeDestruct();
}

void UBwayResultsScreenWidget::ApplyAuthoritativeResults(
	const FBwayPostMatchSummaryData& Summary)
{
	if (bResultsApplied)
	{
		return;
	}

	CachedSummary = Summary;
	for (FBwayMatchBreakdownPlayerColumn& Column : CachedSummary.PlayerColumns)
	{
		const UBwayHeroDataAsset* Hero = UBwayHeroRegistry::GetHeroDataById(Column.HeroId);
		Column.HeroPortrait = Hero ? Hero->Portrait : nullptr;
	}

	bResultsApplied = true;
	BeginPostMatchFlow();
}

void UBwayResultsScreenWidget::BeginPostMatchFlow()
{
	if (UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(GetOwningPlayer()))
	{
		if (UWidget* GameplayLayer = Layout->GetLayerWidget(FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Game"))))
		{
			PreviousGameplayLayerVisibility = GameplayLayer->GetVisibility();
			SuppressedGameplayLayer = GameplayLayer;
			GameplayLayer->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogTemp, Log, TEXT("BwayResultsScreen: Suppressed local gameplay layer"));
		}
	}
	OnPostMatchFlowStarted(CachedSummary);

	FMatchResultsData LegacyResults;
	LegacyResults.WinningTeam = CachedSummary.WinningTeam;
	LegacyResults.Team1Score = CachedSummary.Team0Score;
	LegacyResults.Team2Score = CachedSummary.Team1Score;
	LegacyResults.TotalRounds = CachedSummary.TotalRounds;
	LegacyResults.bLocalPlayerWon = CachedSummary.bLocalPlayerWon;
	LegacyResults.MVPPlayerName = CachedSummary.MVPPlayerName;
	LegacyResults.MVPHeroName = CachedSummary.MVPHeroName;
	LegacyResults.WinnerText = CachedSummary.bLocalPlayerWon
		? NSLOCTEXT("Results", "Victory", "VICTORY")
		: NSLOCTEXT("Results", "Defeat", "DEFEAT");
	OnResultsReady(LegacyResults);

	ShowInterstitial();

	if (CachedSummary.InterstitialDurationSeconds > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				InterstitialTimerHandle,
				this,
				&UBwayResultsScreenWidget::AdvanceToBreakdown,
				CachedSummary.InterstitialDurationSeconds,
				false);
		}
	}
	else
	{
		AdvanceToBreakdown();
	}
}

void UBwayResultsScreenWidget::ShowInterstitial()
{
	const TSubclassOf<UBwayPostMatchInterstitialWidget> LoadedClass = InterstitialWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayResultsScreen: InterstitialWidgetClass not configured — skipping interstitial"));
		return;
	}

	if (!InterstitialWidget)
	{
		InterstitialWidget = CreateWidget<UBwayPostMatchInterstitialWidget>(GetOwningPlayer(), LoadedClass);
		if (!InterstitialWidget)
		{
			return;
		}

		InterstitialWidget->AddToViewport(160);
	}

	InterstitialWidget->ApplySummaryData(CachedSummary);
	InterstitialWidget->ActivateWidget();
	InterstitialWidget->SetUserFocus(GetOwningPlayer());
}

void UBwayResultsScreenWidget::AdvanceToBreakdown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InterstitialTimerHandle);
	}

	if (InterstitialWidget)
	{
		InterstitialWidget->DeactivateWidget();
		InterstitialWidget->RemoveFromParent();
		InterstitialWidget = nullptr;
	}

	ShowBreakdown();
}

void UBwayResultsScreenWidget::ShowBreakdown()
{
	const TSubclassOf<UBwayMatchBreakdownWidget> LoadedClass = BreakdownWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayResultsScreen: BreakdownWidgetClass not configured"));
		return;
	}

	if (!BreakdownWidget)
	{
		BreakdownWidget = CreateWidget<UBwayMatchBreakdownWidget>(GetOwningPlayer(), LoadedClass);
		if (!BreakdownWidget)
		{
			return;
		}

		BreakdownWidget->AddToViewport(160);
		BreakdownWidget->OnReturnToLobbyRequested.AddDynamic(this, &UBwayResultsScreenWidget::HandleBreakdownReturnToLobby);
		BreakdownWidget->OnPlayAgainRequested.AddDynamic(this, &UBwayResultsScreenWidget::HandleBreakdownPlayAgain);
	}

	BreakdownWidget->ApplyBreakdownData(CachedSummary);
	BreakdownWidget->ActivateWidget();
	UWidget* FocusTarget = BreakdownWidget->GetDesiredFocusTarget();
	(FocusTarget ? FocusTarget : BreakdownWidget.Get())->SetUserFocus(GetOwningPlayer());
}

void UBwayResultsScreenWidget::CleanupChildWidgets()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InterstitialTimerHandle);
	}

	if (InterstitialWidget)
	{
		InterstitialWidget->DeactivateWidget();
		InterstitialWidget->RemoveFromParent();
		InterstitialWidget = nullptr;
	}

	if (BreakdownWidget)
	{
		BreakdownWidget->DeactivateWidget();
		BreakdownWidget->OnReturnToLobbyRequested.RemoveDynamic(this, &UBwayResultsScreenWidget::HandleBreakdownReturnToLobby);
		BreakdownWidget->OnPlayAgainRequested.RemoveDynamic(this, &UBwayResultsScreenWidget::HandleBreakdownPlayAgain);
		BreakdownWidget->RemoveFromParent();
		BreakdownWidget = nullptr;
	}
}

FMatchResultsData UBwayResultsScreenWidget::GetMatchResults() const
{
	FMatchResultsData Results;
	Results.WinningTeam = CachedSummary.WinningTeam;
	Results.Team1Score = CachedSummary.Team0Score;
	Results.Team2Score = CachedSummary.Team1Score;
	Results.TotalRounds = CachedSummary.TotalRounds;
	Results.bLocalPlayerWon = CachedSummary.bLocalPlayerWon;
	Results.MVPPlayerName = CachedSummary.MVPPlayerName;
	Results.MVPHeroName = CachedSummary.MVPHeroName;
	Results.WinnerText = CachedSummary.bLocalPlayerWon
		? NSLOCTEXT("Results", "Victory", "VICTORY")
		: NSLOCTEXT("Results", "Defeat", "DEFEAT");
	return Results;
}

void UBwayResultsScreenWidget::PlayAgain()
{
	OnPlayAgainRequested();

	if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(GetOwningPlayer()))
	{
		PC->Server_RequestPlayAgain();
	}
}

void UBwayResultsScreenWidget::ReturnToLobby()
{
	OnReturnToLobbyRequested();

	if (ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(GetOwningPlayer()))
	{
		BwayPC->Server_RequestReturnToFrontEnd();
		return;
	}

	if (ABwayGameState* GameState = GetBwayGameState())
	{
		GameState->ReturnToFrontEnd();
	}
	else if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ConsoleCommand(TEXT("disconnect"));
	}
}

void UBwayResultsScreenWidget::HandleBreakdownReturnToLobby()
{
	ReturnToLobby();
}

void UBwayResultsScreenWidget::HandleBreakdownPlayAgain()
{
	PlayAgain();
}

ABwayGameState* UBwayResultsScreenWidget::GetBwayGameState() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetGameState<ABwayGameState>();
	}

	return nullptr;
}
