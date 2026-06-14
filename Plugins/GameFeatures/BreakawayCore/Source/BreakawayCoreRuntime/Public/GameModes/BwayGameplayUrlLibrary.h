// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayGameplayUrlLibrary.generated.h"

class AGameModeBase;
class UWorld;

/**
 * Resolves gameplay URL options across GameMode::OptionsString, World::URL, PIE LastURL, and command line.
 * PIE "Selected Viewport" / listen-server flows often omit options on GameMode::OptionsString alone.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayUrlLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Breakaway|URL", meta = (WorldContext = "WorldContextObject"))
	static bool HasGameplayUrlOption(const UObject* WorldContextObject, FName OptionName);

	UFUNCTION(BlueprintPure, Category = "Breakaway|URL", meta = (WorldContext = "WorldContextObject"))
	static bool TryGetGameplayUrlOptionInt(const UObject* WorldContextObject, FName OptionName, int32& OutValue);

	UFUNCTION(BlueprintPure, Category = "Breakaway|URL", meta = (WorldContext = "WorldContextObject"))
	static bool TryGetGameplayUrlOptionString(const UObject* WorldContextObject, FName OptionName, FString& OutValue);

	/** Merge known gameplay keys from UrlSource into OptionsString when absent. */
	static void AppendMissingOptionsFromUrlSource(FString& OptionsString, const FString& UrlSource);

	/** After InitGame, pull options from World URL / PIE context into GameMode::OptionsString. */
	static void AugmentGameModeOptionsString(AGameModeBase* GameMode);

	/** Dump every option source and whether each known gameplay key was found (filter log: BwayGameplayUrl). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|URL", meta = (WorldContext = "WorldContextObject"))
	static void LogGameplayUrlDiagnostics(const UObject* WorldContextObject, const FString& Context = TEXT(""));

	/** Like TryGetGameplayUrlOptionInt but returns which source label matched (first wins). */
	static bool TryGetGameplayUrlOptionIntWithSource(
		const UObject* WorldContextObject,
		FName OptionName,
		int32& OutValue,
		FString& OutSourceLabel);
};
