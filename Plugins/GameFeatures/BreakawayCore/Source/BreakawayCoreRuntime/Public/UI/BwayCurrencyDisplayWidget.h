// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BwayCurrencyDisplayWidget.generated.h"

class UTexture2D;

/**
 * UBwayCurrencyDisplayWidget
 * 
 * A widget for displaying currency amounts with an icon.
 * Used for showing purchase costs, player balance, etc.
 * 
 * Designed to be extended in Blueprint for visual customization.
 */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class BREAKAWAYCORERUNTIME_API UBwayCurrencyDisplayWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBwayCurrencyDisplayWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set the currency amount to display
	 * @param Amount The amount to display
	 */
	UFUNCTION(BlueprintCallable, Category = "Currency Display")
	void SetAmount(int32 Amount);

	/**
	 * Set the currency icon
	 * @param Icon The icon texture to display
	 */
	UFUNCTION(BlueprintCallable, Category = "Currency Display")
	void SetCurrencyIcon(UTexture2D* Icon);

	/**
	 * Set whether the player can afford this amount
	 * @param bCanAfford True if player has enough currency
	 */
	UFUNCTION(BlueprintCallable, Category = "Currency Display")
	void SetCanAfford(bool bCanAfford);

	/** Get the current amount */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Currency Display")
	int32 GetAmount() const { return CurrentAmount; }

	/** Check if player can afford */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Currency Display")
	bool GetCanAfford() const { return bPlayerCanAfford; }

protected:
	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	/**
	 * Called when amount changes
	 * @param NewAmount The new amount
	 * @param FormattedText Formatted text with separators (e.g., "1,234")
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Currency Display|Events")
	void OnAmountChanged(int32 NewAmount, const FText& FormattedText);

	/**
	 * Called when currency icon changes
	 * @param NewIcon The new icon texture
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Currency Display|Events")
	void OnCurrencyIconChanged(UTexture2D* NewIcon);

	/**
	 * Called when affordability state changes
	 * @param bCanAfford True if player can afford
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Currency Display|Events")
	void OnAffordabilityChanged(bool bCanAfford);

protected:
	/** Current displayed amount */
	UPROPERTY(BlueprintReadOnly, Category = "Currency Display")
	int32 CurrentAmount = 0;

	/** Current currency icon */
	UPROPERTY(BlueprintReadOnly, Category = "Currency Display")
	TObjectPtr<UTexture2D> CurrencyIcon;

	/** Whether player can afford this amount */
	UPROPERTY(BlueprintReadOnly, Category = "Currency Display")
	bool bPlayerCanAfford = true;
};

