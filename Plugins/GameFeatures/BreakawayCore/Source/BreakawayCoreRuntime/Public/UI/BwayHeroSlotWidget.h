// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "HeroSystems/BwayHeroSelectWidget.h"
#include "BwayHeroSlotWidget.generated.h"

class UTexture2D;
class UImage;
class UCommonTextBlock;
class UBorder;

/**
 * Delegate fired when a hero slot is clicked
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeroSlotClicked, FPrimaryAssetId, HeroId);

/**
 * Delegate fired when a hero slot is hovered or receives controller/keyboard focus
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeroSlotFocusedOrHovered, FPrimaryAssetId, HeroId);

/**
 * UBwayHeroSlotWidget
 *
 * Concrete CommonUI hero card in the staging hero selection roster grid.
 * Displays hero portrait, display name, reconciled class badge (Fighter, Support,
 * Debuff Support, Shooter), selection/focus highlight, teammate ownership indicator,
 * and lock state.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class BREAKAWAYCORERUNTIME_API UBwayHeroSlotWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UBwayHeroSlotWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool Initialize() override;

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
	 * Set whether this slot is currently inspected in the Kit & Buildable panel
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Slot")
	void SetInspectedState(bool bNewInspectedState);

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
	 * Set whether this hero is locked
	 * @param bInLocked True if locked
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Slot")
	void SetLockedState(bool bInLocked);

	/**
	 * Disables/enables grid switching across all slots when the local player locks/unlocks
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Slot")
	void SetLocalPhaseLockedState(bool bInLocalPhaseLocked);

	/** Programmatically invokes slot click when enabled and unlocked */
	UFUNCTION(BlueprintCallable, Category = "Hero Slot")
	bool SimulateClick();

	/** Get the hero ID assigned to this slot */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	FPrimaryAssetId GetHeroId() const { return HeroId; }

	/** Get the hero display name */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	FText GetHeroDisplayName() const { return HeroDisplayName; }

	/** Get the hero's reconciled class badge display text */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	FText GetHeroClassName() const { return HeroClassName; }

	/** Get the hero's portrait texture */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	UTexture2D* GetHeroPortrait() const { return HeroPortrait; }

	/** Check if this slot is currently selected */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	bool IsSlotSelected() const { return bIsSelected; }

	/** Check if this slot is currently locked */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	bool IsSlotLocked() const { return bIsLocked; }

	/** Check if local player lock has disabled grid switching on this slot */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	bool IsLocalPhaseLocked() const { return bLocalPhaseLocked; }

	/** Check if this hero is available */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	bool IsHeroAvailable() const { return bIsAvailable; }

	/** Check if this slot currently has focus visual active */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	bool IsVisuallyFocused() const { return bHasFocusVisual; }

	/** Returns the rendered status badge text (LOCKED / TAKEN / P# / READY) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Slot")
	FText GetStatusBadgeText() const;

	/** Delegate fired when this slot is clicked */
	UPROPERTY(BlueprintAssignable, Category = "Hero Slot|Events")
	FOnHeroSlotClicked OnHeroSlotClicked;

	/** Delegate fired when this slot is hovered or focused (for live kit/3D preview) */
	UPROPERTY(BlueprintAssignable, Category = "Hero Slot|Events")
	FOnHeroSlotFocusedOrHovered OnHeroSlotFocusedOrHovered;

protected:
	//~ Begin UUserWidget / UCommonButtonBase Interface
	virtual void NativeConstruct() override;
	virtual void NativeOnClicked() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	//~ End UUserWidget / UCommonButtonBase Interface

	void EnsureVisualTreeConstructed();
	void RefreshInteractability();
	void RefreshVisualPresentation();

	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnHeroDataSet(UTexture2D* Portrait, const FText& DisplayName);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnSelectionStateChanged(bool bNewIsSelected);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnPlayerIndicatorChanged(int32 PlayerIndex, const FText& PlayerLabel);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnAvailabilityChanged(bool bAvailable);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnLockedStateChanged(bool bIsLockedByOther);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Slot|Events")
	void OnSlotHovered();

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

	/** The hero's reconciled class display name */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	FText HeroClassName;

	/** The hero's class enum */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	EHeroClass HeroClass = EHeroClass::Fighter;

	/** The hero's portrait texture */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	TObjectPtr<UTexture2D> HeroPortrait;

	/** Whether this slot is currently selected */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	bool bIsSelected = false;

	/** Whether this slot is currently inspected in the details panel */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	bool bIsInspected = false;

	/** Whether the hero is available for selection */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	bool bIsAvailable = true;

	/** Whether the hero is locked */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	bool bIsLocked = false;

	/** Whether the local player has locked in (disabling grid switching) */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	bool bLocalPhaseLocked = false;

	/** The player index owning this hero (-1 if none) */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Slot")
	int32 OwningPlayerIndex = -1;

private:
	/**
	 * Native C++ visual tree subwidgets constructed only for direct UBwayHeroSlotWidget::StaticClass()
	 * instances. Blueprint subclasses (e.g. legacy WBP_BW_HeroSlot) own their UMG-authored WidgetTree
	 * and drive their designer variables via OnHeroDataSet / OnSelectionStateChanged hooks without
	 * UClass FProperty collisions.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> NativeOuterFocusBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NativeInnerCardBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NativeClassBadgeBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> NativeClassBadgeText;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> NativeStatusBadgeText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NativePortraitBackdropBorder;

	UPROPERTY(Transient)
	TObjectPtr<UImage> NativePortraitImage;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> NativeHeroNameText;

	bool bUsingNativeVisualTree = false;
	bool bHasFocusVisual = false;
};
