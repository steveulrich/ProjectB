// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "LyraMatchmakingTypes.generated.h"

/**
 * Serializable queue request built from UMatchmakingGoalDefinition (or Blueprint).
 * Lives in LyraGame so ULyraMatchmakingSubsystem does not depend on BreakawayCoreRuntime.
 */
USTRUCT(BlueprintType)
struct FLyraMatchmakingQueuePayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Matchmaking")
	FPrimaryAssetId GoalAssetId;

	UPROPERTY(BlueprintReadWrite, Category = "Matchmaking")
	FGameplayTag QueueTypeTag;

	UPROPERTY(BlueprintReadWrite, Category = "Matchmaking", meta = (ClampMin = "1"))
	int32 MaxPartySize = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Matchmaking", meta = (ClampMin = "1"))
	int32 PlayersPerTeam = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Matchmaking")
	FPrimaryAssetId MapID;

	UPROPERTY(BlueprintReadWrite, Category = "Matchmaking")
	TArray<FPrimaryAssetId> AllowedExperiences;
};
