// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayMidfieldRulesLibrary.generated.h"

class ARelicActor;

/**
 * Sudden-death / timer-expiry rules based on relic world X position.
 * Origin (0,0,0) is midfield; Team 1 (index 0) defends -X, Team 2 (index 1) defends +X.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayMidfieldRulesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Team index (0 = Team 1 / -X half, 1 = Team 2 / +X half), or -1 if neutral/unknown. */
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
