// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BwayCurrencyDisplayWidget.generated.h"

class UTexture2D;
class UAbilitySystemComponent;
class UImage;
class UTextBlock;
struct FOnAttributeChangeData;

/**
 * UBwayCurrencyDisplayWidget
 *
 * A widget for displaying currency amounts with an icon.
 * Used for showing purchase costs, player balance, etc.
 *
 * Designed to be extended in Blueprint for visual customization.
 */
UCLASS(Abstract, Blueprintable)
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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void HandleCurrentGoldChanged(const FOnAttributeChangeData& ChangeData);
	void TryBindToCurrentGold();
	void UnbindFromCurrentGold();

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

	/** Optional direct bindings for display-only match widgets. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Currency Display|Widgets")
	TObjectPtr<UTextBlock> Text_Amount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Currency Display|Widgets")
	TObjectPtr<UImage> Image_CurrencyIcon;

	UPROPERTY(EditDefaultsOnly, Category = "Currency Display", meta = (ClampMin = "0.05"))
	float BindingRetryInterval = 0.25f;

	FDelegateHandle CurrentGoldChangedHandle;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	float TimeSinceLastBindingAttempt = 0.0f;
};

