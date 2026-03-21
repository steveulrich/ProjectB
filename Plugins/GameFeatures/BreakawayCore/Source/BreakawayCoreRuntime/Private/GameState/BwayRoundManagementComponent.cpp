// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayRoundManagementComponent.h"
#include "BwayGameState.h"
#include "Relic/RelicActor.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "SpawnSystem/BwaySpawnPointManagerComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRoundManagementComponent)

UBwayRoundManagementComponent::UBwayRoundManagementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBwayRoundManagementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() == ROLE_Authority && bAutoStartFirstRound)
	{
		GetWorld()->GetTimerManager().SetTimer(
			PreRoundTimerHandle, this, &UBwayRoundManagementComponent::StartRound, PreRoundDelay, false);
		
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: First round will start in %.1f seconds"), PreRoundDelay);
	}
}

void UBwayRoundManagementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS || BwayGS->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return;
	}

	// Check if round time has expired
	if (BwayGS->GetRoundTimeRemaining() <= 0)
	{
		OnRoundTimerExpired();
	}
}

ABwayGameState* UBwayRoundManagementComponent::GetBwayGameState() const
{
	return Cast<ABwayGameState>(GetOwner());
}

void UBwayRoundManagementComponent::StartRound()
{
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRoundManagement: Cannot start round - GameState is null"));
		return;
	}

	const int32 NextRoundNumber = BwayGS->GetCurrentRoundNumber() + 1;
	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Starting Round %d"), NextRoundNumber);

	ResetRoundState();
	BwayGS->SetRoundState(ERoundState::RoundActive);

	OnRoundStarted.Broadcast(NextRoundNumber, BwayGS->RoundDuration);
}

void UBwayRoundManagementComponent::EndRound(int32 WinningTeam, EBwayWinCondition WinCondition)
{
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS)
	{
		return;
	}

	BwayGS->SetRoundState(ERoundState::RoundEnding);

	// Award points via ScoringComponent if available
	if (UBwayScoringComponent* Scoring = GetOwner()->FindComponentByClass<UBwayScoringComponent>())
	{
		Scoring->AddScore(WinningTeam, 1);
	}
	else
	{
		// Fallback to direct GameState scoring
		BwayGS->AddScore(WinningTeam, 1);
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Round ended! Team %d wins"), WinningTeam + 1);

	OnRoundEnded.Broadcast(WinningTeam, WinCondition, BwayGS->GetCurrentRoundNumber());

	if (CheckMatchEnd())
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Match Over! Team %d wins!"), WinningTeam + 1);
		BwayGS->SetRoundState(ERoundState::RoundComplete);
		return;
	}

	BwayGS->SetRoundState(ERoundState::RoundComplete);

	// Schedule next round
	FTimerHandle UnusedHandle;
	GetWorld()->GetTimerManager().SetTimer(
		UnusedHandle, this, &UBwayRoundManagementComponent::StartRound, 
		BwayGS->RoundEndDelay, false);
}

void UBwayRoundManagementComponent::ResetRoundState()
{
	// Reset relic via RelicManagerComponent if available
	if (UBwayRelicManagerComponent* RelicMgr = GetOwner()->FindComponentByClass<UBwayRelicManagerComponent>())
	{
		RelicMgr->ResetRelic();
	}

	// Respawn all players
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
			{
				GM->RestartPlayer(PC);
			}
		}
	}
}

void UBwayRoundManagementComponent::OnRelicScored(int32 ScoringTeam)
{
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS || BwayGS->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Goal! Team %d scored"), ScoringTeam + 1);
	EndRound(ScoringTeam, EBwayWinCondition::GoalScored);
}

void UBwayRoundManagementComponent::CheckTeamElimination()
{
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS || BwayGS->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return;
	}

	for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
	{
		const FTeamInfo& TeamInfo = BwayGS->GetTeamInfo(TeamIndex);
		if (TeamInfo.TeamMembers.Num() > 0 && TeamInfo.AlivePlayerCount == 0)
		{
			const int32 WinningTeam = (TeamIndex == 0) ? 1 : 0;
			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Team %d eliminated!"), TeamIndex + 1);
			EndRound(WinningTeam, EBwayWinCondition::TeamEliminated);
			return;
		}
	}
}

void UBwayRoundManagementComponent::OnRoundTimerExpired()
{
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS || BwayGS->GetCurrentRoundState() != ERoundState::RoundActive)
	{
		return;
	}

	const int32 PossessingTeam = DetermineRelicPossessionTeam();
	if (PossessingTeam >= 0)
	{
		EndRound(PossessingTeam, EBwayWinCondition::TimeExpired);
	}
	else
	{
		// Neutral — no winner, start new round
		BwayGS->SetRoundState(ERoundState::RoundComplete);
		FTimerHandle UnusedHandle;
		GetWorld()->GetTimerManager().SetTimer(
			UnusedHandle, this, &UBwayRoundManagementComponent::StartRound,
			BwayGS->RoundEndDelay, false);
	}
}

bool UBwayRoundManagementComponent::CheckMatchEnd() const
{
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS)
	{
		return false;
	}

	for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
	{
		if (BwayGS->GetTeamScore(TeamIndex) >= PointsToWin)
		{
			return true;
		}
	}
	return false;
}

int32 UBwayRoundManagementComponent::DetermineRelicPossessionTeam() const
{
	// Try the RelicManagerComponent first
	if (const UBwayRelicManagerComponent* RelicMgr = GetOwner()->FindComponentByClass<UBwayRelicManagerComponent>())
	{
		return RelicMgr->GetRelicPossessingTeam();
	}

	// Fallback to GameState
	if (const ABwayGameState* BwayGS = GetBwayGameState())
	{
		return BwayGS->GetRelicPossessingTeam();
	}
	return -1;
}
