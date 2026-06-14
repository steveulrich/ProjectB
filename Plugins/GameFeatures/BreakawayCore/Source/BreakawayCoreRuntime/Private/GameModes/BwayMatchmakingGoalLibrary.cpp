// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/BwayMatchmakingGoalLibrary.h"

#include "GameModes/MatchmakingGoalDefinition.h"
#include "Matchmaking/LyraMatchmakingSubsystem.h"
#include "Matchmaking/LyraMatchmakingTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchmakingGoalLibrary)

DEFINE_LOG_CATEGORY_STATIC(LogBwayMatchmakingGoal, Log, All);

void UBwayMatchmakingGoalLibrary::LogMatchmakingGoalDefinition(const UMatchmakingGoalDefinition* Goal)
{
	if (!Goal)
	{
		UE_LOG(LogBwayMatchmakingGoal, Warning, TEXT("LogMatchmakingGoalDefinition: Goal is null."));
		return;
	}

	FString AllowedExperiencesStr;
	for (int32 Index = 0; Index < Goal->AllowedExperiences.Num(); ++Index)
	{
		if (Index > 0)
		{
			AllowedExperiencesStr += TEXT(", ");
		}
		AllowedExperiencesStr += Goal->AllowedExperiences[Index].ToString();
	}

	UE_LOG(LogBwayMatchmakingGoal, Log,
		TEXT("MatchmakingGoal '%s' — QueueTypeTag=%s MapID=%s MaxPartySize=%d PlayersPerTeam=%d AllowedExperiences=[%s]"),
		*Goal->GetName(),
		*Goal->QueueTypeTag.ToString(),
		*Goal->MapID.ToString(),
		Goal->MaxPartySize,
		Goal->PlayersPerTeam,
		*AllowedExperiencesStr);
}

FGameplayTag UBwayMatchmakingGoalLibrary::GetQueueTypeTag(const UMatchmakingGoalDefinition* Goal)
{
	return Goal ? Goal->QueueTypeTag : FGameplayTag();
}

int32 UBwayMatchmakingGoalLibrary::GetPlayersPerTeam(const UMatchmakingGoalDefinition* Goal)
{
	return Goal ? Goal->PlayersPerTeam : 0;
}

int32 UBwayMatchmakingGoalLibrary::GetMaxPartySize(const UMatchmakingGoalDefinition* Goal)
{
	return Goal ? Goal->MaxPartySize : 0;
}

FPrimaryAssetId UBwayMatchmakingGoalLibrary::GetMapID(const UMatchmakingGoalDefinition* Goal)
{
	return Goal ? Goal->MapID : FPrimaryAssetId();
}

FLyraMatchmakingQueuePayload UBwayMatchmakingGoalLibrary::MakeQueuePayloadFromGoal(const UMatchmakingGoalDefinition* Goal)
{
	FLyraMatchmakingQueuePayload Payload;

	if (!Goal)
	{
		return Payload;
	}

	Payload.GoalAssetId = Goal->GetPrimaryAssetId();
	Payload.QueueTypeTag = Goal->QueueTypeTag;
	Payload.MaxPartySize = Goal->MaxPartySize;
	Payload.PlayersPerTeam = Goal->PlayersPerTeam;
	Payload.MapID = Goal->MapID;
	Payload.AllowedExperiences = Goal->AllowedExperiences;

	return Payload;
}

void UBwayMatchmakingGoalLibrary::StartMatchmakingQueue(const UObject* WorldContextObject, const UMatchmakingGoalDefinition* Goal)
{
	if (!Goal)
	{
		UE_LOG(LogBwayMatchmakingGoal, Warning, TEXT("StartMatchmakingQueue: Goal is null."));
		return;
	}

	ULyraMatchmakingSubsystem* MatchmakingSubsystem = ULyraMatchmakingSubsystem::Get(WorldContextObject);
	if (!MatchmakingSubsystem)
	{
		UE_LOG(LogBwayMatchmakingGoal, Warning, TEXT("StartMatchmakingQueue: ULyraMatchmakingSubsystem not available."));
		return;
	}

	MatchmakingSubsystem->StartMatchmakingQueue(MakeQueuePayloadFromGoal(Goal));
}

void UBwayMatchmakingGoalLibrary::ExecuteCustomSessionCreation(const UObject* WorldContextObject)
{
	ULyraMatchmakingSubsystem* MatchmakingSubsystem = ULyraMatchmakingSubsystem::Get(WorldContextObject);
	if (!MatchmakingSubsystem)
	{
		UE_LOG(LogBwayMatchmakingGoal, Warning, TEXT("ExecuteCustomSessionCreation: ULyraMatchmakingSubsystem not available."));
		return;
	}

	MatchmakingSubsystem->ExecuteCustomSessionCreation();
}
