// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayTeamBridgeComponent.h"
#include "BwayGameState.h"
#include "GameFramework/PlayerState.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayTeamBridgeComponent)

UBwayTeamBridgeComponent::UBwayTeamBridgeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayTeamBridgeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() == ROLE_Authority)
	{
		BindToTeamEvents();
		
		// Do initial sync
		SyncAllTeamsToLyra();
	}
}

void UBwayTeamBridgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UBwayTeamBridgeComponent::BindToTeamEvents()
{
	if (ABwayGameState* BwayGS = Cast<ABwayGameState>(GetOwner()))
	{
		BwayGS->OnTeamsUpdated.AddDynamic(this, &UBwayTeamBridgeComponent::SyncAllTeamsToLyra);
	}
	
	UE_LOG(LogTemp, Log, TEXT("BwayTeamBridge: Listening for team changes"));
}

void UBwayTeamBridgeComponent::SyncAllTeamsToLyra()
{
	ABwayGameState* BwayGS = Cast<ABwayGameState>(GetOwner());
	if (!BwayGS)
	{
		return;
	}

	// Sync each team's members
	for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
	{
		const FTeamInfo& TeamInfo = BwayGS->GetTeamInfo(TeamIndex);
		for (const TObjectPtr<APlayerState>& PS : TeamInfo.TeamMembers)
		{
			if (PS)
			{
				SyncPlayerTeamToLyra(PS, TeamIndex);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BwayTeamBridge: Full team sync complete"));
}

void UBwayTeamBridgeComponent::SyncPlayerTeamToLyra(APlayerState* PlayerState, int32 TeamIndex)
{
	if (!PlayerState)
	{
		return;
	}

	if (ULyraTeamSubsystem* TeamSS = GetWorld()->GetSubsystem<ULyraTeamSubsystem>())
	{
		// Offset by 1 since Breakaway uses indices 0/1 but Lyra expects TeamIDs 1/2
		TeamSS->ChangeTeamForActor(PlayerState, TeamIndex + 1);
	}
	
	UE_LOG(LogTemp, Log, TEXT("BwayTeamBridge: Synced player %s to Lyra team %d"), 
		*PlayerState->GetPlayerName(), TeamIndex + 1);
}
