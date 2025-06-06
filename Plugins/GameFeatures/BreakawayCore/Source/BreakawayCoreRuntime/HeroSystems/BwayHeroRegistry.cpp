// Fill out your copyright notice in the Description page of Project Settings.


#include "BwayHeroRegistry.h"

#include "BwayHeroDataAsset.h"
#include "GameFeatures/LyraGameFeaturePolicy.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Modules/ModuleManager.h"   // for FModuleManager

TObjectPtr<UBwayHeroRegistry> UBwayHeroRegistry::Get(const UObject* WorldContext)
{
	if (UGameInstance* GI = WorldContext->GetWorld()->GetGameInstance())
	{
		return GI->GetSubsystem<UBwayHeroRegistry>();
	}
	return nullptr;
}

void UBwayHeroRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 1. Get all loaded GameFeatureData assets that have a HeroData reference
	TArray<FAssetIdentifier> HeroAssets;
	UGameFeaturesSubsystem::Get().FilterInactivePluginAssets(HeroAssets);

	// 2. Use Asset Registry to find UHeroDataAsset in each GFP’s Data
	FAssetRegistryModule& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	for (const FAssetIdentifier& ID : HeroAssets)
	{
		if (ID.PrimaryAssetType == TEXT("HeroDataAsset"))
		{
			UObject* Obj = UAssetManager::Get().GetPrimaryAssetObject(ID.GetPrimaryAssetId());
			if (UBwayHeroDataAsset* HeroData = Cast<UBwayHeroDataAsset>(Obj))
			{
				RegisteredHeroes.Add(HeroData);
			}
		}
	}

	// 3. Also bind to future HigGameFeatureActivating events (see Step 4)
}

TArray<UBwayHeroDataAsset*> UBwayHeroRegistry::GetAllHeroes() const
{
	TArray<UBwayHeroDataAsset*> Heroes;
	Heroes.Reserve(RegisteredHeroes.Num()); // Good practice: preallocate memory

	for (const TObjectPtr<const UBwayHeroDataAsset>& HeroDataPtr : RegisteredHeroes)
	{
		if (HeroDataPtr) // TObjectPtr can be checked for null directly
		{
			// HeroDataPtr.Get() returns 'const UBwayHeroDataAsset*'
			// We need to cast away the const for the TArray<UBwayHeroDataAsset*>
			// that Blueprints expect. This is generally safe for DataAssets passed to BP.
			Heroes.Add(const_cast<UBwayHeroDataAsset*>(HeroDataPtr.Get()));
		}
	}
	return Heroes;
}

void UBwayHeroRegistry::RegisterHero(const UBwayHeroDataAsset* HeroData)
{
	TObjectPtr<const UBwayHeroDataAsset> HeroDataAsTObjectPtr = HeroData;

	if (HeroData && !RegisteredHeroes.Contains(HeroDataAsTObjectPtr))
	{
		RegisteredHeroes.Add(HeroDataAsTObjectPtr);
	}
}

void UBwayHeroRegistry::UnregisterHero(const UBwayHeroDataAsset* HeroData)
{
	TObjectPtr<const UBwayHeroDataAsset> HeroDataAsTObjectPtr = HeroData;
	
	if (HeroData && RegisteredHeroes.Contains(HeroDataAsTObjectPtr))
	{
		RegisteredHeroes.Remove(HeroDataAsTObjectPtr);
	}
}
