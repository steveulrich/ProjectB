// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MatchmakingGoalDefinition.generated.h"

/**
 * Describes a matchmaking queue goal: party limits, team size, and eligible experiences.
 * Used by the matchmaking subsystem (Phase 3+) to build queue payloads and resolve travel targets.
 */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UMatchmakingGoalDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UMatchmakingGoalDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Identifies this queue for backend / analytics (e.g. Matchmaking.Queue.QuickPlay). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Matchmaking")
	FGameplayTag QueueTypeTag;

	/** Maximum players allowed in a party before queueing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Matchmaking", meta = (ClampMin = "1"))
	int32 MaxPartySize = 4;

	/** Target players per team once a match is formed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Matchmaking", meta = (ClampMin = "1"))
	int32 PlayersPerTeam = 5;

	/** Map loaded when this queue resolves a match (passed to UCommonSession_HostSessionRequest::MapID). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Matchmaking", meta = (AllowedTypes = "Map"))
	FPrimaryAssetId MapID;

	/**
	 * Experiences eligible for this queue. Phase 4 uses the first entry after mock match-found;
	 * production matchmaking may pick among these based on server assignment.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Matchmaking", meta = (AllowedTypes = "LyraExperienceDefinition"))
	TArray<FPrimaryAssetId> AllowedExperiences;
};
