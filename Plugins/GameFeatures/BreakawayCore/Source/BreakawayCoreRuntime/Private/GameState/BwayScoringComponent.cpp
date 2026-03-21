// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayScoringComponent.h"
#include "BwayGameState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayScoringComponent)

UBwayScoringComponent::UBwayScoringComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UBwayScoringComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() == ROLE_Authority)
	{
		// Initialize scores for 2 teams
		TeamScores.SetNum(2);
		TeamScores[0] = 0;
		TeamScores[1] = 0;
	}
}

void UBwayScoringComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBwayScoringComponent, TeamScores);
}

void UBwayScoringComponent::AddScore(int32 TeamIndex, int32 Points)
{
	if (GetOwnerRole() != ROLE_Authority || !TeamScores.IsValidIndex(TeamIndex))
	{
		return;
	}

	TeamScores[TeamIndex] += Points;
	
	UE_LOG(LogTemp, Log, TEXT("BwayScoring: Team %d scored! New score: %d"), TeamIndex + 1, TeamScores[TeamIndex]);
	
	OnTeamScoreChanged.Broadcast(TeamIndex, TeamScores[TeamIndex]);
}

int32 UBwayScoringComponent::GetTeamScore(int32 TeamIndex) const
{
	if (TeamScores.IsValidIndex(TeamIndex))
	{
		return TeamScores[TeamIndex];
	}
	return 0;
}

void UBwayScoringComponent::ResetScores()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	for (int32& Score : TeamScores)
	{
		Score = 0;
	}

	// Broadcast for each team
	for (int32 i = 0; i < TeamScores.Num(); ++i)
	{
		OnTeamScoreChanged.Broadcast(i, 0);
	}
}

TArray<int32> UBwayScoringComponent::GetAllScores() const
{
	return TeamScores;
}

void UBwayScoringComponent::OnRep_Scores()
{
	// Broadcast to UI on clients
	for (int32 i = 0; i < TeamScores.Num(); ++i)
	{
		OnTeamScoreChanged.Broadcast(i, TeamScores[i]);
	}
}
