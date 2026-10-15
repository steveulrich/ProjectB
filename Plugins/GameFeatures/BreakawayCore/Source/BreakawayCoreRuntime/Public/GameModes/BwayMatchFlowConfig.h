// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "BwayMatchFlowConfig.generated.h"

/**
 * Data-driven match flow: phase abilities, timings, and match rules.
 * Referenced by UBwayExperienceDefinition and/or URL ?MatchFlowConfig=DA_*.
 */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API UBwayMatchFlowConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UBwayMatchFlowConfig();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** When true, RM owns phase transitions (11-3+). When false, legacy PlayingPhaseTag listener may run. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow")
	bool bOrchestrateMatchFlow = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Rules", meta = (ClampMin = "1"))
	int32 PointsToWin = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Rules", meta = (ClampMin = "1"))
	float RoundDuration = 240.0f;

	/** Base respawn delay in seconds on a player's first death of the round (Sept 7, 2017 GameSpot evidence: 10.0s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Rules|Respawn", meta = (ClampMin = "0.05"))
	float BaseRespawnDelaySeconds = 10.0f;

	/** Additional respawn delay in seconds added per prior death within the current round (5.0s -> 10s, 15s, 20s, 25s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Rules|Respawn", meta = (ClampMin = "0.0"))
	float PerDeathRespawnIncrementSeconds = 5.0f;

	/** Maximum escalated base respawn delay in seconds before RespawnTimeReduction is applied (25.0s cap). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Rules|Respawn", meta = (ClampMin = "0.05"))
	float MaxRespawnDelaySeconds = 25.0f;

	/** When true, per-player within-round death escalation resets to 0 at each round boundary. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Rules|Respawn")
	bool bResetRespawnEscalationEachRound = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Phases", meta = (ClampMin = "0"))
	float PrematchDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Phases", meta = (ClampMin = "0"))
	float WarmupDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Phases", meta = (ClampMin = "0"))
	float PostRoundDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Phases", meta = (ClampMin = "0"))
	float PostMatchSummaryDuration = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Phases")
	TSubclassOf<ULyraGamePhaseAbility> PrematchPhaseAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Phases")
	TSubclassOf<ULyraGamePhaseAbility> WarmupPhaseAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Phases")
	TSubclassOf<ULyraGamePhaseAbility> PlayingPhaseAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Phases")
	TSubclassOf<ULyraGamePhaseAbility> PostRoundPhaseAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Phases")
	TSubclassOf<ULyraGamePhaseAbility> PostMatchPhaseAbility;

	/** Dev default bot count when URL NumBots is not set (-1 = use BotCreationComponent defaults). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Dev")
	int32 DefaultNumBots = -1;
};

using UBwayMatchConfigDataAsset = UBwayMatchFlowConfig;
