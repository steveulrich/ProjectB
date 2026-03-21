// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "BwayAssetManager.generated.h"

/**
 * Custom Asset Manager for Breakaway.
 * Extends UAssetManager (not Lyra's, to avoid coupling) to handle
 * async loading of hero data, buildable definitions, and gameplay effects.
 *
 * Register in DefaultEngine.ini:
 *   [/Script/Engine.Engine]
 *   AssetManagerClassName=/Script/BreakawayCoreRuntime.BwayAssetManager
 *
 * Usage:
 *   UBwayAssetManager& AssetMgr = UBwayAssetManager::Get();
 *   AssetMgr.LoadHeroDataAsync(HeroId, FStreamableDelegate::CreateLambda([](){ ... }));
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:
	UBwayAssetManager();

	/** Get the singleton instance, typed to our subclass */
	static UBwayAssetManager& Get();

	/** Start the initial asset loading (called by engine on startup) */
	virtual void StartInitialLoading() override;

	// ========================================
	// Async Loading Helpers
	// ========================================

	/** Load a hero data asset by its PrimaryAssetId. Calls Delegate on completion. C++-only (FStreamableDelegate is not Blueprint-compatible). */
	void LoadHeroDataAsync(const FPrimaryAssetId& HeroId, FStreamableDelegate OnLoaded);

	/** Load all registered hero data assets. Calls Delegate when all are loaded. C++-only. */
	void LoadAllHeroDataAsync(FStreamableDelegate OnAllLoaded);

	/** Load a relic settings asset. C++-only. */
	void LoadRelicSettingsAsync(const FPrimaryAssetId& RelicSettingsId, FStreamableDelegate OnLoaded);

protected:
	/** Register primary asset types with scan paths */
	void RegisterPrimaryAssetTypes();
};
