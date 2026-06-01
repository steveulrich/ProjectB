// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayRelicBotLibrary.generated.h"

class ABwayCharacterWithAbilities;
class ABwayGoalVolume;
class ARelicActor;
class AController;

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayRelicBotLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Breakaway|Relic Bot", meta = (WorldContext = "WorldContextObject"))
	static int32 GetBotTeamIndex(const AController* BotController);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Relic Bot", meta = (WorldContext = "WorldContextObject"))
	static ARelicActor* GetActiveRelic(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Relic Bot", meta = (WorldContext = "WorldContextObject"))
	static ABwayGoalVolume* GetEnemyGoalVolume(const UObject* WorldContextObject, const AController* BotController);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Relic Bot")
	static FVector GetEnemyGoalLocation(const ABwayGoalVolume* EnemyGoal);

	UFUNCTION(BlueprintPure, Category = "Breakaway|Relic Bot", meta = (WorldContext = "WorldContextObject"))
	static bool IsSuddenDeathWindowActive(const UObject* WorldContextObject);

	/** Applies GE_BW_Relic_Request (from RelicSettings) or loose RequestingTag — mirrors human request input. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic Bot")
	static void ApplyRelicRequestState(ABwayCharacterWithAbilities* BotCharacter);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic Bot")
	static bool TryBotPickupRelic(ABwayCharacterWithAbilities* BotCharacter);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic Bot")
	static bool TryBotThrowRelicAtEnemyGoal(ABwayCharacterWithAbilities* BotCharacter);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic Bot")
	static bool TryBotPassRelicForward(ABwayCharacterWithAbilities* BotCharacter);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Relic Bot")
	static ABwayCharacterWithAbilities* FindForwardTeammate(const ABwayCharacterWithAbilities* BotCharacter);
};
