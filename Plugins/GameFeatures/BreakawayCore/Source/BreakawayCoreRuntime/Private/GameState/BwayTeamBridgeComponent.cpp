// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayTeamBridgeComponent.h"
#include "BwayGameState.h"
#include "GameFramework/PlayerState.h"

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
	// TODO: When BwayGameState adds delegates for team changes,
	// bind to them here. For now, SyncAllTeamsToLyra() can be called
	// manually after AddPlayerToTeam/RemovePlayerFromTeam.
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

	// TODO: Use ULyraTeamSubsystem to register the player with the corresponding Lyra team.
	// This requires:
	// 1. Getting the ULyraTeamSubsystem from the World
	// 2. Finding or creating the LyraTeam for the TeamIndex
	// 3. Calling the subsystem's team assignment API
	//
	// Example (depends on your Lyra version):
	//   if (ULyraTeamSubsystem* TeamSS = GetWorld()->GetSubsystem<ULyraTeamSubsystem>())
	//   {
	//       TeamSS->ChangeTeamForActor(PlayerState, TeamIndex);
	//   }
	
	UE_LOG(LogTemp, Log, TEXT("BwayTeamBridge: Synced player %s to Lyra team %d"), 
		*PlayerState->GetPlayerName(), TeamIndex);
}
