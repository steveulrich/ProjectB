// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "GameModes/BwayExperienceDefinition.h"
#include "GameModes/BwayGameFeatureAction_MatchFlowConfig.h"
#include "GameModes/BwayMatchFlowConfig.h"
#include "GameModes/LyraExperienceActionSet.h"
#include "GameState/BwayBotCreationComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/AssetManager.h"
#include "GameFeatureAction.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayMatchFlowLibrary)

namespace
{
const AGameModeBase* GetAuthGameMode(const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		return World->GetAuthGameMode();
	}
	return nullptr;
}

const UBwayMatchFlowConfig* LoadConfigFromPrimaryAssetId(const FPrimaryAssetId& AssetId)
{
	if (!AssetId.IsValid())
	{
		return nullptr;
	}

	if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		if (const UBwayMatchFlowConfig* CachedConfig = Cast<UBwayMatchFlowConfig>(AssetManager->GetPrimaryAssetObject(AssetId)))
		{
			return CachedConfig;
		}

		const FSoftObjectPath AssetPath = AssetManager->GetPrimaryAssetPath(AssetId);
		if (AssetPath.IsValid())
		{
			return Cast<UBwayMatchFlowConfig>(AssetPath.TryLoad());
		}
	}

	return nullptr;
}

const UBwayMatchFlowConfig* FindMatchFlowConfigOnExperience(const ULyraExperienceDefinition* Experience)
{
	if (!Experience)
	{
		return nullptr;
	}

	auto FindInActions = [](const TArray<TObjectPtr<UGameFeatureAction>>& Actions) -> const UBwayMatchFlowConfig*
	{
		for (const TObjectPtr<UGameFeatureAction>& Action : Actions)
		{
			if (const UBwayGameFeatureAction_MatchFlowConfig* ConfigAction = Cast<UBwayGameFeatureAction_MatchFlowConfig>(Action))
			{
				return ConfigAction->MatchFlowConfig.LoadSynchronous();
			}
		}
		return nullptr;
	};

	if (const UBwayMatchFlowConfig* Config = FindInActions(Experience->Actions))
	{
		return Config;
	}

	for (const TObjectPtr<ULyraExperienceActionSet>& ActionSet : Experience->ActionSets)
	{
		if (ActionSet)
		{
			if (const UBwayMatchFlowConfig* Config = FindInActions(ActionSet->Actions))
			{
				return Config;
			}
		}
	}

	if (const UBwayExperienceDefinition* BwayExp = Cast<const UBwayExperienceDefinition>(Experience))
	{
		return BwayExp->MatchFlowConfig.LoadSynchronous();
	}

	return nullptr;
}

void CopyConfigToResolved(const UBwayMatchFlowConfig* BaseConfig, FBwayResolvedMatchFlowSettings& Resolved)
{
	if (!BaseConfig)
	{
		return;
	}

	Resolved.ConfigAssetName = BaseConfig->GetFName();
	Resolved.bOrchestrateMatchFlow = BaseConfig->bOrchestrateMatchFlow;
	Resolved.PointsToWin = BaseConfig->PointsToWin;
	Resolved.RoundDuration = BaseConfig->RoundDuration;
	Resolved.PrematchDuration = BaseConfig->PrematchDuration;
	Resolved.WarmupDuration = BaseConfig->WarmupDuration;
	Resolved.PostRoundDuration = BaseConfig->PostRoundDuration;
	Resolved.WarmupPhaseAbility = BaseConfig->WarmupPhaseAbility;
	Resolved.PlayingPhaseAbility = BaseConfig->PlayingPhaseAbility;
	Resolved.PostRoundPhaseAbility = BaseConfig->PostRoundPhaseAbility;
	Resolved.PostMatchPhaseAbility = BaseConfig->PostMatchPhaseAbility;
	Resolved.NumBotsOverride = BaseConfig->DefaultNumBots;
}
}

bool UBwayMatchFlowLibrary::GetUrlOptionInt(const UObject* WorldContextObject, FName OptionName, int32& OutValue)
{
	return UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionInt(WorldContextObject, OptionName, OutValue);
}

bool UBwayMatchFlowLibrary::GetUrlOptionPrimaryAssetId(
	const UObject* WorldContextObject,
	FName OptionName,
	FPrimaryAssetType ExpectedType,
	FPrimaryAssetId& OutAssetId)
{
	FString ValueString;
	if (!UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionString(WorldContextObject, OptionName, ValueString))
	{
		return false;
	}

	OutAssetId = FPrimaryAssetId(ExpectedType, FName(*ValueString));
	return OutAssetId.IsValid();
}

FBwayResolvedMatchFlowSettings UBwayMatchFlowLibrary::ResolveMatchFlowSettings(
	const UObject* WorldContextObject,
	const UBwayMatchFlowConfig* FallbackConfig,
	const ULyraExperienceDefinition* LoadedExperience)
{
	FBwayResolvedMatchFlowSettings Resolved;

	const UBwayMatchFlowConfig* BaseConfig = FallbackConfig;

	FPrimaryAssetId UrlConfigId;
	if (GetUrlOptionPrimaryAssetId(WorldContextObject, TEXT("MatchFlowConfig"), FPrimaryAssetType(TEXT("BwayMatchFlowConfig")), UrlConfigId))
	{
		if (const UBwayMatchFlowConfig* UrlConfig = LoadConfigFromPrimaryAssetId(UrlConfigId))
		{
			BaseConfig = UrlConfig;
		}
	}

	if (!BaseConfig && LoadedExperience)
	{
		BaseConfig = FindMatchFlowConfigOnExperience(LoadedExperience);
	}

	if (!BaseConfig)
	{
		const FSoftObjectPath DefaultDevPath(TEXT("/BreakawayCore/MatchFlow/DA_BW_MatchFlow_Dev.DA_BW_MatchFlow_Dev"));
		BaseConfig = Cast<UBwayMatchFlowConfig>(DefaultDevPath.TryLoad());
	}

	CopyConfigToResolved(BaseConfig, Resolved);

	const int32 DaPointsToWin = Resolved.PointsToWin;
	const int32 DaNumBots = Resolved.NumBotsOverride;

	if (!BaseConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayMatchFlow: No config asset resolved; using component defaults with URL overrides only"));
	}

	UBwayGameplayUrlLibrary::LogGameplayUrlDiagnostics(WorldContextObject, TEXT("ResolveMatchFlowSettings"));

	FString UrlSourceLabel;
	int32 UrlPointsToWin = 0;
	if (UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionIntWithSource(WorldContextObject, TEXT("PointsToWin"), UrlPointsToWin, UrlSourceLabel))
	{
		Resolved.PointsToWin = UrlPointsToWin;
		UE_LOG(LogTemp, Log, TEXT("BwayMatchFlow: PointsToWin=%d (URL override from %s, DA default was %d)"),
			UrlPointsToWin, *UrlSourceLabel, DaPointsToWin);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("BwayMatchFlow: PointsToWin=%d (DA/config default — no URL override)"), Resolved.PointsToWin);
	}

	int32 UrlNumBots = 0;
	if (UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionIntWithSource(WorldContextObject, TEXT("NumBots"), UrlNumBots, UrlSourceLabel))
	{
		Resolved.NumBotsOverride = UrlNumBots;
		UE_LOG(LogTemp, Log, TEXT("BwayMatchFlow: NumBots=%d (URL override from %s, DA default was %d)"),
			UrlNumBots, *UrlSourceLabel, DaNumBots);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("BwayMatchFlow: NumBots=%s (no URL — DA DefaultNumBots=%d or BotCreation scaling)"),
			DaNumBots >= 0 ? *FString::FromInt(DaNumBots) : TEXT("scaling"),
			DaNumBots);
	}

	return Resolved;
}

void UBwayMatchFlowLibrary::ApplyMatchRulesOnly(
	const UObject* WorldContextObject,
	const FBwayResolvedMatchFlowSettings& Settings,
	UBwayRoundManagementComponent* RoundManagement,
	UBwayBotCreationComponent* BotCreation)
{
	if (RoundManagement)
	{
		RoundManagement->SetPointsToWin(Settings.PointsToWin);
	}

	if (BotCreation && Settings.NumBotsOverride >= 0)
	{
		BotCreation->SetNumBotsOverride(Settings.NumBotsOverride);
	}
}

void UBwayMatchFlowLibrary::LogResolvedMatchFlowSettings(const FBwayResolvedMatchFlowSettings& Settings)
{
	const FString NumBotsText = Settings.NumBotsOverride >= 0
		? FString::FromInt(Settings.NumBotsOverride)
		: TEXT("default (use DA DefaultNumBots or BotCreation scaling)");

	UE_LOG(LogTemp, Log,
		TEXT("BwayMatchFlow: Resolved config '%s' — PointsToWin=%d NumBots=%s bOrchestrate=%s (11-1: rules only)"),
		*Settings.ConfigAssetName.ToString(),
		Settings.PointsToWin,
		*NumBotsText,
		Settings.bOrchestrateMatchFlow ? TEXT("true") : TEXT("false"));
}
