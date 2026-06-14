// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CustomGameConfig.generated.h"

/**
 * Mutable custom-lobby settings that persist on the game instance across front-end navigation.
 * Used by the matchmaking subsystem custom-game path (11-MM-4).
 */
UCLASS(BlueprintType)
class LYRAGAME_API UCustomGameConfig : public UObject
{
	GENERATED_BODY()

public:
	/** When true, session creation bypasses queue mock and uses overrides below. */
	UPROPERTY(BlueprintReadWrite, Category = "Custom Game")
	bool bIsCustomGameDefinition = false;

	/** Party / lobby team size override for custom games. */
	UPROPERTY(BlueprintReadWrite, Category = "Custom Game", meta = (ClampMin = "1"))
	int32 CustomMaxPlayersPerTeam = 4;

	/** Experience passed as Experience= URL arg when hosting a custom session. */
	UPROPERTY(BlueprintReadWrite, Category = "Custom Game", meta = (AllowedTypes = "LyraExperienceDefinition"))
	FPrimaryAssetId SelectedExperienceOverride;

	/** Map passed to UCommonSession_HostSessionRequest::MapID for custom sessions. */
	UPROPERTY(BlueprintReadWrite, Category = "Custom Game", meta = (AllowedTypes = "Map"))
	FPrimaryAssetId SelectedMapOverride;

	/** Optional URL options merged into the host session request (e.g. SkipHeroSelection=1). */
	UPROPERTY(BlueprintReadWrite, Category = "Custom Game")
	TMap<FString, FString> ExtraArgs;
};
