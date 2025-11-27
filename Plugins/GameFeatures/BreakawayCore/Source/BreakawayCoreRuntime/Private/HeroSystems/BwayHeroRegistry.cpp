// Fill out your copyright notice in the Description page of Project Settings.


#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "Engine/AssetManager.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"   // for FModuleManager

TObjectPtr<UBwayHeroRegistry> UBwayHeroRegistry::Get(const UObject* WorldContext)
{
	if (UGameInstance* GI = WorldContext->GetWorld()->GetGameInstance())
	{
		return GI->GetSubsystem<UBwayHeroRegistry>();
	}
	return nullptr;
}

TArray<TSoftObjectPtr<UBwayHeroDataAsset>> UBwayHeroRegistry::GetAllHeroSoftObjects() const
{
	//--------------------------------------------------------------------//
	// 1) Build the list of package roots by iterating every discovered
	//    plugin and keeping the ones whose mount path contains "/Heroes/".
	//--------------------------------------------------------------------//
	TArray<FString> HeroRoots;

	for (const TSharedRef<IPlugin>& Plugin :
		 IPluginManager::Get().GetDiscoveredPlugins())                    // finds *all* plugins
	{
		const FString Mount = Plugin->GetMountedAssetPath();              // virtual root, e.g. "/Hero_Alona/"
		if (Mount.Contains(TEXT("/Hero_")))
		{
			HeroRoots.Add(Mount);
		}
	}

	//--------------------------------------------------------------------//
	// 2) Build a FARFilter that lists every root we found.
	//--------------------------------------------------------------------//
	FARFilter Filter;
	Filter.bRecursivePaths = true;

	for (const FString& Root : HeroRoots)
	{
		Filter.PackagePaths.Add(*Root);                                   // add as FName
	}

	Filter.ClassPaths.Add(
		UBwayHeroDataAsset::StaticClass()->GetClassPathName());           // new API, no deprecation warning

	//--------------------------------------------------------------------//
	// 3) Query the Asset Registry once, convert to soft pointers.
	//--------------------------------------------------------------------//
	TArray<FAssetData> Found;
	Registry->GetAssets(Filter, Found);                                   // fast, metadata-only

	TArray<TSoftObjectPtr<UBwayHeroDataAsset>> Out;
	Out.Reserve(Found.Num());
	for (const FAssetData& Data : Found)
	{
		Out.Emplace(Data.ToSoftObjectPath());
	}
	return Out;     // typically <1 ms even with 100+ heroes
}

UBwayHeroDataAsset* UBwayHeroRegistry::LoadHeroSync(const TSoftObjectPtr<UBwayHeroDataAsset>& SoftPtr) const
{
	return Cast<UBwayHeroDataAsset>(SoftPtr.IsValid() ?
			SoftPtr.Get() :
			SoftPtr.LoadSynchronous());
}

UBwayHeroDataAsset* UBwayHeroRegistry::GetHeroDataById(const FPrimaryAssetId& HeroId)
{
	if (!HeroId.IsValid())
	{
		return nullptr;
	}

	UObject* Asset = UAssetManager::Get().GetPrimaryAssetObject(HeroId);
	if (!Asset)
	{
		// Force-load synchronously if needed
		FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(HeroId);
		Asset = AssetPath.TryLoad();
	}

	return Cast<UBwayHeroDataAsset>(Asset);
}

void UBwayHeroRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	Registry = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	
}
