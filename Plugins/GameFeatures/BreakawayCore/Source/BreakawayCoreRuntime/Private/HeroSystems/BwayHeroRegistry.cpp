// Fill out your copyright notice in the Description page of Project Settings.

#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"

void UBwayHeroRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Game-feature mounts may not exist during Asset Manager's initial scan.
	// By game-instance startup their configured hero folders are available.
	UAssetManager& AssetManager = UAssetManager::Get();
	const FPrimaryAssetType HeroType(TEXT("HeroDataAsset"));
	FPrimaryAssetTypeInfo TypeInfo;
	if (AssetManager.GetPrimaryAssetTypeInfo(HeroType, TypeInfo) && TypeInfo.AssetBaseClassLoaded)
	{
		TArray<FString> ScanPaths;
		// Runtime type info does not retain the configured directory list.
		for (const FPrimaryAssetTypeInfo& ConfigType : GetDefault<UAssetManagerSettings>()->PrimaryAssetTypesToScan)
		{
			if (ConfigType.PrimaryAssetType == HeroType.GetName())
			{
				for (const FDirectoryPath& Directory : ConfigType.GetDirectories())
				{
					ScanPaths.AddUnique(Directory.Path);
				}
				for (const FSoftObjectPath& Asset : ConfigType.GetSpecificAssets())
				{
					ScanPaths.AddUnique(Asset.ToString());
				}
			}
		}
		// AssetManager caches even not-yet-mounted scan paths. Refresh registry
		// metadata directly so that cache cannot hide newly mounted hero content.
		TArray<FString> MountedDirectories;
		for (const FString& Path : ScanPaths)
		{
			const FString Directory = Path.Contains(TEXT("."))
				? FPackageName::GetLongPackagePath(FPackageName::ObjectPathToPackageName(Path)) : Path;
			FString Filename;
			if (FPackageName::TryConvertLongPackageNameToFilename(Directory / TEXT(""), Filename))
			{
				MountedDirectories.AddUnique(Directory);
			}
		}
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get()
			.ScanPathsSynchronous(MountedDirectories, true);
		const int32 Count = AssetManager.ScanPathsForPrimaryAssets(HeroType,
			ScanPaths, TypeInfo.AssetBaseClassLoaded,
			TypeInfo.bHasBlueprintClasses, TypeInfo.bIsEditorOnly, true);
		UE_LOG(LogTemp, Log, TEXT("BwayHeroRegistry: Refreshed configured hero discovery (%d assets)"), Count);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroRegistry: HeroDataAsset scan configuration is unavailable"));
	}
}

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

FPrimaryAssetId UBwayHeroRegistry::ResolveHeroIdByName(const FString& HeroName)
{
	if (HeroName.IsEmpty())
	{
		return FPrimaryAssetId();
	}

	const FPrimaryAssetType HeroAssetType(TEXT("HeroDataAsset"));
	const FString TrimmedName = HeroName.TrimStartAndEnd();

	TArray<FString> CandidateNames;
	CandidateNames.Add(TrimmedName);
	if (!TrimmedName.StartsWith(TEXT("DA_"), ESearchCase::IgnoreCase))
	{
		CandidateNames.Add(FString::Printf(TEXT("DA_BW_HeroData_%s"), *TrimmedName));
		CandidateNames.Add(FString::Printf(TEXT("DA_BW_Hero_%s"), *TrimmedName));
	}

	for (const FString& CandidateName : CandidateNames)
	{
		const FPrimaryAssetId CandidateId(HeroAssetType, FName(*CandidateName));
		if (GetHeroDataById(CandidateId))
		{
			return CandidateId;
		}
	}

	TArray<FPrimaryAssetId> HeroIds;
	UAssetManager::Get().GetPrimaryAssetIdList(HeroAssetType, HeroIds);

	for (const FPrimaryAssetId& CandidateId : HeroIds)
	{
		if (CandidateId.PrimaryAssetName.ToString().Equals(TrimmedName, ESearchCase::IgnoreCase))
		{
			return CandidateId;
		}
	}

	for (const FPrimaryAssetId& CandidateId : HeroIds)
	{
		if (UBwayHeroDataAsset* HeroData = GetHeroDataById(CandidateId))
		{
			if (HeroData->DisplayName.ToString().Equals(TrimmedName, ESearchCase::IgnoreCase))
			{
				return CandidateId;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("BwayHeroRegistry: ResolveHeroIdByName('%s') failed — %d HeroDataAsset(s) registered"),
		*TrimmedName, HeroIds.Num());

	return FPrimaryAssetId();
}
