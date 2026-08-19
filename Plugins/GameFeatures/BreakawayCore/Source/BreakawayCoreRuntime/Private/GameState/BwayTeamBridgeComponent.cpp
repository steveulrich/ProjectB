// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayTeamBridgeComponent.h"
#include "BwayGameState.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraExperienceDefinition.h"
#include "GameModes/LyraExperienceManagerComponent.h"
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
		SyncAllTeamsToLyra();

		// LyraTeamCreationComponent assigns teams on HighPriority experience-loaded.
		// Re-assert Breakaway roster afterward so Lyra GenericTeamId stays aligned
		// (otherwise nameplates/cosmetics use Lyra IDs that no longer match FTeamInfo).
		if (AGameStateBase* GS = GetGameState<AGameStateBase>())
		{
			if (ULyraExperienceManagerComponent* ExperienceComponent =
					GS->FindComponentByClass<ULyraExperienceManagerComponent>())
			{
				ExperienceComponent->CallOrRegister_OnExperienceLoaded(
					FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoaded));
			}
		}
	}
}

void UBwayTeamBridgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UBwayTeamBridgeComponent::HandleExperienceLoaded(const ULyraExperienceDefinition* /*Experience*/)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		UE_LOG(LogTemp, Log, TEXT("BwayTeamBridge: Experience loaded — re-syncing Breakaway teams onto Lyra"));
		SyncAllTeamsToLyra();
	}
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
