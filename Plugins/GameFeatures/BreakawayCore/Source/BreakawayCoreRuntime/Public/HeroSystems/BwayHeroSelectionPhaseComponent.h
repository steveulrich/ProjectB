#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GameplayTagContainer.h"
#include "Components/GameStateComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "BwayHeroSelectionPhaseComponent.generated.h"

class UBwayHeroSelectionManager;
class ABwayPlayerState;
class UBwayHeroDataAsset;
class ULyraGamePhaseAbility;
class ULyraExperienceDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeroSelectionPhaseEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeroSelectionPhaseEndedEvent);
/**
 * Phase component that manages the hero selection phase.
 * Integrates with Lyra's GamePhaseSubsystem to drive phase transitions:
 *   HeroSelection → (next phase, e.g. Warmup/Playing)
 *
 * Handles UI display, waiting for players, and spawning heroes.
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayHeroSelectionPhaseComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UBwayHeroSelectionPhaseComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	/**
	 * Start the hero selection phase.
	 * Shows UI to all players and starts selection timer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection Phase")
	void StartHeroSelectionPhase();

	/**
	 * End the hero selection phase.
	 * Validates selections, assigns defaults, hides UI, and spawns heroes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection Phase")
	void EndHeroSelectionPhase();

	/**
	 * Skip hero selection and assign default heroes.
	 * Useful for testing or single-player.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection Phase")
	void SkipHeroSelection();

	/** Returns true while the replicated hero-selection phase is running. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection Phase")
	bool IsHeroSelectionActive() const { return bPhaseActive; }

	/** Returns true once this component has finished hero selection for the current match. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection Phase")
	bool HasHeroSelectionCompleted() const { return bHeroSelectionCompleted; }

	/** Whether GameMode should hold player spawning until hero selection completes. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection Phase")
	bool ShouldBlockPlayerSpawning() const { return bBlockPlayerSpawningUntilComplete && !bHeroSelectionCompleted; }

	// ========== CONFIGURATION ==========

	/**
	 * Widget class to show for hero selection.
	 * Should be a subclass of BwayHeroSelectWidget.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Selection Phase")
	TSoftClassPtr<UCommonActivatableWidget> HeroSelectionWidgetClass;

	/**
	 * Default hero to assign if player doesn't select.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Selection Phase")
	TSoftObjectPtr<UBwayHeroDataAsset> DefaultHeroData;

	/**
	 * Whether to show UI on listen server (false = dedicated server only shows for clients).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Selection Phase")
	bool bShowUIOnListenServer = true;

	/**
	 * Gameplay tag identifying this phase (must match the phase ability's GamePhaseTag).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Selection Phase")
	FGameplayTag PhaseTag;

	/**
	 * Phase ability class to start AFTER hero selection completes (e.g. Warmup or Playing phase).
	 * If not set, the component will log an error but hero selection will still function.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Selection Phase")
	TSubclassOf<ULyraGamePhaseAbility> NextPhaseAbilityClass;

	/**
	 * Optional fallback map to load after hero selection when the current URL contains HeroSelectStaging.
	 * The URL option HeroSelectTargetMap takes precedence when present.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Selection Phase|Travel", meta = (AllowedTypes = "Map"))
	FPrimaryAssetId PostHeroSelectionMapID;

	/**
	 * Optional fallback experience to pass after hero selection when the current URL contains HeroSelectStaging.
	 * The URL option HeroSelectTargetExperience takes precedence when present.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Selection Phase|Travel", meta = (AllowedTypes = "LyraExperienceDefinition"))
	FPrimaryAssetId PostHeroSelectionExperienceID;

	/**
	 * When true, BreakawayGameMode will not spawn player pawns until this phase
	 * completes. Disable only for test experiences that intentionally skip hero select.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hero Selection Phase")
	bool bBlockPlayerSpawningUntilComplete = true;

	// ========== EVENTS ==========

	/** Called when hero selection phase starts */
	UPROPERTY(BlueprintAssignable, Category = "Hero Selection Phase")
	FOnHeroSelectionPhaseEvent OnHeroSelectionPhaseStarted;

	/** Called when hero selection phase ends */
	UPROPERTY(BlueprintAssignable, Category = "Hero Selection Phase")
	FOnHeroSelectionPhaseEndedEvent OnHeroSelectionPhaseEnded;

protected:
	// Show UI to all players - implement in Blueprint
	UFUNCTION(BlueprintNativeEvent, Category = "Hero Selection Phase")
	void ShowHeroSelectionUI();
	virtual void ShowHeroSelectionUI_Implementation();

	// Hide UI from all players - implement in Blueprint
	UFUNCTION(BlueprintNativeEvent, Category = "Hero Selection Phase")
	void HideHeroSelectionUI();
	virtual void HideHeroSelectionUI_Implementation();

	// Spawn a hero for a specific player - implement in Blueprint
	UFUNCTION(BlueprintNativeEvent, Category = "Hero Selection Phase")
	void SpawnHeroForPlayer(ABwayPlayerState* PlayerState);
	virtual void SpawnHeroForPlayer_Implementation(ABwayPlayerState* PlayerState);

	/** Internal callback when Lyra's Game Phase system activates the HeroSelection phase */
	UFUNCTION()
	void HandleLyraPhaseActivated(const FGameplayTag& InPhaseTag);

	void HandleExperienceLoaded(const ULyraExperienceDefinition* Experience);
	
	/** Start the next Lyra phase after hero selection is done */
	void EndPhaseAndProgressToNext();

	bool ShouldSkipHeroSelectionPhase() const;
	bool ShouldTravelToPostHeroSelectionMap() const;
	bool TryTravelToPostHeroSelectionMap() const;

private:

	// Called when all players are ready
	UFUNCTION()
	void HandleAllPlayersReady();

	// Spawn heroes for all players based on their selections
	void SpawnHeroesForAllPlayers();

	// Assign a default hero to players who didn't select
	void AssignDefaultHeroes(bool bOnlyBots = false);

	// Get reference to selection manager
	UBwayHeroSelectionManager* GetSelectionManager() const;

	// Always run the C++ CommonUI push/pop path, even when Blueprint overrides the events.
	void PushHeroSelectionUIToPlayers();
	void PopHeroSelectionUIFromPlayers();

	// Track if phase is active
	bool bPhaseActive = false;

	bool bHeroSelectionCompleted = false;
};
