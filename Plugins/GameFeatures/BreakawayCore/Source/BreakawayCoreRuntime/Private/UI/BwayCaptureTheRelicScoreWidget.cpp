// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayCaptureTheRelicScoreWidget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCaptureTheRelicScoreWidget)

UBwayCaptureTheRelicScoreWidget::UBwayCaptureTheRelicScoreWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayCaptureTheRelicScoreWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshScoreDisplay();
	RefreshRoundLabelDisplay();

	if (bShowRoundTimer)
	{
		RefreshTimerDisplay();

		if (TimerPollInterval > 0.0f)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					TimerPollHandle,
					this,
					&UBwayCaptureTheRelicScoreWidget::RefreshTimerDisplay,
					TimerPollInterval,
					true);
			}
		}
	}
}

void UBwayCaptureTheRelicScoreWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerPollHandle);
	}

	Super::NativeDestruct();
}

void UBwayCaptureTheRelicScoreWidget::RefreshScoreDisplay()
{
	const int32 LocalTeam = GetLocalPlayerTeamForWidget(this);
	const int32 LeftScore = GetDisplayTeamScore(GetWorld(), 0, LocalTeam);
	const int32 RightScore = GetDisplayTeamScore(GetWorld(), 1, LocalTeam);

	UpdateBoundScoreTexts(LeftScore, RightScore);
	OnScoreChanged(LeftScore, RightScore);
}

void UBwayCaptureTheRelicScoreWidget::RefreshTimerDisplay()
{
	const int32 SecondsRemaining = GetRoundTimeRemainingFromWorld(GetWorld());
	UpdateBoundTimerText(SecondsRemaining);
	OnRoundTimeUpdated(SecondsRemaining, FormatRoundTime(SecondsRemaining));
}

void UBwayCaptureTheRelicScoreWidget::NotifyTeamScoreChanged(int32 TeamIndex, int32 NewScore)
{
	RefreshScoreDisplay();
}

void UBwayCaptureTheRelicScoreWidget::NotifyRoundTimeChanged(int32 SecondsRemaining)
{
	if (bShowRoundTimer)
	{
		UpdateBoundTimerText(SecondsRemaining);
		OnRoundTimeUpdated(SecondsRemaining, FormatRoundTime(SecondsRemaining));
	}
}

void UBwayCaptureTheRelicScoreWidget::NotifyRoundStateChanged(FName NewState)
{
	RefreshRoundLabelDisplay();
}

void UBwayCaptureTheRelicScoreWidget::RefreshRoundLabelDisplay()
{
	UpdateBoundRoundLabelText();
}

void UBwayCaptureTheRelicScoreWidget::UpdateBoundRoundLabelText()
{
	if (Text_RoundLabel)
	{
		const int32 RoundNumber = FMath::Max(GetCurrentRoundNumberFromWorld(GetWorld()), 1);
		Text_RoundLabel->SetText(FText::Format(
			NSLOCTEXT("BwayCTRScore", "RoundLabel", "ROUND {0}"),
			FText::AsNumber(RoundNumber)));
	}
}

void UBwayCaptureTheRelicScoreWidget::UpdateBoundScoreTexts(int32 Team1Score, int32 Team2Score)
{
	if (Text_Team1Score)
	{
		Text_Team1Score->SetText(FText::AsNumber(Team1Score));
	}

	if (Text_Team2Score)
	{
		Text_Team2Score->SetText(FText::AsNumber(Team2Score));
	}
}

void UBwayCaptureTheRelicScoreWidget::UpdateBoundTimerText(int32 SecondsRemaining)
{
	if (Text_Timer)
	{
		Text_Timer->SetText(FormatRoundTime(SecondsRemaining));
	}
}
