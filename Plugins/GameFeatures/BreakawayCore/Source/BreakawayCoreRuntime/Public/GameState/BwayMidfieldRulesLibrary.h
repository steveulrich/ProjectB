// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayMidfieldRulesLibrary.generated.h"

class ARelicActor;

/**
 * Timer-expiry territory is measured along the horizontal axis from team 0's goal
 * to team 1's goal, with midfield halfway between the goals.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayMidfieldRulesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Resolve a unique goal per team; reject missing, duplicate, or coincident goals. */
	static bool TryGetMidfieldFrame(const UObject* WorldContextObject, FVector& OutCenter, FVector& OutTeamOneDirection);

	/** Classify a world position using the active arena's goal geometry. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Midfield", meta = (WorldContext = "WorldContextObject"))
	static int32 GetRelicHalfAtWorldLocation(const UObject* WorldContextObject, FVector RelicWorldLocation, float MidfieldTolerance = 1.0f);

	/** Legacy canonical-coordinate helper: X is signed distance along the goal axis, not arena world X. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Midfield")
	static int32 GetRelicHalfFromLocation(FVector RelicWorldLocation, float MidfieldTolerance = 1.0f);

	/** Team that loses when the timer hits 0:00 (relic in that team's half). Returns -1 if undecided. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Midfield")
	static int32 GetLosingTeamAtMidfieldFromLocation(FVector RelicWorldLocation, float MidfieldTolerance = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Midfield", meta = (WorldContext = "WorldContextObject"))
	static int32 GetRelicHalfFromWorld(const UObject* WorldContextObject, float MidfieldTolerance = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Midfield", meta = (WorldContext = "WorldContextObject"))
	static int32 GetLosingTeamAtMidfieldFromWorld(const UObject* WorldContextObject, float MidfieldTolerance = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Midfield")
	static int32 GetRelicHalfFromRelic(const ARelicActor* Relic, float MidfieldTolerance = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Midfield")
	static int32 GetLosingTeamAtMidfieldFromRelic(const ARelicActor* Relic, float MidfieldTolerance = 1.0f);
};
