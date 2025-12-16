// Fill out your copyright notice in the Description page of Project Settings.

#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "Engine/AssetManager.h"

TObjectPtr<UBwayHeroRegistry> UBwayHeroRegistry::Get(const UObject* WorldContext)
{
	if (!WorldContext || !WorldContext->GetWorld())
	{
		return nullptr;
	}
	
	if (UGameInstance* GI = WorldContext->GetWorld()->GetGameInstance())
	{
		return GI->GetSubsystem<UBwayHeroRegistry>();
	}
	return nullptr;
}

TArray<TSoftObjectPtr<UBwayHeroDataAsset>> UBwayHeroRegistry::GetAllHeroSoftObjects() const
{
	TArray<TSoftObjectPtr<UBwayHeroDataAsset>> Out;

	// Get all primary asset IDs of type "HeroDataAsset" from Asset Manager
	TArray<FPrimaryAssetId> HeroIds;
	UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType("HeroDataAsset"), HeroIds);

	UE_LOG(LogTemp, Log, TEXT("BwayHeroRegistry: Found %d heroes via Asset Manager"), HeroIds.Num());

	// Convert each ID to a soft object pointer
	Out.Reserve(HeroIds.Num());
	for (const FPrimaryAssetId& HeroId : HeroIds)
	{
		FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(HeroId);
		if (AssetPath.IsValid())
		{
			Out.Emplace(AssetPath);
		}
	}

	return Out;
}

UBwayHeroDataAsset* UBwayHeroRegistry::LoadHeroSync(const TSoftObjectPtr<UBwayHeroDataAsset>& SoftPtr) const
{
	if (!SoftPtr.IsNull())
	{
		return Cast<UBwayHeroDataAsset>(SoftPtr.IsValid() ? SoftPtr.Get() : SoftPtr.LoadSynchronous());
	}
	return nullptr;
}

UBwayHeroDataAsset* UBwayHeroRegistry::GetHeroDataById(const FPrimaryAssetId& HeroId)
{
	if (!HeroId.IsValid())
	{
		return nullptr;
	}

	// Try to get already-loaded asset first
	UObject* Asset = UAssetManager::Get().GetPrimaryAssetObject(HeroId);
	
	if (!Asset)
	{
		// Force-load synchronously if needed
		FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(HeroId);
		if (AssetPath.IsValid())
		{
			Asset = AssetPath.TryLoad();
		}
	}

	return Cast<UBwayHeroDataAsset>(Asset);
}
