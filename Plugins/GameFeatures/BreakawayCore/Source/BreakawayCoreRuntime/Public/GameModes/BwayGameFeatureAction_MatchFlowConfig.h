// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeatureAction.h"
#include "BwayGameFeatureAction_MatchFlowConfig.generated.h"

class UBwayMatchFlowConfig;

/**
 * Data-only experience action: references the match-flow config without reparenting the experience BP.
 * Add to B_BW_Experience_Dev -> Actions (or an action set). Do NOT reparent the experience to BwayExperienceDefinition.
 */
UCLASS(meta = (DisplayName = "Breakaway Match Flow Config"))
class BREAKAWAYCORERUNTIME_API UBwayGameFeatureAction_MatchFlowConfig : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Breakaway|Match Flow")
	TSoftObjectPtr<UBwayMatchFlowConfig> MatchFlowConfig;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
