// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "BwayAbilityPreviewWidget.generated.h"

class UTexture2D;
class UMediaSource;

/**
 * UBwayAbilityPreviewWidget
 * 
 * A widget for displaying ability preview information including
 * preview image/video, ability name, and description.
 * 
 * Designed to be extended in Blueprint for visual customization.
 */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class BREAKAWAYCORERUNTIME_API UBwayAbilityPreviewWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBwayAbilityPreviewWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set the ability to preview
	 * @param AbilityInfo The ability display information
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Preview")
	void SetAbilityPreview(const FAbilityDisplayInfo& AbilityInfo);

	/**
	 * Set the preview directly with individual parameters
	 * @param PreviewImage The preview image
	 * @param AbilityName The ability name
	 * @param Description The ability description
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability Preview")
	void SetAbilityPreviewDirect(UTexture2D* PreviewImage, const FText& AbilityName, const FText& Description);

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

protected:
	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	/**
	 * Called when a new ability preview is set
	 * @param PreviewImage The preview image (may be null)
	 * @param AbilityName The ability name
	 * @param Description The ability description
	 * @param bIsUltimate Whether this is an ultimate ability
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Preview|Events")
	void OnAbilityPreviewSet(UTexture2D* PreviewImage, const FText& AbilityName, const FText& Description, bool bIsUltimate);

	/**
	 * Called when preview video should be played (if available)
	 * @param VideoSource The video media source (may be null)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability Preview|Events")
	void OnPreviewVideoAvailable(UMediaSource* VideoSource);

	/**
	 * Called when preview is cleared
	 */
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
	/** Cached loaded preview image */
	UPROPERTY()
	TObjectPtr<UTexture2D> LoadedPreviewImage;

	/** Cached loaded video source */
	UPROPERTY()
	TObjectPtr<UMediaSource> LoadedVideoSource;
};

