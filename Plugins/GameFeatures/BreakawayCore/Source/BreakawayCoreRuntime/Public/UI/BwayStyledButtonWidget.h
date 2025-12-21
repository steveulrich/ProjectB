// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "BwayStyledButtonWidget.generated.h"

/**
 * Button style preset for quick styling
 */
UENUM(BlueprintType)
enum class EBwayButtonStyle : uint8
{
	Primary     UMETA(DisplayName = "Primary (Orange)"),
	Secondary   UMETA(DisplayName = "Secondary (Red)"),
	Tertiary    UMETA(DisplayName = "Tertiary (Gray)"),
	Positive    UMETA(DisplayName = "Positive (Green)"),
	Negative    UMETA(DisplayName = "Negative (Red)"),
	Ghost       UMETA(DisplayName = "Ghost (Transparent)")
};

/**
 * UBwayStyledButtonWidget
 * 
 * A styled button widget for consistent UI across the game.
 * Supports different style presets (Primary, Secondary, etc.)
 * and handles text display with proper CommonUI input hints.
 * 
 * Designed to be extended in Blueprint for visual customization.
 */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class BREAKAWAYCORERUNTIME_API UBwayStyledButtonWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UBwayStyledButtonWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set the button text
	 * @param InText The text to display on the button
	 */
	UFUNCTION(BlueprintCallable, Category = "Styled Button")
	void SetButtonText(const FText& InText);

	/**
	 * Set the button style preset
	 * @param InStyle The style preset to apply
	 */
	UFUNCTION(BlueprintCallable, Category = "Styled Button")
	void SetButtonStyle(EBwayButtonStyle InStyle);

	/**
	 * Set whether the button shows a loading state
	 * @param bInIsLoading True to show loading spinner
	 */
	UFUNCTION(BlueprintCallable, Category = "Styled Button")
	void SetIsLoading(bool bInIsLoading);

	/**
	 * Set a custom icon to display
	 * @param Icon The icon texture (null to hide icon)
	 */
	UFUNCTION(BlueprintCallable, Category = "Styled Button")
	void SetButtonIcon(UTexture2D* Icon);

	/** Get the current button text */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Styled Button")
	FText GetButtonText() const { return ButtonText; }

	/** Get the current button style */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Styled Button")
	EBwayButtonStyle GetButtonStyle() const { return CurrentStyle; }

	/** Check if button is in loading state */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Styled Button")
	bool IsLoading() const { return bIsLoading; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	//~ End UUserWidget Interface

	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	/**
	 * Called when button text changes
	 * @param NewText The new button text
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Styled Button|Events")
	void OnButtonTextChanged(const FText& NewText);

	/**
	 * Called when button style changes
	 * @param NewStyle The new style preset
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Styled Button|Events")
	void OnButtonStyleChanged(EBwayButtonStyle NewStyle);

	/**
	 * Called when loading state changes
	 * @param bLoading True if now loading
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Styled Button|Events")
	void OnLoadingStateChanged(bool bLoading);

	/**
	 * Called when button icon changes
	 * @param NewIcon The new icon (may be null)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Styled Button|Events")
	void OnButtonIconChanged(UTexture2D* NewIcon);

protected:
	/** The button text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Styled Button")
	FText ButtonText;

	/** The button style preset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Styled Button")
	EBwayButtonStyle CurrentStyle = EBwayButtonStyle::Primary;

	/** Whether the button is in loading state */
	UPROPERTY(BlueprintReadOnly, Category = "Styled Button")
	bool bIsLoading = false;

	/** Current button icon */
	UPROPERTY(BlueprintReadOnly, Category = "Styled Button")
	TObjectPtr<UTexture2D> ButtonIcon;
};

