// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/BwayGameFeatureAction_MatchFlowConfig.h"
#include "GameModes/BwayMatchFlowConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameFeatureAction_MatchFlowConfig)

#if WITH_EDITOR
EDataValidationResult UBwayGameFeatureAction_MatchFlowConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (MatchFlowConfig.IsNull())
	{
		Context.AddError(FText::FromString(TEXT("MatchFlowConfig must be set.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
