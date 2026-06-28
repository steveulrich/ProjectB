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
	int32 PointsToWin = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match Flow|Rules", meta = (ClampMin = "1"))
	float RoundDuration = 90.0f;

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
