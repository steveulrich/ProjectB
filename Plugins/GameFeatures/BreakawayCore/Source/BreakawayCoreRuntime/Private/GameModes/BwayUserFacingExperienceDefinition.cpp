// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameModes/BwayUserFacingExperienceDefinition.h"

#include "CommonSessionSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayUserFacingExperienceDefinition)

namespace BwayHeroSelectStagingDefaults
{
	const FPrimaryAssetType MapType(TEXT("Map"));
	const FPrimaryAssetType ExperienceType(TEXT("LyraExperienceDefinition"));

	FPrimaryAssetId GetDefaultStagingMapId()
	{
		return FPrimaryAssetId(MapType, FName(TEXT("/BreakawayCore/Maps/L_BW_HeroSelect_Staging")));
	}

	FPrimaryAssetId GetDefaultStagingExperienceId()
	{
		return FPrimaryAssetId(ExperienceType, FName(TEXT("B_BW_Experience_HeroSelectStaging")));
	}

	FString ResolveMapPackagePath(FPrimaryAssetId MapAssetId)
	{
		if (!MapAssetId.IsValid())
		{
			return FString();
		}

		if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
		{
			return AssetManager->GetPrimaryAssetPath(MapAssetId).GetLongPackageName();
		}

		return FString();
	}
}

FPrimaryAssetId UBwayUserFacingExperienceDefinition::GetPrimaryAssetId() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? FPrimaryAssetId()
		: FPrimaryAssetId(FPrimaryAssetType(TEXT("LyraUserFacingExperienceDefinition")), GetFName());
}

UCommonSession_HostSessionRequest* UBwayUserFacingExperienceDefinition::CreateHostingRequest(const UObject* WorldContextObject) const
{
	// The feature's mount can become available after the initial game asset scan.
	// Resolve its maps before CommonSession validates the hosting request in -game.
	if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		AssetManager->ScanPathsForPrimaryAssets(UAssetManager::MapType,
			{TEXT("/BreakawayCore/Maps")}, UWorld::StaticClass(), false, false, true);
	}

	UCommonSession_HostSessionRequest* Result = ULyraUserFacingExperienceDefinition::CreateHostingRequest(WorldContextObject);
	if (!Result || !bRouteThroughHeroSelectStaging)
	{
		return Result;
	}

	const FPrimaryAssetId TargetMatchMapId = MapID;
	const FPrimaryAssetId TargetMatchExperienceId = ExperienceID;

	if (!TargetMatchMapId.IsValid() || !TargetMatchExperienceId.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayUserFacingExperienceDefinition: Cannot route through hero select staging without a valid match MapID and ExperienceID on %s"),
			*GetPrimaryAssetId().ToString());
		return Result;
	}

	const FPrimaryAssetId StagingMapId = HeroSelectStagingMapID.IsValid()
		? HeroSelectStagingMapID
		: BwayHeroSelectStagingDefaults::GetDefaultStagingMapId();

	const FPrimaryAssetId StagingExperienceId = HeroSelectStagingExperienceID.IsValid()
		? HeroSelectStagingExperienceID
		: BwayHeroSelectStagingDefaults::GetDefaultStagingExperienceId();

	const FString TargetMapPath = BwayHeroSelectStagingDefaults::ResolveMapPackagePath(TargetMatchMapId);
	if (TargetMapPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayUserFacingExperienceDefinition: Could not resolve match map path for %s"), *TargetMatchMapId.ToString());
		return Result;
	}

	Result->MapID = StagingMapId;
	Result->ExtraArgs.Add(TEXT("HeroSelectStaging"), TEXT("1"));
	Result->ExtraArgs.Add(TEXT("HeroSelectTargetMap"), TargetMapPath);
	Result->ExtraArgs.Add(TEXT("HeroSelectTargetExperience"), TargetMatchExperienceId.PrimaryAssetName.ToString());
	Result->ExtraArgs.Add(TEXT("Experience"), StagingExperienceId.PrimaryAssetName.ToString());

	UE_LOG(LogTemp, Log, TEXT("BwayUserFacingExperienceDefinition: Routing %s through hero select staging (%s) -> match map %s (%s)"),
		*GetPrimaryAssetId().ToString(),
		*StagingMapId.ToString(),
		*TargetMapPath,
		*TargetMatchExperienceId.PrimaryAssetName.ToString());

	return Result;
}
