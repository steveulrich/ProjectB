// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BwayCombatReadabilityLibrary.h"

#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayGameFeatureAction_CombatReadabilityConfig.h"
#include "Engine/AssetManager.h"
#include "GameModes/LyraExperienceActionSet.h"
#include "GameModes/LyraExperienceDefinition.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameFramework/GameStateBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayCombatReadabilityLibrary)

namespace BwayCombatReadabilityLibPrivate
{
	const UBwayCombatReadabilityConfig* FindConfigOnExperience(const ULyraExperienceDefinition* Experience)
	{
		if (!Experience)
		{
			return nullptr;
		}

		auto FindInActions = [](const TArray<TObjectPtr<UGameFeatureAction>>& Actions) -> const UBwayCombatReadabilityConfig*
		{
			for (const TObjectPtr<UGameFeatureAction>& Action : Actions)
			{
				if (const UBwayGameFeatureAction_CombatReadabilityConfig* ConfigAction =
					Cast<UBwayGameFeatureAction_CombatReadabilityConfig>(Action))
				{
					return ConfigAction->CombatReadabilityConfig.LoadSynchronous();
				}
			}
			return nullptr;
		};

		if (const UBwayCombatReadabilityConfig* Config = FindInActions(Experience->Actions))
		{
			return Config;
		}

		for (const TObjectPtr<ULyraExperienceActionSet>& ActionSet : Experience->ActionSets)
		{
			if (ActionSet)
			{
				if (const UBwayCombatReadabilityConfig* Config = FindInActions(ActionSet->Actions))
				{
					return Config;
				}
			}
		}

		return nullptr;
	}

	const ULyraExperienceDefinition* GetLoadedExperience(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			return nullptr;
		}

		const UWorld* World = WorldContextObject->GetWorld();
		if (!World)
		{
			return nullptr;
		}

		const AGameStateBase* GameState = World->GetGameState();
		if (!GameState)
		{
			return nullptr;
		}

	const ULyraExperienceManagerComponent* ExperienceManager =
		GameState->FindComponentByClass<ULyraExperienceManagerComponent>();
	if (ExperienceManager && ExperienceManager->IsExperienceLoaded())
	{
		return ExperienceManager->GetCurrentExperienceChecked();
	}

	return nullptr;
}
}

const UBwayCombatReadabilityConfig* UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(
	const UObject* WorldContextObject,
	const ULyraExperienceDefinition* LoadedExperience)
{
	const ULyraExperienceDefinition* Experience = LoadedExperience
		? LoadedExperience
		: BwayCombatReadabilityLibPrivate::GetLoadedExperience(WorldContextObject);

	if (const UBwayCombatReadabilityConfig* Config =
		BwayCombatReadabilityLibPrivate::FindConfigOnExperience(Experience))
	{
		return Config;
	}

	const FSoftObjectPath DefaultPath(
		TEXT("/BreakawayCore/CombatFeedback/DA_BW_CombatReadability.DA_BW_CombatReadability"));
	if (const UBwayCombatReadabilityConfig* Fallback =
		Cast<UBwayCombatReadabilityConfig>(DefaultPath.TryLoad()))
	{
		return Fallback;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("UBwayCombatReadabilityLibrary: No combat readability config found on experience or fallback path."));
	return nullptr;
}
