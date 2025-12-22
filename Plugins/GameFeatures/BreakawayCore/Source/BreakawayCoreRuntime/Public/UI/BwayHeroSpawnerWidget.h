// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BwayHeroSpawnerWidget.generated.h"

class UTexture2D;

/**
 * UBwayHeroSpawnerWidget
 * 
 * A world-space widget for displaying hero information above a hero test spawner.
 * Shows the hero's name and portrait.
 * 
 * Designed to be extended in Blueprint for visual customization.
 * 
 * Blueprint Implementation:
 * - Override OnHeroInfoChanged to update visual elements
 * - Bind Portrait and DisplayName to your UI elements
 */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class BREAKAWAYCORERUNTIME_API UBwayHeroSpawnerWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBwayHeroSpawnerWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set the hero information to display
	 * @param InDisplayName The hero's display name
	 * @param InPortrait The hero's portrait texture
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Spawner Widget")
	void SetHeroInfo(const FText& InDisplayName, UTexture2D* InPortrait);

	/**
	 * Clear the displayed hero information
	 */
	UFUNCTION(BlueprintCallable, Category = "Hero Spawner Widget")
	void ClearHeroInfo();

	/** Get the current display name */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Spawner Widget")
	FText GetDisplayName() const { return DisplayName; }

	/** Get the current portrait texture */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Spawner Widget")
	UTexture2D* GetPortrait() const { return Portrait; }

	/** Check if hero info is currently set */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Spawner Widget")
	bool HasHeroInfo() const { return bHasHeroInfo; }

protected:
	// ========================================
	// Blueprint Implementable Events
	// ========================================

	/**
	 * Called when hero info changes
	 * Implement in Blueprint to update visual elements
	 * @param NewDisplayName The new hero name (empty if cleared)
	 * @param NewPortrait The new portrait texture (null if cleared)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Spawner Widget|Events")
	void OnHeroInfoChanged(const FText& NewDisplayName, UTexture2D* NewPortrait);

	/**
	 * Called when hero info is cleared
	 * Implement in Blueprint to hide or reset visual elements
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Spawner Widget|Events")
	void OnHeroInfoCleared();

	// ========================================
	// Data
	// ========================================

	/** The hero's display name */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Spawner Widget")
	FText DisplayName;

	/** The hero's portrait texture */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Spawner Widget")
	TObjectPtr<UTexture2D> Portrait;

	/** Whether hero info is currently set */
	UPROPERTY(BlueprintReadOnly, Category = "Hero Spawner Widget")
	bool bHasHeroInfo = false;
};

