// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BwayCurrencyDisplayWidget.generated.h"

class UAbilitySystemComponent;
class UBorder;
class UBwayUpgradeComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UImage;
class UInputAction;
class UTextBlock;
class UTexture2D;
struct FOnAttributeChangeData;

/**
 * UBwayCurrencyDisplayWidget
 *
 * A widget for displaying currency amounts with an icon.
 * In match HUD wallet mode (W_BW_GoldWidget), also displays configured MaxGold cap,
 * dynamic IA_BW_UpgradeShop key/gamepad prompt, and pulses only when an eligible
 * and affordable upgrade purchase exists.
 */
UCLASS(Abstract, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayCurrencyDisplayWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBwayCurrencyDisplayWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Currency Display")
	void SetAmount(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Currency Display")
	void SetCurrencyIcon(UTexture2D* Icon);

	UFUNCTION(BlueprintCallable, Category = "Currency Display")
	void SetCanAfford(bool bCanAfford);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Currency Display")
	int32 GetAmount() const { return CurrentAmount; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Currency Display")
	bool GetCanAfford() const { return bPlayerCanAfford; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Currency Display")
	int32 GetDisplayedMaxGold() const { return CurrentMaxGold; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Currency Display")
	FText GetDisplayedCapText() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Currency Display")
	FText GetResolvedShopKeyLabel() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Currency Display")
	FText GetDisplayedShopPromptText() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Currency Display")
	bool IsShopPromptPulsing() const { return bShopPromptShouldPulse; }

	/** Refresh wallet cap, shop prompt keybind, and affordability/eligibility pulse state. */
	UFUNCTION(BlueprintCallable, Category = "Currency Display")
	void RefreshWalletHUDState();

	void SetWalletHUDModeForTest(bool bEnable);
	void SetInputDeviceOverrideForTest(bool bEnableOverride, bool bPreferGamepad);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	void HandleCurrentGoldChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxGoldChanged(const FOnAttributeChangeData& ChangeData);
	void TryBindToCurrentGold();
	void UnbindFromCurrentGold();

	UFUNCTION()
	void HandleOwnedUpgradesChanged();

	UFUNCTION()
	void HandleControlMappingsRebuilt();

	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	UFUNCTION(BlueprintImplementableEvent, Category = "Currency Display|Events")
	void OnAmountChanged(int32 NewAmount, const FText& FormattedText);

	UFUNCTION(BlueprintImplementableEvent, Category = "Currency Display|Events")
	void OnCurrencyIconChanged(UTexture2D* NewIcon);

	UFUNCTION(BlueprintImplementableEvent, Category = "Currency Display|Events")
	void OnAffordabilityChanged(bool bCanAfford);

protected:
	/** Current displayed amount */
	UPROPERTY(BlueprintReadOnly, Category = "Currency Display")
	int32 CurrentAmount = 0;

	/** Configured MaxGold cap from UBwayGoldAttributeSet (0 if unset/unbound). */
	UPROPERTY(BlueprintReadOnly, Category = "Currency Display")
	int32 CurrentMaxGold = 0;

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
	FDelegateHandle MaxGoldChangedHandle;
	FDelegateHandle InputMethodChangedHandle;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UBwayUpgradeComponent> BoundUpgradeComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BoundInputSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<const UInputAction> CachedUpgradeShopAction;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CachedGoldFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_MaxGoldCap;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_ShopPrompt;

	float TimeSinceLastBindingAttempt = 0.0f;
	float PulseAccumulatedSeconds = 0.0f;
	bool bIsWalletHUDWidget = false;
	bool bForceWalletHUDMode = false;
	bool bShopPromptShouldPulse = false;
	bool bTestOverrideInputDevice = false;
	bool bTestPreferGamepad = false;

private:
	void EnsureWalletHUDWidgets();
	void BindInputDelegates();
	void UnbindInputDelegates();
	bool EvaluateEligibleAffordablePurchase() const;
	void ApplyPulseVisuals(float PulseAlpha);
};
