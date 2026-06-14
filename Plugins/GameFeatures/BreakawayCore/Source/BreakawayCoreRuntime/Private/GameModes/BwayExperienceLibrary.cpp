// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/BwayExperienceLibrary.h"

#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameModes/LyraExperienceDefinition.h"
#include "LyraLogChannels.h"

namespace BwayExperienceLibraryPrivate
{
	static const FName ExperienceTypeName(TEXT("LyraExperienceDefinition"));
	static const TCHAR* ExperienceRootPath = TEXT("/BreakawayCore/Experiences");

	static const TArray<FName> KnownExperienceNames = {
		TEXT("B_BW_Experience_Dev"),
		TEXT("B_BW_Experience_CaptureTheRelic"),
		TEXT("B_BW_Experience_HeroSelectStaging"),
	};

	static FSoftObjectPath MakeExperienceObjectPath(FName AssetName)
	{
		const FString AssetString = AssetName.ToString();
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), ExperienceRootPath, *AssetString, *AssetString));
	}

	static IAssetRegistry& GetAssetRegistry()
	{
		return FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	}

	static bool RegisterExperienceFromRegistry(FPrimaryAssetId ExperienceId)
	{
		if (!ExperienceId.IsValid())
		{
			return false;
		}

		UAssetManager& AssetManager = UAssetManager::Get();
		FAssetData ExistingData;
		if (AssetManager.GetPrimaryAssetData(ExperienceId, ExistingData))
		{
			return true;
		}

		IAssetRegistry& Registry = GetAssetRegistry();
		const FSoftObjectPath ObjectPath = MakeExperienceObjectPath(ExperienceId.PrimaryAssetName);
		FAssetData RegistryData = Registry.GetAssetByObjectPath(ObjectPath, /*bIncludeOnlyOnDiskAssets*/ true);
		if (!RegistryData.IsValid())
		{
			UE_LOG(LogLyraExperience, Warning, TEXT("BwayExperience: %s is not in the cooked asset registry at %s (repackage with BreakawayCore enabled)"),
				*ExperienceId.ToString(), *ObjectPath.ToString());
			return false;
		}

		if (!AssetManager.RegisterSpecificPrimaryAsset(ExperienceId, RegistryData))
		{
			UE_LOG(LogLyraExperience, Warning, TEXT("BwayExperience: Failed to register %s from %s"),
				*ExperienceId.ToString(), *ObjectPath.ToString());
			return false;
		}

		UE_LOG(LogLyraExperience, Log, TEXT("BwayExperience: Registered packaged experience %s from %s"),
			*ExperienceId.ToString(), *ObjectPath.ToString());
		return true;
	}
}

void UBwayExperienceLibrary::RegisterPackagedBreakawayExperiences()
{
	if (!UAssetManager::IsInitialized())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.ScanPathsSynchronous({BwayExperienceLibraryPrivate::ExperienceRootPath});

	const FPrimaryAssetType ExperienceType(BwayExperienceLibraryPrivate::ExperienceTypeName);
	for (const FName& ExperienceName : BwayExperienceLibraryPrivate::KnownExperienceNames)
	{
		BwayExperienceLibraryPrivate::RegisterExperienceFromRegistry(FPrimaryAssetId(ExperienceType, ExperienceName));
	}
}

bool UBwayExperienceLibrary::EnsureExperienceRegistered(FPrimaryAssetId ExperienceId)
{
	if (!UAssetManager::IsInitialized() || !ExperienceId.IsValid())
	{
		return false;
	}

	return BwayExperienceLibraryPrivate::RegisterExperienceFromRegistry(ExperienceId);
}
