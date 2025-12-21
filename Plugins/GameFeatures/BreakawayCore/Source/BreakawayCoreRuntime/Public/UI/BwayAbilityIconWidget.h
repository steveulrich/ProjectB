// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "BwayAbilityIconWidget.generated.h"

class UTexture2D;

/**
 * Delegate fired when an ability icon is clicked
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityIconClicked, int32, AbilityIndex);

/**
 * Delegate fired when an ability icon is hovered
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityIconHovered, int32, AbilityIndex);

/**
 * UBwayAbilityIconWidget
 * 
 * A button widget representing a single ability in the character select ability bar.
 * Handles icon display, selection highlighting, and hover states.
 * When selected, the ability preview panel shows this ability's details.
 * 
 * Designed to be extended in Blueprint for visual customization.
 */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class BREAKAWAYCORERUNTIME_API UBwayAbilityIconWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UBwayAbilityIconWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set the ability data for this icon
	 * @param InAbilityInfo The display information for the ability
	 * @param InAbilityIndex The index of this ability (0-5)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Icon")
	void SetAbilityData(const FAbilityDisplayInfo& InAbilityInfo, int32 InAbilityIndex);

	/**
	 * Set the ability icon directly
	 * @param Icon The icon texture
	 * @param Name The ability name
	 * @param Description The ability description
	 * @param InAbilityIndex The index of this ability
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Icon")
	void SetAbilityDataDirect(UTexture2D* Icon, const FText& Name, const FText& Description, int32 InAbilityIndex);

	/**
	 * Set whether this icon is currently selected
	 * @param bInSelected True if this ability is selected
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Icon")
	void SetSelected(bool bInSelected);

	/**
	 * Set whether this ability is locked (not yet unlocked)
	 * @param bInLocked True if ability is locked
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Icon")
	void SetLocked(bool bInLocked);

	/**
	 * Mark this as the "cancel" slot (X button in the UI)
	 * @param bInIsCancelSlot True if this is the cancel slot
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Icon")
	void SetIsCancelSlot(bool bInIsCancelSlot);

	/** Get the ability index */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Icon")
	int32 GetAbilityIndex() const { return AbilityIndex; }

	/** Get the ability display info */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Icon")
	const FAbilityDisplayInfo& GetAbilityInfo() const { return AbilityInfo; }

	/** Check if this icon is selected */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Icon")
	bool IsIconSelected() const { return bIsSelected; }

	/** Check if this ability is locked */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Icon")
	bool IsAbilityLocked() const { return bIsLocked; }

	/** Check if this is the cancel slot */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Icon")
	bool IsCancelSlot() const { return bCancelSlot; }

	/** Delegate fired when this icon is clicked */
	UPROPERTY(BlueprintAssignable, Category = "Ability Icon|Events")
	FOnAbilityIconClicked OnAbilityIconClicked;

	/** Delegate fired when this icon is hovered */
	UPROPERTY(BlueprintAssignable, Category = "Ability Icon|Events")
	FOnAbilityIconHovered OnAbilityIconHovered;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeOnClicked() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	//~ End UUserWidget Interface

	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	/**
	 * Called when ability data is set - update your visuals here
	 * @param Icon The ability's icon texture (may be null)
	 * @param AbilityName The ability's display name
	 * @param bIsUltimate Whether this is an ultimate ability
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Icon|Events")
	void OnAbilityDataSet(UTexture2D* Icon, const FText& AbilityName, bool bIsUltimate);

	/**
	 * Called when selection state changes
	 * @param bNewIsSelected True if now selected
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Icon|Events")
	void OnSelectionStateChanged(bool bNewIsSelected);

	/**
	 * Called when locked state changes
	 * @param bIsAbilityLocked True if ability is locked
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Icon|Events")
	void OnLockedStateChanged(bool bIsAbilityLocked);

	/**
	 * Called when this is set as a cancel slot
	 * @param bIsCancelSlot True if this is the cancel slot
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Icon|Events")
	void OnCancelSlotStateChanged(bool bIsCancelSlot);

	/**
	 * Called when the icon is hovered
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Icon|Events")
	void OnIconHovered();

	/**
	 * Called when the icon is unhovered
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Icon|Events")
	void OnIconUnhovered();

protected:
	// ========== STATE ==========

	/** The ability display info */
	UPROPERTY(BlueprintReadOnly, Category = "Ability Icon")
	FAbilityDisplayInfo AbilityInfo;

	/** The index of this ability in the ability bar */
	UPROPERTY(BlueprintReadOnly, Category = "Ability Icon")
	int32 AbilityIndex = -1;

	/** Whether this icon is currently selected */
	UPROPERTY(BlueprintReadOnly, Category = "Ability Icon")
	bool bIsSelected = false;

	/** Whether the ability is locked */
	UPROPERTY(BlueprintReadOnly, Category = "Ability Icon")
	bool bIsLocked = false;

	/** Whether this is the cancel/exit slot */
	UPROPERTY(BlueprintReadOnly, Category = "Ability Icon")
	bool bCancelSlot = false;

private:
	/** Cached loaded icon texture */
	UPROPERTY()
	TObjectPtr<UTexture2D> LoadedIconTexture;
};

