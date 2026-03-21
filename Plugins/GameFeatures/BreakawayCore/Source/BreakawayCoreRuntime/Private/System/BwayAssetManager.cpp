// Copyright Epic Games, Inc. All Rights Reserved.

#include "System/BwayAssetManager.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAssetManager)

UBwayAssetManager::UBwayAssetManager()
{
}

UBwayAssetManager& UBwayAssetManager::Get()
{
	check(GEngine);

	if (UBwayAssetManager* Singleton = Cast<UBwayAssetManager>(GEngine->AssetManager))
	{
		return *Singleton;
	}

	UE_LOG(LogTemp, Fatal, TEXT("BwayAssetManager: Invalid AssetManagerClassName in DefaultEngine.ini. "
		"It must be set to /Script/BreakawayCoreRuntime.BwayAssetManager"));

	// This line will never reach, but satisfies the compiler
	return *NewObject<UBwayAssetManager>();
}

void UBwayAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	RegisterPrimaryAssetTypes();

	UE_LOG(LogTemp, Log, TEXT("BwayAssetManager: Initial loading started"));
}

void UBwayAssetManager::RegisterPrimaryAssetTypes()
{
	// Additional scan paths can be registered here for custom asset types
	// E.g., scanning for all BwayHeroDataAsset instances in a specific directory
	UE_LOG(LogTemp, Log, TEXT("BwayAssetManager: Primary asset types registered"));
}

void UBwayAssetManager::LoadHeroDataAsync(const FPrimaryAssetId& HeroId, FStreamableDelegate OnLoaded)
{
	if (!HeroId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayAssetManager: Invalid HeroId for async load"));
		OnLoaded.ExecuteIfBound();
		return;
	}

	TArray<FName> BundlesToLoad;
	// Load the "Game" bundle which includes meshes, animations, VFX
	BundlesToLoad.Add(FName("Game"));

	LoadPrimaryAsset(HeroId, BundlesToLoad, OnLoaded);
	UE_LOG(LogTemp, Log, TEXT("BwayAssetManager: Loading hero data async: %s"), *HeroId.ToString());
}

void UBwayAssetManager::LoadAllHeroDataAsync(FStreamableDelegate OnAllLoaded)
{
	TArray<FPrimaryAssetId> HeroIds;
	GetPrimaryAssetIdList(FPrimaryAssetType("BwayHeroDataAsset"), HeroIds);

	if (HeroIds.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayAssetManager: No hero data assets found to load"));
		OnAllLoaded.ExecuteIfBound();
		return;
	}

	TArray<FName> BundlesToLoad;
	BundlesToLoad.Add(FName("Game"));

	LoadPrimaryAssets(HeroIds, BundlesToLoad, OnAllLoaded);
	UE_LOG(LogTemp, Log, TEXT("BwayAssetManager: Loading %d hero data assets async"), HeroIds.Num());
}

void UBwayAssetManager::LoadRelicSettingsAsync(const FPrimaryAssetId& RelicSettingsId, FStreamableDelegate OnLoaded)
{
	if (!RelicSettingsId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayAssetManager: Invalid RelicSettingsId for async load"));
		OnLoaded.ExecuteIfBound();
		return;
	}

	LoadPrimaryAsset(RelicSettingsId, TArray<FName>(), OnLoaded);
	UE_LOG(LogTemp, Log, TEXT("BwayAssetManager: Loading relic settings async: %s"), *RelicSettingsId.ToString());
}
