// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "BwayTeamBridgeComponent.generated.h"

class ABwayGameState;

class ULyraExperienceDefinition;

/**
 * Bridge component that synchronizes Breakaway's custom team system
 * (FTeamInfo arrays on BwayGameState) with Lyra's team subsystem.
 *
 * Purpose:
 * - Allows Lyra's built-in HUD, scoreboard, and gameplay cue systems to
 *   query team information using the standard ILyraTeamAgentInterface
 * - Provides team color resolution for existing Lyra widgets
 * - Ensures GameplayCues can automatically apply team-colored effects
 *
 * This component observes BwayGameState team changes and forwards them
 * to the ULyraTeamSubsystem so both systems stay in sync.
 *
 * Lives on the GameState as a modular component.
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class BREAKAWAYCORERUNTIME_API UBwayTeamBridgeComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayTeamBridgeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Force a full sync of all team data from BwayGameState to Lyra's team subsystem */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Teams")
	void SyncAllTeamsToLyra();

	/** Sync a single player's team assignment to Lyra */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Breakaway|Teams")
	void SyncPlayerTeamToLyra(APlayerState* PlayerState, int32 TeamIndex);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Bind to BwayGameState team change events */
	void BindToTeamEvents();

	/** After Lyra TeamCreation assigns players, re-push Breakaway roster onto Lyra. */
	void HandleExperienceLoaded(const ULyraExperienceDefinition* Experience);
};
