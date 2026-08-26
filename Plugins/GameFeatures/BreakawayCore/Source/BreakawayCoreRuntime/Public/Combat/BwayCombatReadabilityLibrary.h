// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayCombatReadabilityLibrary.generated.h"

class UBwayCombatReadabilityConfig;
class ULyraExperienceDefinition;

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayCombatReadabilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Resolve combat readability config from the loaded experience actions (after experience load). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Combat Readability", meta = (WorldContext = "WorldContextObject"))
	static const UBwayCombatReadabilityConfig* ResolveCombatReadabilityConfig(
		const UObject* WorldContextObject,
		const ULyraExperienceDefinition* LoadedExperience = nullptr);

	/**
	 * World location for a combat number pop.
	 * Prefers the possessed pawn when Target is a PlayerState (Lyra HealthSet owns the ASC on PS).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Combat Readability")
	static FVector ResolveNumberPopWorldLocation(const UObject* TargetObject, const FVector& WorldOffset);
};
