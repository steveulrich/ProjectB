// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "BwayAbilityPreviewWidget.generated.h"

class UTexture2D;
class UMediaSource;
class UBorder;
class UImage;
class UCommonTextBlock;

/**
 * UBwayAbilityPreviewWidget
 *
 * Concrete C++ inspection row widget displaying a hero ability or signature buildable:
 * icon, active input keybind badge, ability name, role tag (PRIMARY / KIT / ULTIMATE / BUILDABLE),
 * reconciled cooldown readout, and description.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class BREAKAWAYCORERUNTIME_API UBwayAbilityPreviewWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBwayAbilityPreviewWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool Initialize() override;

	/**
	 * Set the ability or signature buildable to preview
	 * @param AbilityInfo The ability display information
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Preview")
	void SetAbilityPreview(const FAbilityDisplayInfo& AbilityInfo);

	/**
	 * Set the preview directly with individual parameters
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Preview")
	void SetAbilityPreviewDirect(UTexture2D* PreviewImage, const FText& AbilityName, const FText& Description);

	/**
	 * Updates only the resolved input key label (e.g. when local player switches KBM <-> Gamepad)
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Preview")
	void SetInputKeyLabelOverride(const FText& NewKeyLabel);

	/**
	 * Clear the current preview
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Preview")
	void ClearPreview();

	/** Get the current ability info */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Preview")
	const FAbilityDisplayInfo& GetCurrentAbilityInfo() const { return CurrentAbilityInfo; }

	/** Check if there's an active preview */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Preview")
	bool HasActivePreview() const { return bHasPreview; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Preview")
	FText GetDisplayedAbilityName() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Preview")
	FText GetDisplayedKeyLabel() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Preview")
	FText GetDisplayedRoleLabel() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Preview")
	FText GetDisplayedCooldownText() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Preview")
	FText GetDisplayedDescription() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	void EnsureVisualTreeConstructed();
	void RefreshVisualPresentation();

	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Preview|Events")
	void OnAbilityPreviewSet(UTexture2D* PreviewImage, const FText& AbilityName, const FText& Description, bool bIsUltimate);

	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Preview|Events")
	void OnPreviewVideoAvailable(UMediaSource* VideoSource);

	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Preview|Events")
	void OnPreviewCleared();

protected:
	/** Current ability info being displayed */
	UPROPERTY(BlueprintReadOnly, Category = "Ability Preview")
	FAbilityDisplayInfo CurrentAbilityInfo;

	/** Whether there's an active preview */
	UPROPERTY(BlueprintReadOnly, Category = "Ability Preview")
	bool bHasPreview = false;

private:
	/** Cached loaded preview/icon image */
	UPROPERTY()
	TObjectPtr<UTexture2D> LoadedPreviewImage;

	/** Cached loaded video source */
	UPROPERTY()
	TObjectPtr<UMediaSource> LoadedVideoSource;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RowBorder;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> IconBackdropBorder;

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> KeyBadgeBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> KeyBadgeText;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> AbilityNameText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RoleBadgeBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> RoleBadgeText;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> CooldownText;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> DescriptionText;
};
