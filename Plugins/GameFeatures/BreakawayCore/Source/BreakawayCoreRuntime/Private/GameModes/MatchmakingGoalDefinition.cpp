// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/MatchmakingGoalDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MatchmakingGoalDefinition)

namespace MatchmakingGoalDefinitionDefaults
{
	const FPrimaryAssetType AssetType(TEXT("MatchmakingGoalDefinition"));
}

UMatchmakingGoalDefinition::UMatchmakingGoalDefinition()
{
}

FPrimaryAssetId UMatchmakingGoalDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(MatchmakingGoalDefinitionDefaults::AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UMatchmakingGoalDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!QueueTypeTag.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("QueueTypeTag must be set.")));
		Result = EDataValidationResult::Invalid;
	}

	if (!MapID.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("MapID must be set.")));
		Result = EDataValidationResult::Invalid;
	}

	if (AllowedExperiences.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("AllowedExperiences must contain at least one LyraExperienceDefinition.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
