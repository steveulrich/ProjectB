// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/LyraPlayerController.h"
#include "BwayPlayerController.generated.h"

class UUserWidget;

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

	/** Ask the owning client to send its frontend-selected hero back to the server. */
	UFUNCTION(Client, Reliable)
	void Client_RequestPreSelectedHero();

	/** Server receives the owning client's frontend-selected hero. */
	UFUNCTION(Server, Reliable)
	void Server_SubmitPreSelectedHero(FPrimaryAssetId PreSelectedHeroId);

	/** Show hero selection UI on the owning client. */
	UFUNCTION(Client, Reliable)
	void Client_ShowHeroSelection(const TSoftClassPtr<UUserWidget>& WidgetClass);

	/** Hide hero selection UI on the owning client. */
	UFUNCTION(Client, Reliable)
	void Client_HideHeroSelection();

	// ========================================
	// Post-Match Results
	// ========================================

	/**
	 * Replicates the post-match results widget to the owning client.
	 * Called from ABwayGameState::ShowResultsScreen_Implementation on the server for each PC.
	 * Runs on the owning client only (RPC), guaranteeing remote clients on a dedicated
	 * server also see their results screen.
	 */
	UFUNCTION(Client, Reliable)
	void Client_ShowResults(int32 WinningTeam, const TSoftClassPtr<UUserWidget>& WidgetClass);

	/**
	 * Requests the server to return all clients to the front-end map.
	 * Bound to the results-screen "Return to Lobby" button.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RequestReturnToFrontEnd();

private:
	/** Active hero-selection widget, owned by the local player controller. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> HeroSelectionWidget;

	/** Active results widget, kept so we can clean it up if the match restarts. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ResultsWidget;
};

