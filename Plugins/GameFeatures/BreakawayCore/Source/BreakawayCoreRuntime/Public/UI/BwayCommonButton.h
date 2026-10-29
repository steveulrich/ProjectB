// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Types/SlateEnums.h"
#include "BwayCommonButton.generated.h"

class UBorder;
class UCommonTextBlock;

/**
 * Transparent SButton backing style so UBwayCommonButton renders clean Breakaway
 * focus/hover borders without default Slate button bevel chrome.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayTransparentCommonButtonStyle : public UCommonButtonStyle
{
	GENERATED_BODY()

public:
	UBwayTransparentCommonButtonStyle();
};

/**
 * Concrete C++ CommonUI button used across Breakaway Front End, Server Browser,
 * Pause Menu, and Staging Hero Select controls.
 *
 * Maintains a single UCommonTextBlock child inside its WidgetTree so CommonUI
 * gamepad/keyboard focus, confirm actions, and text-inspection probes work uniformly.
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayCommonButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UBwayCommonButton(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool Initialize() override;

	UFUNCTION(BlueprintCallable, Category = "Breakaway|UI")
	void SetLabelText(const FText& InText);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|UI")
	FText GetLabelText() const;

	UFUNCTION(BlueprintCallable, Category = "Breakaway|UI")
	void SetFontSize(int32 InFontSize);

	void SetJustification(ETextJustify::Type InJustification);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|UI")
	void SetContentPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|UI")
	void SetButtonColors(
		const FLinearColor& InNormalBackground,
		const FLinearColor& InFocusedBackground,
		const FLinearColor& InFocusRingColor,
		const FLinearColor& InNormalTextColor = FLinearColor::White);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|UI")
	void SetHighlightedState(bool bInHighlighted);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|UI")
	bool IsVisuallyFocused() const { return bHasFocusVisual; }

	/** Programmatically invokes the CommonUI click path when enabled and interactable. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|UI")
	bool SimulateClick();

	UCommonTextBlock* GetLabelWidget() const { return LabelText; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	void EnsureVisualTreeConstructed();
	void RefreshVisualState();

private:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> OuterFocusBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> InnerBackgroundBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> LabelText;

	FText CachedLabelText;
	int32 FontSize = 16;
	TEnumAsByte<ETextJustify::Type> TextJustification = ETextJustify::Center;
	FMargin ContentPadding = FMargin(20.f, 11.f);

	FLinearColor NormalBackgroundColor = FLinearColor(0.06f, 0.11f, 0.20f, 0.96f);
	FLinearColor FocusedBackgroundColor = FLinearColor(0.13f, 0.24f, 0.40f, 0.99f);
	FLinearColor NormalRingColor = FLinearColor(0.18f, 0.30f, 0.46f, 0.85f);
	FLinearColor FocusRingColor = FLinearColor(0.98f, 0.82f, 0.30f, 1.00f);
	FLinearColor HoverRingColor = FLinearColor(0.24f, 0.78f, 0.96f, 1.00f);
	FLinearColor DisabledBackgroundColor = FLinearColor(0.05f, 0.07f, 0.10f, 0.65f);
	FLinearColor DisabledRingColor = FLinearColor(0.14f, 0.17f, 0.22f, 0.55f);
	FLinearColor NormalTextColor = FLinearColor::White;
	FLinearColor DisabledTextColor = FLinearColor(0.50f, 0.55f, 0.62f, 0.85f);

	bool bHighlighted = false;
	bool bHasFocusVisual = false;
};
