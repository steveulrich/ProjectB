// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/LyraPlayerController.h"
#include "BwayPlayerController.generated.h"

/**
 * ABwayPlayerController
 *
 * Breakaway-specific player controller that extends LyraPlayerController.
 * Sets up the Breakaway cheat manager for dev testing commands.
 * 
 * Dev Commands (open console with ~ key):
 * - ShowHeroSelect: Opens hero selection UI
 * - HideHeroSelect: Closes hero selection UI
 * - SelectHero <name>: Select a hero by name
 * - RespawnAsHero: Respawn with selected hero
 * - ChangeHero <name>: Select and respawn in one command
 * - ListHeroes: Show all available heroes
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API ABwayPlayerController : public ALyraPlayerController
{
	GENERATED_BODY()

public:
	ABwayPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Opens the hero selection UI for this player.
	 * Can be called from Blueprint or bound to input in Blueprint.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|HeroSelection")
	void ShowHeroSelect();

	/**
	 * Hides the hero selection UI for this player.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|HeroSelection")
	void HideHeroSelect();

	/**
	 * Toggle the hero selection UI.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|HeroSelection")
	void ToggleHeroSelect();
};

