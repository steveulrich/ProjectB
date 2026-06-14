// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayExperienceLibrary.generated.h"

struct FPrimaryAssetId;

/**
 * Helpers for resolving Breakaway Lyra experiences in packaged builds.
 * Game-feature experience BPs can be cooked into the pak but missing from the
 * asset manager's primary-asset forward map at startup.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayExperienceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Register known /BreakawayCore/Experiences assets with the asset manager if needed. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Experience")
	static void RegisterPackagedBreakawayExperiences();

	/** Returns true when ExperienceId is registered (registering from the asset registry if needed). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Experience")
	static bool EnsureExperienceRegistered(FPrimaryAssetId ExperienceId);
};
