// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameModes/LyraExperienceDefinition.h"
#include "BwayExperienceDefinition.generated.h"

class UBwayMatchFlowConfig;

/**
 * Optional C++ experience parent with an embedded MatchFlowConfig pointer.
 * Prefer UBwayGameFeatureAction_MatchFlowConfig on the experience Actions list instead —
 * do NOT reparent B_BW_Experience_Dev away from LyraExperienceDefinition (breaks pawn/action wiring).
 */
UCLASS(BlueprintType, Const)
class BREAKAWAYCORERUNTIME_API UBwayExperienceDefinition : public ULyraExperienceDefinition
{
	GENERATED_BODY()

public:
	UBwayExperienceDefinition() = default;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Match Flow")
	TSoftObjectPtr<UBwayMatchFlowConfig> MatchFlowConfig;
};
