// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GameModes/BwayMatchPhaseTypes.h"
#include "Player/LyraPlayerController.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "BwayPlayerController.generated.h"

class UUserWidget;
class UBwayPostRoundSummaryWidget;
class UBwayRoundManagementComponent;
class UBwayCombatNumberPopComponent;
class UBwayNameplatePolicyComponent;

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
	UFUNCTION(Exec, BlueprintCallable, Category="Breakaway|Economy")
	void ToggleUpgradeShop();

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
	void Server_SubmitPreSelectedHero(FPrimaryAssetId PreSelectedHeroId, bool bLockSelection);

	/** Show hero selection UI on the owning client. */
	UFUNCTION(Client, Reliable)
	void Client_ShowHeroSelection(const TSoftClassPtr<UCommonActivatableWidget>& WidgetClass);

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
	void Client_ShowResults(int32 WinningTeam, int32 Team1Score, int32 Team2Score, int32 TotalRounds, const TSoftClassPtr<UUserWidget>& WidgetClass);

	/**
	 * Requests the server to return all clients to the front-end map.
	 * Bound to the results-screen "Return to Lobby" button.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RequestReturnToFrontEnd();

	/** Remove the results widget and restore gameplay input before front-end travel. */
	UFUNCTION(Client, Reliable)
	void Client_DismissResultsScreen();

	/** Restores mouse capture and hides the cursor for local gameplay (e.g. after frontend UI or hero select). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Input")
	void RestoreGameplayInputMode();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakaway|Combat")
	TObjectPtr<UBwayCombatNumberPopComponent> CombatNumberPopComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakaway|Combat")
	TObjectPtr<UBwayNameplatePolicyComponent> NameplatePolicyComponent;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UBwayUpgradeShopWidget> UpgradeShopWidget;
	void BindPostRoundSummaryListeners();
	void UnbindPostRoundSummaryListeners();

	UFUNCTION()
	void HandlePostRoundSummaryStarted(FBwayPostRoundSummaryData SummaryData);

	UFUNCTION()
	void HandleMatchPhaseChanged(EBwayMatchPhase NewPhase);

	void ShowPostRoundSummary(const FBwayPostRoundSummaryData& SummaryData);
	void DismissPostRoundSummary();

	TSubclassOf<UUserWidget> ResolvePostRoundSummaryWidgetClass() const;

	/** Active hero-selection widget pushed through Lyra's CommonUI layer stack. */
	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidget> HeroSelectionWidget;

	/** Active results widget, kept so we can clean it up if the match restarts. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ResultsWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBwayPostRoundSummaryWidget> PostRoundSummaryWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBwayRoundManagementComponent> BoundRoundManagementComponent;
};
