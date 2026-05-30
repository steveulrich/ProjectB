#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayHeroSelectionFlowLibrary.generated.h"

class AGameModeBase;
class UWorld;

/** URL option helpers shared by hero-select staging, match travel, and editor PIE dev flow. */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayHeroSelectionFlowLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Breakaway|Hero Selection")
	static bool HasUrlOption(const UObject* WorldContextObject, FName OptionName);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Hero Selection")
	static bool IsHeroSelectStagingWorld(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Hero Selection")
	static bool ShouldSkipHeroSelectionWorld(const UObject* WorldContextObject);

	/** Editor PIE launched directly into a gameplay map (no staging travel, no post-staging skip). */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Hero Selection")
	static bool IsDirectEditorPlayWithoutHeroSelectFlow(const UObject* WorldContextObject);

	static bool HasUrlOptionForGameMode(const AGameModeBase* GameMode, FName OptionName);
	static bool IsHeroSelectStagingGameMode(const AGameModeBase* GameMode);
	static bool ShouldSkipHeroSelectionGameMode(const AGameModeBase* GameMode);
	static bool IsDirectEditorPlayWithoutHeroSelectFlowForGameMode(const AGameModeBase* GameMode);

	static const AGameModeBase* GetAuthGameMode(const UObject* WorldContextObject);
};
