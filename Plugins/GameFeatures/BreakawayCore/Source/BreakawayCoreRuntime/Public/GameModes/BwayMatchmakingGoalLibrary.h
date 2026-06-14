// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Matchmaking/LyraMatchmakingTypes.h"
#include "BwayMatchmakingGoalLibrary.generated.h"

class UMatchmakingGoalDefinition;

/**
 * Blueprint helpers for reading matchmaking goal data assets (Phase 1 UI verification).
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayMatchmakingGoalLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Logs QueueTypeTag, MaxPartySize, PlayersPerTeam, and AllowedExperiences to the output log. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Matchmaking", meta = (DisplayName = "Log Matchmaking Goal"))
	static void LogMatchmakingGoalDefinition(const UMatchmakingGoalDefinition* Goal);

	/** Builds the Lyra queue payload struct from a goal data asset. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Matchmaking")
	static FLyraMatchmakingQueuePayload MakeQueuePayloadFromGoal(const UMatchmakingGoalDefinition* Goal);

	/** Starts mock matchmaking for the given goal (11-MM-3). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Matchmaking", meta = (WorldContext = "WorldContextObject", DisplayName = "Start Matchmaking Queue"))
	static void StartMatchmakingQueue(const UObject* WorldContextObject, const UMatchmakingGoalDefinition* Goal);

	/** Hosts a custom game immediately from UCustomGameConfig overrides (11-MM-4). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Matchmaking", meta = (WorldContext = "WorldContextObject", DisplayName = "Execute Custom Session Creation"))
	static void ExecuteCustomSessionCreation(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Matchmaking")
	static FGameplayTag GetQueueTypeTag(const UMatchmakingGoalDefinition* Goal);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Matchmaking")
	static int32 GetPlayersPerTeam(const UMatchmakingGoalDefinition* Goal);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Matchmaking")
	static int32 GetMaxPartySize(const UMatchmakingGoalDefinition* Goal);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Matchmaking")
	static FPrimaryAssetId GetMapID(const UMatchmakingGoalDefinition* Goal);
};
