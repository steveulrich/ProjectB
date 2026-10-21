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
class UBwayRespawnHUDWidget;
class ABuildableActor;
class ABwayPlayerState;

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
	UFUNCTION(BlueprintCallable, Category="Breakaway|Economy")
	void OpenUpgradeShop();
	const class UInputAction* GetUpgradeShopInputAction() const { return UpgradeShopInputAction; }

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

	/** Only the local authority host can retry a rejected staging travel. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|HeroSelection")
	bool CanRetryHeroSelectionTravel() const;
	UFUNCTION(BlueprintCallable, Category = "Breakaway|HeroSelection")
	bool RetryHeroSelectionTravel();

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

	/** Start the 5-8 second Arena Intro cinematic camera pan across the Shrine of Auros / center Relic spawn. */
	UFUNCTION(Client, Reliable)
	void Client_StartArenaIntroFlyThrough(float DurationSeconds);

	// ========================================
	// Post-Match Results
	// ========================================

	/**
	 * Sends a complete authoritative results snapshot to the owning client for local UI.
	 * Called from ABwayGameState::ShowResultsScreen_Implementation on the server for each PC.
	 * Runs on the owning client only (RPC), guaranteeing remote clients on a dedicated
	 * server also see their results screen.
	 */
	UFUNCTION(Client, Reliable)
	void Client_ShowResults(const FBwayPostMatchSummaryData& Summary, const TSoftClassPtr<UUserWidget>& WidgetClass);

	/**
	 * Requests the server to return all clients to the front-end map.
	 * Bound to the results-screen "Return to Lobby" button.
	 */
	UFUNCTION(Server, Reliable)
	void Server_RequestReturnToFrontEnd();

	/** Request a server-owned fresh match; accepted only during PostMatch. */
	UFUNCTION(Server, Reliable)
	void Server_RequestPlayAgain();

	/** Remove the results widget and restore gameplay input before front-end travel. */
	UFUNCTION(Client, Reliable)
	void Client_DismissResultsScreen();

	/** Development-only probe used by the team-combat fixture to sample replicated peer state. */
	UFUNCTION(Client, Reliable)
	void Client_RunTeamPeerConvergence(int32 EnemyPlayerId);

	/** Development-only remote-owner fixture; runs production damage/pass input on a remote client. */
	UFUNCTION(Client, Reliable)
	void Client_RunTeamRemoteAction(int32 TeammatePlayerId, int32 EnemyPlayerId, bool bPassPhase);

	/** Development-only owned fixture: leave through the local GameInstance session flow. */
	UFUNCTION(Client, Reliable)
	void Client_RunCreditDisconnectProbe();

	/** Development-only packaged fixture; sends the normal owner purchase RPC. */
	UFUNCTION(Client, Reliable)
	void Client_RunRespawnUpgradePurchase(int32 ExpectedRank, int32 AuditStage);

	/** Development-only fixture: activate/submit through the real owning-client GAS path. */
	UFUNCTION(Client, Reliable)
	void Client_RunBuildableAuditAction(int32 Action, int32 Sequence);

	/** Development-only independent journal for an authority-selected player's buildables. */
	UFUNCTION(Client, Reliable)
	void Client_RunBuildableAuditObserver(int32 ObservedPlayerId);

	/** Development-only journal for the unmodified rolling Siege Engine lifecycle. */
	UFUNCTION(Client, Reliable)
	void Client_RunSiegeAuditObserver(int32 ObservedPlayerId);

	/** Development-only actual primary activation and independent buildable state journal. */
	UFUNCTION(Client, Reliable)
	void Client_RunBuildableCombatProbe(ABuildableActor* Target, int32 Sequence, int32 FiringPlayerId);

	/** Development-only independent Siege movement, health and destruction journal. */
	UFUNCTION(Client, Reliable)
	void Client_RunSiegeCombatProbe(ABuildableActor* Siege, ABuildableActor* Enemy, ABuildableActor* Friendly, int32 Sequence);

	/** Development-only independent aura health, pawn and registry observations. */
	UFUNCTION(Client, Reliable)
	void Client_RunShrineAuraProbe(ABwayPlayerState* OwnerPlayer, ABwayPlayerState* Ally, ABwayPlayerState* Enemy, ABwayPlayerState* Outside, int32 Phase);

	/** Development-only independent Ward speed, status, pawn and registry observations. */
	UFUNCTION(Client, Reliable)
	void Client_RunWardAuraProbe(ABwayPlayerState* OwnerPlayer, ABwayPlayerState* Ally, ABwayPlayerState* Enemy, ABwayPlayerState* Outside, int32 Phase);

	/** Development-only Jail attachment, status and registry observations; phase 2 requests the owner's real primary. */
	UFUNCTION(Client, Reliable)
	void Client_RunJailCaptureProbe(ABwayPlayerState* Placer, ABwayPlayerState* Ally, ABwayPlayerState* Victim, ABwayPlayerState* SecondVictim, int32 Phase);

	/** Development-only aim handoff used by the remote-owner fixture before a server-only pass. */
	UFUNCTION(Server, Reliable)
	void Server_SetTeamRemoteAim(FRotator AimRotation, int32 TeammatePlayerId);

	/** Restores the active CommonUI screen's input, or gameplay capture when no screen owns it. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Input")
	void RestoreGameplayInputMode();
	virtual void ResetIgnoreInputFlags() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakaway|Combat")
	TObjectPtr<UBwayCombatNumberPopComponent> CombatNumberPopComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakaway|Combat")
	TObjectPtr<UBwayNameplatePolicyComponent> NameplatePolicyComponent;

protected:
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;

	/** Replaceable presentation skin; canonical timing remains on PlayerState. */
	UPROPERTY(EditDefaultsOnly, Category="Breakaway|UI")
	TSubclassOf<UBwayRespawnHUDWidget> RespawnHUDWidgetClass;

private:
	void RefreshRespawnHUD();
	UPROPERTY(Transient)
	TObjectPtr<UBwayRespawnHUDWidget> RespawnHUDWidget;
	/** Shared with the authored default mapping context; survives pawn replacement. */
	UPROPERTY(Transient)
	TObjectPtr<class UInputAction> UpgradeShopInputAction;
	TWeakObjectPtr<class UEnhancedInputComponent> ShopBoundInputComponent;
	uint32 ShopInputBindingHandle = 0;
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

	void TickArenaIntroFlyThrough();
	void StopArenaIntroFlyThrough();

	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> ArenaIntroCameraActor;

	FTimerHandle ArenaIntroTickTimerHandle;
	double ArenaIntroStartTime = 0.0;
	float ArenaIntroTotalDuration = 5.5f;
	FVector ArenaIntroCenterLocation = FVector::ZeroVector;
};
