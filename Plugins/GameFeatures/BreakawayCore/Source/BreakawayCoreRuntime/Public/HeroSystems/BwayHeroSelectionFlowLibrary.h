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

	/** Section 1 humanoid regression — no hero apply, no hero assets. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Hero Selection")
	static bool ShouldForceHumanoidWorld(const UObject* WorldContextObject);

	/** Editor PIE launched directly into a match map (not hero-select staging). */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Hero Selection")
	static bool IsDirectEditorPlayMatchMap(const UObject* WorldContextObject);

	/** Editor PIE on match map without Hero= URL, staging travel, or SkipHeroSelection — auto hero UI path. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Hero Selection")
	static bool IsDirectEditorPlayWithoutHeroSelectFlow(const UObject* WorldContextObject);

	/** Fixed bot hero for direct editor PIE (Hero= URL or Argus). Invalid when random/production bots apply. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Hero Selection")
	static FPrimaryAssetId ResolveDirectPieBotHeroId(const UObject* WorldContextObject);

	static bool HasUrlOptionForGameMode(const AGameModeBase* GameMode, FName OptionName);
	static bool IsHeroSelectStagingGameMode(const AGameModeBase* GameMode);
	static bool ShouldSkipHeroSelectionGameMode(const AGameModeBase* GameMode);
	static bool ShouldForceHumanoidGameMode(const AGameModeBase* GameMode);
	static bool IsDirectEditorPlayMatchMapForGameMode(const AGameModeBase* GameMode);
	static bool IsDirectEditorPlayWithoutHeroSelectFlowForGameMode(const AGameModeBase* GameMode);
	static FPrimaryAssetId ResolveDirectPieBotHeroIdForGameMode(const AGameModeBase* GameMode);

	static const AGameModeBase* GetAuthGameMode(const UObject* WorldContextObject);
};
