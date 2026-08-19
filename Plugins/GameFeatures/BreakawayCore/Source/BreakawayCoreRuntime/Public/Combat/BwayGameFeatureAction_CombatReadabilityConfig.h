// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeatureAction.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "BwayGameFeatureAction_CombatReadabilityConfig.generated.h"

class UBwayCombatReadabilityConfig;

/**
 * Data-only experience action referencing combat readability config.
 * Add to B_BW_Experience_* Actions or LAS_BW_CombatReadability.
 */
UCLASS(meta = (DisplayName = "Breakaway Combat Readability Config"))
class BREAKAWAYCORERUNTIME_API UBwayGameFeatureAction_CombatReadabilityConfig : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Breakaway|Combat Readability")
	TSoftObjectPtr<UBwayCombatReadabilityConfig> CombatReadabilityConfig;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
