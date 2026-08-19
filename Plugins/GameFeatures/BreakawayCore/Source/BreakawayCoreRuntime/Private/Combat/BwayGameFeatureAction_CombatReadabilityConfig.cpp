// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BwayGameFeatureAction_CombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameFeatureAction_CombatReadabilityConfig)

#if WITH_EDITOR
EDataValidationResult UBwayGameFeatureAction_CombatReadabilityConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (CombatReadabilityConfig.IsNull())
	{
		Context.AddError(FText::FromString(TEXT("CombatReadabilityConfig must be set.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
