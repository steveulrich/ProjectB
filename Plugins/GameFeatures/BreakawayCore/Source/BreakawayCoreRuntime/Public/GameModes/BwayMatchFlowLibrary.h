// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayMatchFlowLibrary.generated.h"

class AGameModeBase;
class UBwayBotCreationComponent;
class UBwayMatchFlowConfig;
class UBwayRoundManagementComponent;
class ULyraExperienceDefinition;

/** Resolved match-flow values after config asset + URL overrides. */
USTRUCT(BlueprintType)
struct FBwayResolvedMatchFlowSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	FName ConfigAssetName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	bool bOrchestrateMatchFlow = true;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	int32 PointsToWin = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	float RoundDuration = 90.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	float PrematchDuration = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	float WarmupDuration = 3.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	float PostRoundDuration = 5.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	TSubclassOf<class ULyraGamePhaseAbility> PrematchPhaseAbility;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	TSubclassOf<class ULyraGamePhaseAbility> WarmupPhaseAbility;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	TSubclassOf<class ULyraGamePhaseAbility> PlayingPhaseAbility;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	TSubclassOf<class ULyraGamePhaseAbility> PostRoundPhaseAbility;

	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	TSubclassOf<class ULyraGamePhaseAbility> PostMatchPhaseAbility;

	/** -1 means use BotCreationComponent defaults / scaling. */
	UPROPERTY(BlueprintReadOnly, Category = "Match Flow")
	int32 NumBotsOverride = -1;
};

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayMatchFlowLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow", meta = (WorldContext = "WorldContextObject"))
	static bool GetUrlOptionInt(const UObject* WorldContextObject, FName OptionName, int32& OutValue);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Match Flow", meta = (WorldContext = "WorldContextObject"))
	static bool GetUrlOptionPrimaryAssetId(const UObject* WorldContextObject, FName OptionName, FPrimaryAssetType ExpectedType, FPrimaryAssetId& OutAssetId);

	/** Build resolved settings: URL overrides on top of config asset (URL, experience default, then dev fallback). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Match Flow", meta = (WorldContext = "WorldContextObject"))
	static FBwayResolvedMatchFlowSettings ResolveMatchFlowSettings(
		const UObject* WorldContextObject,
		const UBwayMatchFlowConfig* FallbackConfig,
		const ULyraExperienceDefinition* LoadedExperience = nullptr);

	/** Apply match rules on RM / BotCreation: PointsToWin, RoundDuration (11-5), NumBots. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Match Flow", meta = (WorldContext = "WorldContextObject"))
	static void ApplyMatchRulesOnly(
		const UObject* WorldContextObject,
		const FBwayResolvedMatchFlowSettings& Settings,
		UBwayRoundManagementComponent* RoundManagement,
		UBwayBotCreationComponent* BotCreation);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Match Flow")
	static void LogResolvedMatchFlowSettings(const FBwayResolvedMatchFlowSettings& Settings);
};
