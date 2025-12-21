// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "HeroSystems/BwayHeroSelectWidget.h"
#include "BwayHeroSlotWidget.generated.h"

class UTexture2D;
class UImage;
class UTextBlock;
class UBorder;

/**
 * Delegate fired when a hero slot is clicked
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeroSlotClicked, FPrimaryAssetId, HeroId);

/**
 * UBwayHeroSlotWidget
 * 
 * A button widget representing a single hero in the character selection grid.
 * Handles portrait display, selection highlighting, availability states,
 * and player ownership indicators (P1, P2, etc.).
 * 
 * Designed to be extended in Blueprint for visual customization.
 */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class BREAKAWAYCORERUNTIME_API UBwayHeroSlotWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UBwayHeroSlotWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set the hero data for this slot
	 * @param HeroInfo The display information for the hero
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Slot")
	void SetHeroData(const FHeroDisplayInfo& HeroInfo);

	/**
	 * Set whether this slot is currently selected
	 * @param bNewSelectedState True if this hero is selected by the local player
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Slot")
	void SetSelectionState(bool bNewSelectedState);

	/**
	 * Set the player indicator badge (P1, P2, etc.)
	 * @param PlayerIndex The player index (0-based), -1 to hide
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Slot")
	void SetPlayerIndicator(int32 PlayerIndex);

	/**
	 * Set whether this hero is available for selection
	 * @param bAvailable True if the hero can be selected
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Slot")
	void SetAvailability(bool bAvailable);

	/**
	 * Set whether this hero is locked (owned by another player on the team)
	 * @param bInLocked True if locked by another player
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Slot")
	void SetLockedState(bool bInLocked);

	/** Get the hero ID assigned to this slot */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	FPrimaryAssetId GetHeroId() const { return HeroId; }

	/** Check if this slot is currently selected */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	bool IsSlotSelected() const { return bIsSelected; }

	/** Check if this hero is available */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	bool IsHeroAvailable() const { return bIsAvailable; }

	/** Delegate fired when this slot is clicked */
	UPROPERTY(BlueprintAssignable, Category = "Hero Slot|Events")
	FOnHeroSlotClicked OnHeroSlotClicked;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeOnClicked() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	//~ End UUserWidget Interface

	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	/**
	 * Called when hero data is set - update your visuals here
	 * @param Portrait The hero's portrait texture (may be null)
	 * @param DisplayName The hero's display name
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnHeroDataSet(UTexture2D* Portrait, const FText& DisplayName);

	/**
	 * Called when selection state changes
	 * @param bNewIsSelected True if now selected
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnSelectionStateChanged(bool bNewIsSelected);

	/**
	 * Called when player indicator should update
	 * @param PlayerIndex The player index (0-based), -1 means no indicator
	 * @param PlayerLabel The text to display (e.g., "P1", "P2")
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnPlayerIndicatorChanged(int32 PlayerIndex, const FText& PlayerLabel);

	/**
	 * Called when availability state changes
	 * @param bAvailable True if hero is available
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnAvailabilityChanged(bool bAvailable);

	/**
	 * Called when locked state changes
	 * @param bIsLockedByOther True if locked by another player
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnLockedStateChanged(bool bIsLockedByOther);

	/**
	 * Called when the slot is hovered
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnSlotHovered();

	/**
	 * Called when the slot is unhovered
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnSlotUnhovered();

protected:
	// ========== STATE ==========

	/** The hero ID this slot represents */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	FPrimaryAssetId HeroId;

	/** The hero's display name */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	FText HeroDisplayName;

	/** The hero's portrait texture */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	TObjectPtr<UTexture2D> HeroPortrait;

	/** Whether this slot is currently selected */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	bool bIsSelected = false;

	/** Whether the hero is available for selection */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	bool bIsAvailable = true;

	/** Whether the hero is locked (selected by teammate) */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	bool bIsLocked = false;

	/** The player index owning this hero (-1 if none) */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	int32 OwningPlayerIndex = -1;
};

