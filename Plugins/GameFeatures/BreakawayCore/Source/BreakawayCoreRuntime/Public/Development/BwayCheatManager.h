// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/LyraCheatManager.h"
#include "BwayCheatManager.generated.h"

class UUserWidget;
class UBwayHeroDataAsset;

/**
 * UBwayCheatManager
 *
 * Breakaway-specific cheat manager with dev commands for hero selection testing.
 * Extends LyraCheatManager to add:
 * - ShowHeroSelect: Opens the hero selection UI
 * - SelectHero: Directly selects a hero by name
 * - RespawnAsHero: Respawns the player as their selected hero
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayCheatManager : public ULyraCheatManager
{
	GENERATED_BODY()

public:
	UBwayCheatManager();

	/**
	 * Opens the hero selection UI for the local player.
	 * Console command: ShowHeroSelect
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Breakaway|Cheats")
	void ShowHeroSelect();

	/**
	 * Closes the hero selection UI if it's open.
	 * Console command: HideHeroSelect
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Breakaway|Cheats")
	void HideHeroSelect();

	/**
	 * Directly selects a hero by name (partial match supported).
	 * Console command: SelectHero <HeroName>
	 * Example: SelectHero Spartacus
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Breakaway|Cheats")
	void SelectHero(FString HeroName);

	/**
	 * Respawns the player with their currently selected hero.
	 * Console command: RespawnAsHero
	 */
	UFUNCTION(Exec, BlueprintAuthorityOnly, BlueprintCallable, Category = "Breakaway|Cheats")
	void RespawnAsHero();

	/**
	 * Lists all available heroes in the console.
	 * Console command: ListHeroes
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Breakaway|Cheats")
	void ListHeroes();

	/**
	 * Combined command: Select hero and respawn immediately.
	 * Console command: ChangeHero <HeroName>
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Breakaway|Cheats")
	void ChangeHero(FString HeroName);

	/**
	 * Toggle the hero selection UI visibility.
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Cheats")
	void ToggleHeroSelect();

	/**
	 * Check if the hero selection UI is currently visible.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Cheats")
	bool IsHeroSelectVisible() const;

	// ========== DEBUG COMMANDS ==========

	/**
	 * Print current hero info to console/log.
	 * Shows hero name, ID, and ability summary.
	 * Console command: DebugHero
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Breakaway|Debug")
	void DebugHero();

	/**
	 * List all granted abilities with their input tags.
	 * Console command: DebugAbilities
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Breakaway|Debug")
	void DebugAbilities();

	/**
	 * Toggle on-screen hero debug display.
	 * Shows hero name, ability count, and activation log.
	 * Console command: ToggleHeroDebug
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Breakaway|Debug")
	void ToggleHeroDebug();

	/**
	 * Force initialize hero data on current character.
	 * Debug command to manually trigger InitializeHeroData.
	 * Console command: ForceInitHero
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Breakaway|Debug")
	void ForceInitHero();

	/**
	 * Server: force-fumble the relic if the local pawn (or current carrier) is holding it.
	 * Console command: ForceFumbleRelic
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Breakaway|Cheats")
	void ForceFumbleRelic();

	/** Widget class to use for hero selection UI */
	UPROPERTY(EditDefaultsOnly, Category = "Breakaway|Cheats")
	TSoftClassPtr<UUserWidget> HeroSelectionWidgetClass;

protected:
	/** Current hero selection widget instance (if open) */
	UPROPERTY()
	TObjectPtr<UUserWidget> HeroSelectWidgetInstance;

	/** Called when hero selection is confirmed from the widget */
	UFUNCTION()
	void OnHeroSelectionConfirmed();

	/** Find a hero data asset by partial name match */
	UBwayHeroDataAsset* FindHeroByName(const FString& HeroName) const;

	/** Get the owning player controller */
	APlayerController* GetOwningPlayerController() const;
};

