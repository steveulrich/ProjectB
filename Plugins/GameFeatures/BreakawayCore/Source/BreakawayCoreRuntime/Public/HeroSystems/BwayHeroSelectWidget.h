// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "BwayHeroSelectWidget.generated.h"

class UBwayHeroRegistry;
class UBwayHeroSelectionManager;
class ABwayPlayerState;

// Delegate for when hero selection is confirmed (for dev tools)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeroConfirmedDelegate);

/**
 * Struct containing all display info for a hero in the UI
 */
USTRUCT(BlueprintType)
struct FHeroDisplayInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FPrimaryAssetId HeroId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	/** The hero's class display text (e.g., "FIGHTER", "TANK") */
	UPROPERTY(BlueprintReadOnly)
	FText ClassName;

	/** The hero's class enum value */
	UPROPERTY(BlueprintReadOnly)
	EHeroClass HeroClass = EHeroClass::Fighter;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UTexture2D> Portrait;

	UPROPERTY(BlueprintReadOnly)
	FHeroStats Stats;

	/** Ability display information for the character select UI */
	UPROPERTY(BlueprintReadOnly)
	TArray<FAbilityDisplayInfo> Abilities;

	UPROPERTY(BlueprintReadOnly)
	bool bIsAvailable = true;

	UPROPERTY(BlueprintReadOnly)
	bool bIsSelected = false;

	UPROPERTY(BlueprintReadOnly)
	bool bIsLocked = false;

	/** Player index who has selected this hero (-1 if none) */
	UPROPERTY(BlueprintReadOnly)
	int32 SelectedByPlayerIndex = -1;
};

/**
 * UBwayHeroSelectWidget
 * 
 * Base class for hero selection UI
 * Handles communication with PlayerState and HeroSelectionManager
 * Designed to be extended in Blueprint for visual implementation
 */
UCLASS(Abstract, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayHeroSelectWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayHeroSelectWidget(const FObjectInitializer& ObjectInitializer);

protected:
	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~End of UUserWidget interface

	//~UCommonActivatableWidget interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	//~End of UCommonActivatableWidget interface

public:
	// ========== BLUEPRINT CALLABLE FUNCTIONS ==========

	/**
	 * Get all available heroes for display
	 * Call this to populate your hero grid/list
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	TArray<FHeroDisplayInfo> GetAvailableHeroes();

	/**
	 * Get the currently selected hero for the local player
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	FPrimaryAssetId GetSelectedHeroId() const;

	/**
	 * Check if the local player's selection is locked
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	bool IsSelectionLocked() const;

	/**
	 * Select a hero (sends request to server)
	 * Returns false if the hero is unavailable
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	bool SelectHero(FPrimaryAssetId HeroId);

	/**
	 * Lock the current hero selection
	 * Returns false if no hero is selected
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	bool LockSelection();

	/**
	 * Confirm the current selection and close the widget.
	 * This locks the selection and broadcasts OnHeroConfirmed.
	 * Used by dev tools to trigger respawn after selection.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	void ConfirmSelection();

	/**
	 * Delegate fired when hero selection is confirmed.
	 * Dev tools can bind to this to trigger respawn.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Hero Selection Events")
	FOnHeroConfirmedDelegate OnHeroConfirmed;

	/**
	 * Get the local player's team index
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	int32 GetLocalPlayerTeam() const;

	/**
	 * Check if a specific hero is available for selection
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	bool IsHeroAvailable(FPrimaryAssetId HeroId) const;

	/**
	 * Get remaining selection time (in seconds)
	 * Returns -1 if no time limit
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	float GetRemainingSelectionTime() const;

	/**
	 * Get number of players ready vs total
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	void GetReadyPlayerCount(int32& OutReady, int32& OutTotal) const;

	/**
	 * Get the full hero data asset for a hero ID
	 * @param HeroId The hero to look up
	 * @return The hero data asset (may be null)
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	UBwayHeroDataAsset* GetHeroDataAsset(FPrimaryAssetId HeroId) const;

	/**
	 * Get the display info for a specific hero
	 * @param HeroId The hero to look up
	 * @param OutDisplayInfo The display info struct to populate
	 * @return True if the hero was found
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	bool GetHeroDisplayInfo(FPrimaryAssetId HeroId, FHeroDisplayInfo& OutDisplayInfo) const;

	/**
	 * Get the currently selected hero's full display info
	 * @param OutDisplayInfo The display info struct to populate
	 * @return True if a hero is selected
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	bool GetSelectedHeroDisplayInfo(FHeroDisplayInfo& OutDisplayInfo) const;

	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	/**
	 * Called when the hero list should be refreshed
	 * Implement this to update your UI with GetAvailableHeroes()
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnHeroListChanged();

	/**
	 * Called when the local player selects a different hero
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnLocalSelectionChanged(FPrimaryAssetId NewHeroId);

	/**
	 * Called when the local player locks their selection
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnLocalSelectionLocked(FPrimaryAssetId LockedHeroId);

	/**
	 * Called when another player selects a hero
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnOtherPlayerSelectionChanged(APlayerState* PlayerState, FPrimaryAssetId HeroId, int32 TeamIndex);

	/**
	 * Called when the selection timer updates
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnSelectionTimerUpdated(float RemainingTime);

	/**
	 * Called when all players are ready
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnAllPlayersReady();

protected:
	// ========== INTERNAL STATE ==========

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	TObjectPtr<ABwayPlayerState> LocalPlayerState;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	TObjectPtr<UBwayHeroSelectionManager> SelectionManager;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	TObjectPtr<UBwayHeroRegistry> HeroRegistry;

private:
	// Timer for updating selection countdown
	FTimerHandle SelectionTimerHandle;

	// Bind to player state and selection manager
	void BindToPlayerState();
	void UnbindFromPlayerState();
	void BindToSelectionManager();
	void UnbindFromSelectionManager();

	// Delegate callbacks
	UFUNCTION()
	void HandlePlayerSelectionChanged(FPrimaryAssetId NewHeroId);

	UFUNCTION()
	void HandlePlayerHeroLocked(FPrimaryAssetId LockedHeroId);

	UFUNCTION()
	void HandleOtherPlayerSelectionChanged(APlayerState* PlayerState, FPrimaryAssetId HeroId, int32 TeamIndex);

	UFUNCTION()
	void HandleAllPlayersReady();

	// Update timer display
	void UpdateSelectionTimer();

	// Cache of hero data for quick access
	UPROPERTY()
	TArray<TObjectPtr<UBwayHeroDataAsset>> CachedHeroData;

	bool bIsInitialized = false;
};
